// Importazione ufficiale tramite il percorso relativo corretto per la tua struttura
import * as Juce from "../../thirdparty/juce/modules/juce_gui_extra/native/javascript/index.js";

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
            else if (log.includes('SUCCESS') || log.includes('✓')) color = '#00ff00';
            else if (log.includes('⬅')) color = '#00ddff';
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

// ===== COSTANTI =====
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

// Inizializzazione sicura dell'About Menu (corretto refuso variabili)
function setupAboutMenu() {
    const modal = document.getElementById("aboutModal");
    const infoBtn = document.getElementById("infoBtn");
    const closeBtn = document.getElementById("closeBtn");

    if (!modal || !infoBtn || !closeBtn) return;

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
    };
}

// ===== FUNZIONI DI NORMALIZZAZIONE RIPRISTINATE =====
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

// ===== FUNZIONI GRAFICHE METER =====
function normalizeDb(raw) {
    const v = Number(raw);
    if (!Number.isFinite(v) || v <= SILENCE_DB)
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
    if (!el) return;

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

    if (fill) fill.style.height = dbToPercent(rmsDb) + '%';
    if (peakMarker) {
        const peakPct = dbToPercent(peakDb);
        peakMarker.style.bottom = peakPct + '%';
        peakMarker.classList.toggle('visible', peakPct > 0.5);
    }
    if (rmsLabel) rmsLabel.textContent = formatDbShort(rmsDb);
    if (peakLabel) peakLabel.textContent = formatDbShort(peakDb);
}

function setStereoMeter(fillLId, fillRId, peakLId, peakRId, rmsLId, rmsRId, peakValLId, peakValRId,
                        clipId, clipKey, rmsL, rmsR, peakL, peakR) {
    setChannel(fillLId, peakLId, rmsLId, peakValLId, rmsL, peakL);
    setChannel(fillRId, peakRId, rmsRId, peakValRId, rmsR, peakR);
    setClipLed(clipId, clipKey, isClippingDb(rmsL) || isClippingDb(rmsR) || isClippingDb(peakL) || isClippingDb(peakR));
}

function parseMeterPayload(data) {
    if (!data) return null;
    if (typeof data === 'string') {
        try {
            const fixedData = data.replace(/(:\s*[-+]?\d+),(\d+)/g, '$1.$2');
            return JSON.parse(fixedData);
        } catch (e) {
            console.error("Errore nel parsing del Meter:", e);
            return null;
        }
    }
    return data;
}

