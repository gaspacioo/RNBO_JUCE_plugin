import { getSliderState } from 'juce-framework-frontend';

// ===== DEBUG WINDOW =====
const DEBUG_ENABLED = true;
const DEBUG_LOGS = [];
const MAX_LOGS = 50;

function createDebugWindow() {
    if (document.getElementById('debugWindow'))
        return;

    const debugWindow = document.createElement('div');
    debugWindow.id = 'debugWindow';
    debugWindow.style.cssText = `
        position: fixed;
        bottom: 10px;
        right: 10px;
        width: 400px;
        max-height: 300px;
        background: #1e1e1e;
        border: 2px solid #00ff00;
        border-radius: 4px;
        color: #00ff00;
        font-family: monospace;
        font-size: 11px;
        overflow-y: auto;
        z-index: 99999;
        padding: 8px;
        box-shadow: 0 0 10px rgba(0, 255, 0, 0.3);
    `;

    const closeBtn = document.createElement('button');
    closeBtn.textContent = '✕';
    closeBtn.style.cssText = `
        position: absolute;
        top: 5px;
        right: 5px;
        background: #00ff00;
        color: #1e1e1e;
        border: none;
        width: 20px;
        height: 20px;
        cursor: pointer;
        border-radius: 2px;
        font-weight: bold;
    `;
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
    if (DEBUG_LOGS.length > MAX_LOGS)
        DEBUG_LOGS.shift();

    const debugContent = document.getElementById('debugContent');
    if (debugContent) {
        debugContent.innerHTML = DEBUG_LOGS.map(log => {
            let color = '#00ff00';
            if (log.includes('ERROR')) color = '#ff0000';
            else if (log.includes('WARN')) color = '#ffff00';
            return `<div style="color: ${color};">${log}</div>`;
        }).join('');
        debugContent.parentElement.scrollTop = debugContent.parentElement.scrollHeight;
    }

    console.log(logMsg);
}

// ===== WAIT FOR JUCE =====
function waitForJUCE(maxWaitMs = 5000) {
    return new Promise((resolve) => {
        const startTime = Date.now();

        debugLog('Waiting for __JUCE__ initialization...');

        const checkJUCE = () => {
            if (typeof window.__JUCE__ !== 'undefined') {
                debugLog('✓ __JUCE__ is available', 'SUCCESS');
                resolve(true);
                return;
            }

            const elapsed = Date.now() - startTime;
            if (elapsed > maxWaitMs) {
                debugLog(`✗ Timeout waiting for __JUCE__ (${elapsed}ms)`, 'ERROR');
                resolve(false);
                return;
            }

            setTimeout(checkJUCE, 100);
        };

        checkJUCE();
    });
}

const METER_MIN_DB = -60;
const METER_MAX_DB = 0;
const CLIP_THRESHOLD_DB = -0.1;
const CLIP_HOLD_MS = 400;
const SILENCE_DB = -90;
const GAIN_MIN_DB = -60;
const GAIN_MAX_DB = 12;
const GAIN_DEFAULT_DB = 0;
const GAIN_STEP_DB = 0.1;

const clipHold = { input: null, output: null };

const modal = document.getElementById("aboutModal");
const btn = document.getElementById("infoBtn");
const closeBtn = document.getElementById("closeBtn");

function toggleAboutMenu() {
    const isModalOpen = window.getComputedStyle(modal).display === "flex";

    if (isModalOpen) {
        modal.style.display = "none";
        infoBtn.classList.remove("active"); 
    } else {
        modal.style.display = "flex";
        infoBtn.classList.add("active");
    }
}
infoBtn.onclick = toggleAboutMenu;
closeBtn.onclick = toggleAboutMenu;

window.onclick = function(event) {
    if (event.target === modal) {
        toggleAboutMenu();
    }
}

function getGainRange(state) {
    const props = state?.properties;
    if (props && props.end - props.start > 1.5)
        return { start: props.start, end: props.end, skew: props.skew ?? 1 };

    return { start: GAIN_MIN_DB, end: GAIN_MAX_DB, skew: 1 };
}

function dbToNormalised(db, range) {
    const { start, end, skew } = range;
    if (end === start)
        return 0;

    const clamped = Math.max(start, Math.min(end, db));
    return Math.pow((clamped - start) / (end - start), skew);
}

// NUOVA: Funzione inversa necessaria per ricostruire correttamente i dB reali dal valore normalizzato di JUCE
function normalisedToDb(norm, range) {
    const { start, end, skew } = range;
    if (end === start) 
        return start;

    const clampedNorm = Math.max(0, Math.min(1, norm));
    return start + (end - start) * Math.pow(clampedNorm, 1 / skew);
}

function clampGainDb(db) {
    return Math.max(GAIN_MIN_DB, Math.min(GAIN_MAX_DB, db));
}

function normalizeDb(raw) {
    const v = Number(raw);
    if (!Number.isFinite(v))
        return Number.NEGATIVE_INFINITY;
    if (v <= SILENCE_DB)
        return Number.NEGATIVE_INFINITY;
    return v;
}

