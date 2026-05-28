// Importazione con il percorso relativo corretto da src/webui/
import * as Juce from "../../thirdparty/juce/modules/juce_gui_extra/native/javascript/index.js";

// ===== DEBUG WINDOW =====
const DEBUG_ENABLED = true;
const DEBUG_LOGS = [];
const MAX_LOGS = 50;

function createDebugWindow() {
    if (document.getElementById('debugWindow')) return;
    const debugWindow = document.createElement('div');
    debugWindow.id = 'debugWindow';
    debugWindow.style.cssText = `position: fixed; bottom: 10px; right: 10px; width: 400px; max-height: 300px; background: #1e1e1e; border: 2px solid #00ff00; border-radius: 4px; color: #00ff00; font-family: monospace; font-size: 11px; overflow-y: auto; z-index: 99999; padding: 8px; box-shadow: 0 0 10px rgba(0, 255, 0, 0.3);`;
    const closeBtn = document.createElement('button');
    closeBtn.textContent = '✕';
    closeBtn.style.cssText = `position: absolute; top: 5px; right: 5px; background: #00ff00; color: #1e1e1e; border: none; width: 20px; height: 20px; cursor: pointer; border-radius: 2px; font-weight: bold;`;
    closeBtn.onclick = () => debugWindow.style.display = 'none';
    debugWindow.appendChild(closeBtn);
    const content = document.createElement('div');
    content.id = 'debugContent';
    content.style.marginTop = '20px';
    debugWindow.appendChild(content);
    document.body.appendChild(debugWindow);
}

function debugLog(msg, level = 'INFO') {
    if (!DEBUG_ENABLED) return;
    const timestamp = new Date().toLocaleTimeString();
    const logMsg = `[${timestamp}] ${level}: ${msg}`;
    DEBUG_LOGS.push(logMsg);
    if (DEBUG_LOGS.length > MAX_LOGS) DEBUG_LOGS.shift();
    const debugContent = document.getElementById('debugContent');
    if (debugContent) {
        debugContent.innerHTML = DEBUG_LOGS.map(log => {
            let color = '#00ff00';
            if (log.includes('ERROR')) color = '#ff0000';
            else if (log.includes('WARN')) color = '#ffff00';
            else if (log.includes('SUCCESS') || log.includes('✓')) color = '#00ff00';
            return `<div style="color: ${color};">${log}</div>`;
        }).join('');
        debugContent.parentElement.scrollTop = debugContent.parentElement.scrollHeight;
    }
    console.log(logMsg);
}

// ===== COSTANTI E FUNZIONI MATEMATICHE =====
const METER_MIN_DB = -60;
const METER_MAX_DB = 0;
const CLIP_THRESHOLD_DB = -0.1;
const CLIP_HOLD_MS = 400;
const SILENCE_DB = -90;
const GAIN_MIN_DB = -60;
const GAIN_MAX_DB = 12;

const clipHold = { input: null, output: null };

function getGainRange(state) {
    const props = state?.properties;
    if (props && props.end - props.start > 1.5)
        return { start: props.start, end: props.end, skew: props.skew ?? 1 };
    return { start: GAIN_MIN_DB, end: GAIN_MAX_DB, skew: 1 };
}

function dbToNormalised(db, range) {
    const { start, end, skew } = range;
    if (end === start) return 0;
    const clamped = Math.max(start, Math.min(end, db));
    return Math.pow((clamped - start) / (end - start), skew);
}

function normalisedToDb(norm, range) {
    const { start, end, skew } = range;
    if (end === start) return start;
    const clampedNorm = Math.max(0, Math.min(1, norm));
    return start + (end - start) * Math.pow(clampedNorm, 1 / skew);
}

// ===== FUNZIONI GRAFICHE METER =====
function normalizeDb(raw) {
    const v = Number(raw);
    return (!Number.isFinite(v) || v <= SILENCE_DB) ? Number.NEGATIVE_INFINITY : v;
}

function dbToPercent(db) {
    const v = normalizeDb(db);
    if (!Number.isFinite(v)) return 0;
    const clamped = Math.max(METER_MIN_DB, Math.min(METER_MAX_DB, v));
    return ((clamped - METER_MIN_DB) / (METER_MAX_DB - METER_MIN_DB)) * 100;
}

function formatDbShort(db) {
    const v = normalizeDb(db);
    return !Number.isFinite(v) ? '−∞' : v.toFixed(1);
}

function setClipLed(id, key, isClipping) {
    const el = document.getElementById(id);
    if (!el) return;
    if (isClipping) {
        el.classList.add('active');
        if (clipHold[key] != null) { clearTimeout(clipHold[key]); clipHold[key] = null; }
        return;
    }
    el.classList.remove('active');
    if (clipHold[key] != null) clearTimeout(clipHold[key]);
    clipHold[key] = setTimeout(() => {
        el.classList.remove('active');
        clipHold[key] = null;
    }, CLIP_HOLD_MS);
}