// ===== CORE LOGIC: SLIDER E PARAMETRI =====
function wireGain() {
    debugLog('wireGain: Starting...');

    const platform = navigator.platform.toLowerCase();
    const isWindows = platform.includes('win');
    debugLog(`Platform detected: ${platform} (Windows: ${isWindows})`);

    const slider = document.getElementById('gainSlider');
    const label = document.getElementById('gainValue');
    if (!slider || !label) {
        debugLog('✗ gainSlider or gainValue element not found', 'ERROR');
        return;
    }

    debugLog('✓ Slider elements found');

    let state = null;
    try {
        // Chiamata corretta tramite il modulo JUCE esportato
        state = Juce.getSliderState('gain');
    } catch (e) {
        debugLog(`✗ Error getting slider state: ${e.message}`, 'ERROR');
        return;
    }

    if (!state) {
        debugLog('✗ getSliderState returned null', 'ERROR');
        return;
    }

    debugLog('✓ Slider state obtained', 'SUCCESS');

    // Imposta correttamente i limiti reali in dB sulla barra visiva HTML
    slider.min = String(GAIN_MIN_DB);
    slider.max = String(GAIN_MAX_DB);
    slider.step = String(GAIN_STEP_DB);

    const sync = () => {
        try {
            const range = getGainRange(state);

            // Legge il valore 0-1 da JUCE e lo mappa nella scala reale dB del tuo algoritmo
            const norm = state.getNormalisedValue();
            const db = clampGainDb(normalisedToDb(norm, range));

            debugLog(`sync() from C++ - normalized: ${norm.toFixed(4)}, dB: ${db.toFixed(1)}`);

            label.textContent = db.toFixed(1) + ' dB';

            if (document.activeElement !== slider) {
                slider.value = db.toFixed(1).replace(',', '.');
                debugLog(`  Updated slider visual to: ${slider.value}`);
            }
        } catch (e) {
            debugLog(`✗ Error in sync: ${e.message}`, 'ERROR');
        }
    };

    const setGainDb = (db) => {
        try {
            const range = getGainRange(state);
            const normalised = dbToNormalised(db, range);

            debugLog(`setGainDb: User input: ${db.toFixed(1)} dB -> Normalised: ${normalised.toFixed(4)}`);

            // Invia al C++ il valore normalizzato corretto (0.0 -> 1.0)
            state.setNormalisedValue(normalised);

            label.textContent = clampGainDb(db).toFixed(1) + ' dB';
        } catch (e) {
            debugLog(`✗ Error in setGainDb: ${e.message}`, 'ERROR');
        }
    };

    try {
        state.valueChangedEvent.addListener(() => {
            debugLog('⬅ Event: valueChangedEvent fired from C++', 'SUCCESS');
            sync();
        });
        state.propertiesChangedEvent.addListener(() => {
            debugLog('⬅ Event: propertiesChangedEvent fired from C++', 'SUCCESS');
            sync();
        });
        debugLog('✓ Event listeners added to C++ state');
    } catch (e) {
        debugLog(`✗ Error adding event listeners: ${e.message}`, 'ERROR');
    }

    sync();

    // Gestione dei gesti per i blocchi di automazione DAW
    slider.addEventListener('mousedown', () => {
        try { state.sliderDragStarted(); } catch (e) { debugLog(`Error on drag start: ${e.message}`, 'ERROR'); }
    });
    slider.addEventListener('touchstart', () => {
        try { state.sliderDragStarted(); } catch (e) { debugLog(`Error on touch start: ${e.message}`, 'ERROR'); }
    }, { passive: true });
    slider.addEventListener('mouseup', () => {
        try { state.sliderDragEnded(); } catch (e) { debugLog(`Error on drag end: ${e.message}`, 'ERROR'); }
    });
    slider.addEventListener('touchend', () => {
        try { state.sliderDragEnded(); } catch (e) { debugLog(`Error on touch end: ${e.message}`, 'ERROR'); }
    });

    slider.addEventListener('input', () => {
        const cleanValue = slider.value.replace(',', '.');
        setGainDb(parseFloat(cleanValue));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        debugLog('Slider double-clicked, resetting to default (0 dB)');
        setGainDb(GAIN_DEFAULT_DB);
        sync();
    });

    debugLog('✓ wireGain completed successfully', 'SUCCESS');
}

function wireMeters() {
    debugLog('wireMeters: Starting...');
    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener) {
        debugLog('✗ Backend not available or addEventListener not found', 'ERROR');
        return;
    }

    try {
        backend.addEventListener('meterLevels', (raw) => {
            const data = parseMeterPayload(raw);
            if (!data) return;

            setStereoMeter(
                'inputFillL', 'inputFillR', 'inputPeakL', 'inputPeakR',
                'inputRmsL', 'inputRmsR', 'inputPeakValL', 'inputPeakValR',
                'inputClip', 'input', data.inL, data.inR, data.inPeakL, data.inPeakR
            );
            setStereoMeter(
                'outputFillL', 'outputFillR', 'outputPeakL', 'outputPeakR',
                'outputRmsL', 'outputRmsR', 'outputPeakValL', 'outputPeakValR',
                'outputClip', 'output', data.outL, data.outR, data.outPeakL, data.outPeakR
            );
        });
        debugLog('✓ meterLevels event listener added', 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding meterLevels listener: ${e.message}`, 'ERROR');
    }
}

function init() {
    createDebugWindow();
    setupAboutMenu();
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
    setupAboutMenu();
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

// Avvio del ciclo vitale
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        init();
        if (typeof window.__JUCE__ === 'undefined') {
            initAsync();
        }
    });
} else {
    init();
    if (typeof window.__JUCE__ === 'undefined') {
        initAsync();
    }
}