function dbToPercent(db) {
    const v = normalizeDb(db);
    if (!Number.isFinite(v))
        return 0;
    const clamped = Math.max(METER_MIN_DB, Math.min(METER_MAX_DB, v));
    return ((clamped - METER_MIN_DB) / (METER_MAX_DB - METER_MIN_DB)) * 100;
}

function formatDbShort(db) {
    const v = normalizeDb(db);
    if (!Number.isFinite(v))
        return '−∞';
    return v.toFixed(1);
}

function isClippingDb(db) {
    const v = normalizeDb(db);
    return Number.isFinite(v) && v >= CLIP_THRESHOLD_DB;
}

function setClipLed(id, key, isClipping) {
    const el = document.getElementById(id);
    if (!el)
        return;

    if (isClipping) {
        el.classList.add('active');
        if (clipHold[key] != null) {
            clearTimeout(clipHold[key]);
            clipHold[key] = null;
        }
        return;
    }

    el.classList.remove('active');
    if (clipHold[key] != null)
        clearTimeout(clipHold[key]);

    clipHold[key] = setTimeout(() => {
        el.classList.remove('active');
        clipHold[key] = null;
    }, CLIP_HOLD_MS);
}

function setChannel(fillId, peakMarkerId, rmsLabelId, peakLabelId, rmsDb, peakDb) {
    const fill = document.getElementById(fillId);
    const peakMarker = document.getElementById(peakMarkerId);
    const rmsLabel = document.getElementById(rmsLabelId);
    const peakLabel = document.getElementById(peakLabelId);

    if (fill)
        fill.style.height = dbToPercent(rmsDb) + '%';

    if (peakMarker) {
        const peakPct = dbToPercent(peakDb);
        peakMarker.style.bottom = peakPct + '%';
        peakMarker.classList.toggle('visible', peakPct > 0.5);
    }

    if (rmsLabel)
        rmsLabel.textContent = formatDbShort(rmsDb);

    if (peakLabel)
        peakLabel.textContent = formatDbShort(peakDb);
}

function setStereoMeter(fillLId, fillRId, peakLId, peakRId, rmsLId, rmsRId, peakValLId, peakValRId,
                        clipId, clipKey, rmsL, rmsR, peakL, peakR) {
    setChannel(fillLId, peakLId, rmsLId, peakValLId, rmsL, peakL);
    setChannel(fillRId, peakRId, rmsRId, peakValRId, rmsR, peakR);
    setClipLed(clipId, clipKey, isClippingDb(rmsL) || isClippingDb(rmsR) || isClippingDb(peakL) || isClippingDb(peakR));
}

// AGGIORNATA: Corregge il bug del meter destro causato dalla virgola decimale nel JSON generato da Windows
function parseMeterPayload(data) {
    if (!data)
        return null;
    if (typeof data === 'string') {
        try {
            // Sostituisce le virgole con i punti nei numeri dentro la stringa JSON (es: ': 0,15' diventa ': 0.15')
            const fixedData = data.replace(/(:\s*[-+]?\d+),(\d+)/g, '$1.$2');
            return JSON.parse(fixedData);
        } catch (e) {
            console.error("Errore nel parsing del Meter:", e);
            return null;
        }
    }
    return data;
}