// ===== INIZIALIZZAZIONE UNICA AL CARICAMENTO DEL DOM =====
document.addEventListener("DOMContentLoaded", () => {
    createDebugWindow();
    debugLog('✓ DOM caricato, avvio integrazione nativa JUCE 8...', 'SUCCESS');

    // --- SETUP INTERFACCIA ABOUT MENU ---
    const modal = document.getElementById("aboutModal");
    const infoBtn = document.getElementById("infoBtn");
    const closeBtn = document.getElementById("closeBtn");
    const toggleAboutMenu = () => {
        const isModalOpen = window.getComputedStyle(modal).display === "flex";
        modal.style.display = isModalOpen ? "none" : "flex";
        if(isModalOpen) infoBtn.classList.remove("active"); else infoBtn.classList.add("active");
    };
    if (infoBtn) infoBtn.onclick = toggleAboutMenu;
    if (closeBtn) closeBtn.onclick = toggleAboutMenu;
    window.onclick = (event) => { if (event.target === modal) toggleAboutMenu(); };

    // --- SETUP DELLO SLIDER DI GAIN CON JUCE 8 ---
    const slider = document.getElementById("gainSlider");
    const label = document.getElementById("gainValue");
    
    // Agganciamo lo stato nativo del parametro C++
    const gainState = Juce.getSliderState("gain"); 

    if (slider && label && gainState) {
        debugLog('✓ Slider collegato allo stato nativo JUCE', 'SUCCESS');

        // Forza i limiti geometrici del cursore HTML tra 0 e 1 (Normalizzato)
        slider.min = "0";
        slider.max = "1";
        if (gainState.properties && gainState.properties.numSteps) {
            slider.step = (1 / gainState.properties.numSteps).toString();
        } else {
            slider.step = "0.001";
        }

        // 1. Dal C++ alla UI (Aggiornamento da DAW o valore iniziale)
        gainState.valueChangedEvent.addListener(() => {
            const norm = gainState.getNormalisedValue();
            
            // Muove lo slider usando solo il valore puro 0.0 -> 1.0
            if (document.activeElement !== slider) {
                slider.value = norm;
            }

            // Converte il valore in dB SOLO per la stringa di testo
            const range = getGainRange(gainState);
            const dbValue = normalisedToDb(norm, range);
            label.textContent = dbValue.toFixed(1) + ' dB';
        });

        // 2. Dalla UI al C++ (Quando muovi il cursore)
        slider.addEventListener('input', (e) => {
            const norm = parseFloat(e.target.value);
            
            // Invia a JUCE il valore normalizzato (0-1) senza calcoli JS intermedi
            gainState.setNormalisedValue(norm);
            
            // Mostra i dB aggiornati a schermo
            const range = getGainRange(gainState);
            const dbValue = normalisedToDb(norm, range);
            label.textContent = dbValue.toFixed(1) + ' dB';
        });

        // 3. Gestione drag gesti per automazioni DAW
        slider.addEventListener('mousedown', () => gainState.sliderDragStarted());
        slider.addEventListener('mouseup', () => gainState.sliderDragEnded());
        slider.addEventListener('touchstart', () => gainState.sliderDragStarted(), { passive: true });
        slider.addEventListener('touchend', () => gainState.sliderDragEnded());
        
        // 4. Doppio click per reset a 0 dB
        slider.addEventListener('dblclick', (e) => {
            e.preventDefault();
            const range = getGainRange(gainState);
            // Calcola quale valore tra 0 e 1 corrisponde a 0 dB nel tuo range
            const defaultNorm = dbToNormalised(0, range);
            gainState.setNormalisedValue(defaultNorm);
        });
    } else {
        debugLog('✗ Impossibile trovare gainSlider o agganciare getSliderState', 'ERROR');
    }

    // --- SETUP DEI METER AUDIO ---
    if (window.__JUCE__ && window.__JUCE__.backend) {
        debugLog('✓ Backend JUCE pronto, inizializzazione ascoltatore dei Meter...', 'INFO');
        
        window.__JUCE__.backend.addEventListener('meterLevels', (raw) => {
            let data = null;
            if (typeof raw === 'string') {
                try {
                    const fixedData = raw.replace(/(:\s*[-+]?\d+),(\d+)/g, '$1.$2');
                    data = JSON.parse(fixedData);
                } catch (e) {
                    return;
                }
            } else {
                data = raw;
            }

            if (data) {
                // I tuoi meter usano ancora la logica Db nativa inviata via JSON personalizzato
                const fillL = document.getElementById('inputFillL');
                const fillR = document.getElementById('inputFillR');
                if (fillL) fillL.style.height = dbToPercent(data.inL) + '%';
                if (fillR) fillR.style.height = dbToPercent(data.inR) + '%';
                
                const outFillL = document.getElementById('outputFillL');
                const outFillR = document.getElementById('outputFillR');
                if (outFillL) outFillL.style.height = dbToPercent(data.outL) + '%';
                if (outFillR) outFillR.style.height = dbToPercent(data.outR) + '%';
                
                const labelInL = document.getElementById('inputRmsL');
                const labelInR = document.getElementById('inputRmsR');
                if (labelInL) labelInL.textContent = formatDbShort(data.inL);
                if (labelInR) labelInR.textContent = formatDbShort(data.inR);

                const labelOutL = document.getElementById('outputRmsL');
                const labelOutR = document.getElementById('outputRmsR');
                if (labelOutL) labelOutL.textContent = formatDbShort(data.outL);
                if (labelOutR) labelOutR.textContent = formatDbShort(data.outR);
            }
        });
    }
});