// AGGIORNATA: Gestisce la mappatura in dB, risolve il feedback loop e corregge la localizzazione dello slider
function wireGain() {
    debugLog('wireGain: Starting...');

    const slider = document.getElementById('gainSlider');
    const label = document.getElementById('gainValue');
    if (!slider || !label) {
        debugLog('✗ gainSlider or gainValue element not found', 'ERROR');
        return;
    }

    debugLog('✓ Slider elements found');

    let state = null;
    try {
        state = getSliderState('gain');
    } catch (e) {
        debugLog(`✗ Error getting slider state: ${e.message}`, 'ERROR');
        return;
    }

    if (!state) {
        debugLog('✗ getSliderState returned null', 'ERROR');
        return;
    }

    debugLog('✓ Slider state obtained', 'SUCCESS');

    slider.min = String(GAIN_MIN_DB);
    slider.max = String(GAIN_MAX_DB);
    slider.step = String(GAIN_STEP_DB);

    const sync = () => {
        try {
            const range = getGainRange(state);

            // Leggiamo il valore normalizzato pulito dal C++ e lo convertiamo localmente in dB reali
            const norm = state.getNormalisedValue();
            const db = clampGainDb(normalisedToDb(norm, range));

            label.textContent = db.toFixed(1) + ' dB';

            // Aggiorna lo slider grafico SOLO se l'utente non lo sta attivamente trascinando
            if (document.activeElement !== slider) {
                slider.value = db.toFixed(1).replace(',', '.');
            }
        } catch (e) {
            debugLog(`✗ Error in sync: ${e.message}`, 'ERROR');
        }
    };

    const setGainDb = (db) => {
        try {
            const range = getGainRange(state);
            // Invia il corretto valore normalizzato (0-1) calcolato in base al range reale
            debugLog(`setGainDb: Setting value to ${db.toFixed(1)} dB`);
            state.setNormalisedValue(dbToNormalised(db, range));

            // Aggiorna la label testuale istantaneamente per dare fluidità
            label.textContent = clampGainDb(db).toFixed(1) + ' dB';
        } catch (e) {
            debugLog(`✗ Error in setGainDb: ${e.message}`, 'ERROR');
        }
    };

    try {
        state.valueChangedEvent.addListener(sync);
        state.propertiesChangedEvent.addListener(sync);
        debugLog('✓ Event listeners added');
    } catch (e) {
        debugLog(`✗ Error adding event listeners: ${e.message}`, 'ERROR');
    }

    sync();

    slider.addEventListener('mousedown', () => {
        try {
            state.sliderDragStarted();
            debugLog('Slider drag started');
        } catch (e) {
            debugLog(`✗ Error in sliderDragStarted: ${e.message}`, 'ERROR');
        }
    });
    slider.addEventListener('touchstart', () => {
        try {
            state.sliderDragStarted();
            debugLog('Slider touch started');
        } catch (e) {
            debugLog(`✗ Error in sliderDragStarted (touch): ${e.message}`, 'ERROR');
        }
    }, { passive: true });
    slider.addEventListener('mouseup', () => {
        try {
            state.sliderDragEnded();
            debugLog('Slider drag ended');
        } catch (e) {
            debugLog(`✗ Error in sliderDragEnded: ${e.message}`, 'ERROR');
        }
    });
    slider.addEventListener('touchend', () => {
        try {
            state.sliderDragEnded();
            debugLog('Slider touch ended');
        } catch (e) {
            debugLog(`✗ Error in sliderDragEnded (touch): ${e.message}`, 'ERROR');
        }
    });

    slider.addEventListener('input', () => {
        // Forza la sostituzione della virgola con il punto prima di convertire in numero float
        const cleanValue = slider.value.replace(',', '.');
        debugLog(`Slider input: ${cleanValue}`);
        setGainDb(parseFloat(cleanValue));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        debugLog('Slider double-clicked, resetting to default');
        setGainDb(GAIN_DEFAULT_DB);
        sync(); // Sincronizza immediatamente la posizione dopo il reset
    });

    debugLog('✓ wireGain completed successfully', 'SUCCESS');
}

function wireMeters() {
    debugLog('wireMeters: Starting...');

    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener) {
        debugLog('✗ Backend not available or addEventListener not found', 'ERROR');
        debugLog(`  __JUCE__ available: ${typeof window.__JUCE__ !== 'undefined'}`);
        debugLog(`  backend available: ${!!backend}`);
        debugLog(`  addEventListener available: ${!!backend?.addEventListener}`);
        return;
    }

    debugLog('✓ Backend found');

    try {
        backend.addEventListener('meterLevels', (raw) => {
            const data = parseMeterPayload(raw);
            if (!data) {
                debugLog('✗ Failed to parse meter payload', 'WARN');
                return;
            }

            setStereoMeter(
                'inputFillL', 'inputFillR',
                'inputPeakL', 'inputPeakR',
                'inputRmsL', 'inputRmsR',
                'inputPeakValL', 'inputPeakValR',
                'inputClip', 'input',
                data.inL, data.inR,
                data.inPeakL, data.inPeakR
            );
            setStereoMeter(
                'outputFillL', 'outputFillR',
                'outputPeakL', 'outputPeakR',
                'outputRmsL', 'outputRmsR',
                'outputPeakValL', 'outputPeakValR',
                'outputClip', 'output',
                data.outL, data.outR,
                data.outPeakL, data.outPeakR
            );
        });
        debugLog('✓ meterLevels event listener added', 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding meterLevels listener: ${e.message}`, 'ERROR');
    }
}

function init() {
    createDebugWindow();
    debugLog('=== SimpleMeter Init Started ===', 'INFO');

    if (typeof window.__JUCE__ === 'undefined') {
        debugLog('⏳ __JUCE__ not yet available, waiting...', 'WARN');
        return;
    }

    debugLog('✓ __JUCE__ available, initializing...', 'SUCCESS');
    wireGain();
    wireMeters();
    debugLog('=== Init Complete ===', 'SUCCESS');
}

async function initAsync() {
    createDebugWindow();
    debugLog('=== SimpleMeter Init Started (Async) ===', 'INFO');

    const juceReady = await waitForJUCE(5000);

    if (!juceReady) {
        debugLog('✗ Initialization failed: __JUCE__ not available after timeout', 'ERROR');
        return;
    }

    debugLog('✓ Starting component initialization...', 'SUCCESS');
    wireGain();
    wireMeters();
    debugLog('=== Init Complete ===', 'SUCCESS');
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        // Prova prima il percorso sincrono
        init();

        // Se __JUCE__ non è disponibile, aspetta
        if (typeof window.__JUCE__ === 'undefined') {
            initAsync();
        }
    });
} else {
    // Prova prima il percorso sincrono
    init();

    // Se __JUCE__ non è disponibile, aspetta
    if (typeof window.__JUCE__ === 'undefined') {
        initAsync();
    }
}