import * as Juce from "juce-framework-frontend";

// ===== DEBUG WINDOW =====
const DEBUG_ENABLED = false;
const DEBUG_LOGS = [];
const MAX_LOGS = 50;

function createDebugWindow() {
    const win = document.getElementById('debugWindow');
    if (!win) return;
    if (DEBUG_ENABLED) win.style.display = 'block';
    win.querySelector('.debug-close-btn').onclick = () => { win.style.display = 'none'; };
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
const UI_LIMITS = {
    MIN_WIDTH: 600,
    MAX_WIDTH: 720,
    MIN_HEIGHT: 650,
    MAX_HEIGHT: 900
};

const METER_MIN_DB = -60;
const METER_MAX_DB = 0;
const CLIP_THRESHOLD_DB = -0.1;
const CLIP_HOLD_MS = 400;
const SILENCE_DB = -90;
const DELTA_SMOOTH = 0.97;  // EMA α — ~0.5s time constant at 60 Hz

const GAIN_MIN_DB = -60;
const GAIN_MAX_DB = 12;
const GAIN_DEFAULT_DB = 0;
const GAIN_STEP_DB = 0.1;

const MID_GAIN_MIN_DB = -60;
const MID_GAIN_MAX_DB = 12;
const MID_GAIN_DEFAULT_DB = 0;
const MID_GAIN_STEP_DB = 0.1;

const SIDE_GAIN_MIN_DB = -60;
const SIDE_GAIN_MAX_DB = 12;
const SIDE_GAIN_DEFAULT_DB = 0;
const SIDE_GAIN_STEP_DB = 0.1;

const PARAM_RANGE_MIN_SPAN = 1.5;

const TEMP_MIN = -10;
const TEMP_MAX = 40;
const TEMP_DEFAULT = 20;
const TEMP_STEP = 0.1;

const DIST_MIN = 0;
const DIST_MAX = 60;
const DIST_DEFAULT = 0;
const DIST_STEP = 0.1;

const TEMP_RANGE = { start: TEMP_MIN, end: TEMP_MAX, skew: 1 };
const DIST_RANGE = { start: DIST_MIN, end: DIST_MAX, skew: 1 };

const clipHold = { input: null, output: null };

let correlationValue = 0;
let _vsLastX = null;
let _vsLastY = null;
let _vsPendingBatch = null;

const VS_ZOOM_MIN  = 0.25;
const VS_ZOOM_MAX  = 8.0;
const VS_ZOOM_STEP = 1.2;
let _vsZoom          = 1.0;
let _vsShowOverlay   = true;
let _vsOverlayCanvas = null;

// ===== MENU ABOUT =====
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

// ===== RESIZE WINDOW =====
function wireWindowResize() {
    const corner = document.getElementById('resize-corner');
    if (!corner) return;

    let startX, startY, startWidth, startHeight;
    let resizeWindowNative = null;

    try {
        resizeWindowNative = Juce.getNativeFunction("resizeWindow");
    } catch (e) {
        console.error("Impossibile trovare la funzione nativa resizeWindow", e);
    }

    corner.addEventListener('mousedown', (e) => {
        e.preventDefault();
        startX = e.clientX;
        startY = e.clientY;
        startWidth = window.innerWidth;
        startHeight = window.innerHeight;

        document.addEventListener('mousemove', doResize);
        document.addEventListener('mouseup', stopResize);
    });

    function doResize(e) {
        let newWidth = startWidth + (e.clientX - startX);
        let newHeight = startHeight + (e.clientY - startY);

        newWidth = Math.max(UI_LIMITS.MIN_WIDTH, Math.min(UI_LIMITS.MAX_WIDTH, newWidth));
        newHeight = Math.max(UI_LIMITS.MIN_HEIGHT, Math.min(UI_LIMITS.MAX_HEIGHT, newHeight));

        if (resizeWindowNative) {
            resizeWindowNative(newWidth, newHeight);
        }
    }

    function stopResize() {
        document.removeEventListener('mousemove', doResize);
        document.removeEventListener('mouseup', stopResize);
    }
}

// ===== FUNZIONI DI NORMALIZZAZIONE =====
function sanitizeRange(range, fallback) {
    const start = Number(range?.start);
    const end = Number(range?.end);
    const skew = Number(range?.skew ?? 1);

    if (Number.isFinite(start) && Number.isFinite(end) && end > start) {
        return {
            start,
            end,
            skew: Number.isFinite(skew) && skew > 0 ? skew : 1,
        };
    }

    return fallback;
}

function getParamRange(state, fallback) {
    const props = state?.properties;
    const range = sanitizeRange(props, fallback);

    if (range.end - range.start > PARAM_RANGE_MIN_SPAN)
        return range;

    return fallback;
}

function getGainRange(state) {
    return getParamRange(state, { start: GAIN_MIN_DB, end: GAIN_MAX_DB, skew: 1 });
}

function clampToRange(value, range) {
    return Math.max(range.start, Math.min(range.end, value));
}

function valueToNormalised(value, range) {
    const { start, end, skew } = range;
    if (end === start)
        return 0;

    const clamped = clampToRange(value, range);
    return Math.pow((clamped - start) / (end - start), skew);
}

function normalisedToValue(norm, range) {
    const { start, end, skew } = range;
    if (end === start) 
        return start;

    const clampedNorm = Math.max(0, Math.min(1, norm));
    return start + (end - start) * Math.pow(clampedNorm, 1 / skew);
}

function dbToNormalised(db, range) {
    return valueToNormalised(db, range);
}

function normalisedToDb(norm, range) {
    return normalisedToValue(norm, range);
}

function clampGainDb(db) {
    return Math.max(GAIN_MIN_DB, Math.min(GAIN_MAX_DB, db));
}

function parseNumericInputValue(value) {
    return Number.parseFloat(String(value).replace(',', '.'));
}

function formatNumericValue(value, decimals = 1) {
    return Number.isFinite(value) ? value.toFixed(decimals) : '';
}

// ===== FUNZIONI GRAFICHE METER =====
let outStatsState = {
    maxPeakL: Number.NEGATIVE_INFINITY,
    maxPeakR: Number.NEGATIVE_INFINITY,
    maxRmsL: Number.NEGATIVE_INFINITY,
    maxRmsR: Number.NEGATIVE_INFINITY,
    maxDeltaL: Number.NEGATIVE_INFINITY, 
    maxDeltaR: Number.NEGATIVE_INFINITY,
    sumLinearPeak: 0.0,
    sumLinearRms: 0.0,
    frameCount: 0
};

const statsDbToLinear = (db) => (!Number.isFinite(db) || db <= SILENCE_DB) ? 0.0 : Math.pow(10, db / 20);
const statsLinearToDb = (linear) => (linear <= 0.00001) ? Number.NEGATIVE_INFINITY : 20 * Math.log10(linear);

function initOutputStats() {
    const resetBtn = document.getElementById('resetStatsBtn');
    if (resetBtn) {
        resetBtn.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            
            outStatsState.maxPeakL = Number.NEGATIVE_INFINITY;
            outStatsState.maxPeakR = Number.NEGATIVE_INFINITY;
            outStatsState.maxRmsL = Number.NEGATIVE_INFINITY;
            outStatsState.maxRmsR = Number.NEGATIVE_INFINITY;
            outStatsState.sumLinearPeak = 0.0;
            outStatsState.sumLinearRms = 0.0;
            outStatsState.frameCount = 0;
            outStatsState.maxDeltaL = Number.NEGATIVE_INFINITY;
            outStatsState.maxDeltaR = Number.NEGATIVE_INFINITY;
            
            updateOutputStatsUI();
            debugLog('✓ Output Stats Reset', 'SUCCESS');
        });
    }
}

function processOutputStats(outPeakL, outPeakR, outRmsL, outRmsR) {
    const pL = normalizeDb(outPeakL);
    const pR = normalizeDb(outPeakR);
    const rL = normalizeDb(outRmsL);
    const rR = normalizeDb(outRmsR);

    if (pL > outStatsState.maxPeakL) outStatsState.maxPeakL = pL;
    if (pR > outStatsState.maxPeakR) outStatsState.maxPeakR = pR;
    if (rL > outStatsState.maxRmsL) outStatsState.maxRmsL = rL;
    if (rR > outStatsState.maxRmsR) outStatsState.maxRmsR = rR;

    const currentLinearPeak = (statsDbToLinear(pL) + statsDbToLinear(pR)) / 2.0;
    const currentLinearRms  = (statsDbToLinear(rL) + statsDbToLinear(rR)) / 2.0;

    outStatsState.sumLinearPeak += currentLinearPeak;
    outStatsState.sumLinearRms  += currentLinearRms;
    outStatsState.frameCount++;

    updateOutputStatsUI();
}

function updateOutputStatsUI() {
    // 1. Formattazione classici Peak e RMS
    document.getElementById('maxOutPeakL').textContent = formatDbValue(outStatsState.maxPeakL);
    document.getElementById('maxOutPeakR').textContent = formatDbValue(outStatsState.maxPeakR);
    document.getElementById('maxOutRmsL').textContent = formatDbValue(outStatsState.maxRmsL);
    document.getElementById('maxOutRmsR').textContent = formatDbValue(outStatsState.maxRmsR);

    // 2. Calcolo e calibrazione del CREST FACTOR (Peak dB - RMS dB)
    // Se non c'è segnale (valori a -Infinity), il Crest Factor è 0
    let crestL = 0.0;
    let crestR = 0.0;
    if (outStatsState.maxPeakL > Number.NEGATIVE_INFINITY && outStatsState.maxRmsL > Number.NEGATIVE_INFINITY) {
        crestL = outStatsState.maxPeakL - outStatsState.maxRmsL;
    }
    if (outStatsState.maxPeakR > Number.NEGATIVE_INFINITY && outStatsState.maxRmsR > Number.NEGATIVE_INFINITY) {
        crestR = outStatsState.maxPeakR - outStatsState.maxRmsR;
    }
    document.getElementById('crestFactorL').textContent = crestL.toFixed(1) + " dB";
    document.getElementById('crestFactorR').textContent = crestR.toFixed(1) + " dB";

    // 3. Calcolo DELTA GAIN (Differenza tra RMS Massimo registrato e il Gain impostato)
    let deltaL = outStatsState.maxDeltaL;
    let deltaR = outStatsState.maxDeltaR;

    document.getElementById('deltaGainL').textContent =
        (!Number.isFinite(deltaL)) ? "−∞" : (deltaL > 0 ? "+" : "") + deltaL.toFixed(1) + " dB";

    document.getElementById('deltaGainR').textContent =
        (!Number.isFinite(deltaR)) ? "−∞" : (deltaR > 0 ? "+" : "") + deltaR.toFixed(1) + " dB";
    
    // 4. Formattazione Medie Storiche esistenti
    if (outStatsState.frameCount > 0) {
        let avgPeakDb = 20 * Math.log10(outStatsState.sumLinearPeak / outStatsState.frameCount);
        let avgRmsDb = 20 * Math.log10(outStatsState.sumLinearRms / outStatsState.frameCount);
        document.getElementById('avgOutPeak').textContent = formatDbValue(avgPeakDb);
        document.getElementById('avgOutRms').textContent = formatDbValue(avgRmsDb);
    } else {
        document.getElementById('avgOutPeak').textContent = "−∞";
        document.getElementById('avgOutRms').textContent = "−∞";
    }
}

function formatDbValue(val) {
    if (val === Number.NEGATIVE_INFINITY || val < -100) return "−∞";
    return val.toFixed(1) + " dB";
}

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

function updateCorrelationUI(val) {
    const bar = document.getElementById('correlationBar');
    if (!bar) return;

    // 1. Assicuriamoci che il valore sia compreso tra -1 e 1
    const clampedVal = Math.max(-1, Math.min(1, val));
    
    // 2. Calcoliamo l'intensità (da 0 a 1)
    const absVal = Math.abs(clampedVal);
    
    // 3. La larghezza massima verso un lato è il 50% del contenitore totale
    const widthPercent = absVal * 50;

    // 4. Posizionamento: se negativo va a sinistra del centro, se positivo a destra
    if (clampedVal < 0) {
        // Estensione verso sinistra: il punto di partenza si sposta indietro
        bar.style.left = (50 - widthPercent) + '%';
        bar.style.width = widthPercent + '%';
    } else {
        // Estensione verso destra: il punto di partenza è fisso al 50%
        bar.style.left = '50%';
        bar.style.width = widthPercent + '%';
    }

    // 5. Calcolo del colore dinamico
    // absVal = 0   => r: 0,   g: 255 (Verde puro al centro)
    // absVal = 1   => r: 255, g: 0   (Rosso puro agli estremi)
    const r = Math.round(absVal * 255);
    const g = Math.round((1 - absVal) * 255);
    
    bar.style.backgroundColor = `rgb(${r}, ${g}, 0)`;
}

function drawVectorscope() {
    const canvas = document.getElementById('vectorscopeCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w  = canvas.width;
    const h  = canvas.height;
    const cx = w * 0.5;
    const cy = h * 0.5;

    // Fade lento: persistenza ~2s a 60fps (fosforo CRT)
    ctx.shadowBlur = 0;
    ctx.fillStyle = 'rgba(15, 15, 15, 0.04)';
    ctx.fillRect(0, 0, w, h);

    if (_vsPendingBatch) {
        const { bx, by } = _vsPendingBatch;
        _vsPendingBatch = null;

        ctx.save();
        ctx.shadowBlur = 5;
        ctx.shadowColor = 'rgba(194, 146, 68, 0.8)';
        ctx.strokeStyle = 'rgb(240, 195, 120)';
        ctx.lineWidth = 1.2;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();

        if (_vsLastX !== null) ctx.moveTo(_vsLastX, _vsLastY);

        for (let i = 0; i < bx.length; i++) {
            const x = cx - bx[i] * cx * _vsZoom;  // negato: L→sinistra, R→destra
            const y = cy - by[i] * cy * _vsZoom;
            if (i === 0 && _vsLastX === null) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
            _vsLastX = x;
            _vsLastY = y;
        }

        ctx.stroke();
        ctx.restore();
    }

    
    if (_vsOverlayCanvas) {
        const oc   = _vsOverlayCanvas;
        const octx = oc.getContext('2d');
        octx.clearRect(0, 0, oc.width, oc.height);

        if (_vsShowOverlay) {
            drawVectorscopeOverlay(octx, oc.width, oc.height, cx, cy);

            if (_vsLastX !== null) {
                octx.save();
                octx.shadowBlur = 8;
                octx.shadowColor = 'rgba(255, 220, 100, 1)';
                octx.fillStyle = 'rgb(255, 240, 180)';
                octx.beginPath();
                octx.arc(_vsLastX, _vsLastY, 2, 0, Math.PI * 2);
                octx.fill();
                octx.restore();
            }
        }
    }

    requestAnimationFrame(drawVectorscope);
}

function drawVectorscopeOverlay(ctx, w, h, cx, cy) {
    const r  = Math.min(cx, cy) - 1;
    const d  = r * 0.7071;   // r / sqrt(2) — diagonal axis endpoints
    const lr = r * 0.65;     // label position radius
    const ld = lr * 0.7071;

    ctx.save();
    ctx.shadowBlur = 0;

    // Reference grid lines
    ctx.strokeStyle = 'rgba(194, 146, 68, 0.9)';
    ctx.lineWidth = 0.5;

    // Outer circle (unity amplitude boundary)
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.stroke();

    // M axis (vertical) and S axis (horizontal)
    ctx.beginPath();
    ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r);
    ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy);
    ctx.stroke();

    // L and R diagonal reference lines (±45°)
    ctx.beginPath();
    ctx.moveTo(cx - d, cy + d); ctx.lineTo(cx + d, cy - d); // L channel: upper-right
    ctx.moveTo(cx + d, cy + d); ctx.lineTo(cx - d, cy - d); // R channel: upper-left
    ctx.stroke();

    ctx.restore();
}

// ===== CORE LOGIC: SLIDER E PARAMETRI =====
const FADER_UNITY_POS = 0.75;

function gainDbToSliderPos(db, minDb, maxDb) {
    const clamped = Math.max(minDb, Math.min(maxDb, db));
    if (clamped <= 0)
        return FADER_UNITY_POS * (clamped - minDb) / -minDb;
    else
        return FADER_UNITY_POS + (1 - FADER_UNITY_POS) * clamped / maxDb;
}

function sliderPosToGainDb(pos, minDb, maxDb) {
    const clamped = Math.max(0, Math.min(1, pos));
    if (clamped <= FADER_UNITY_POS)
        return minDb + (clamped / FADER_UNITY_POS) * -minDb;
    else
        return (clamped - FADER_UNITY_POS) / (1 - FADER_UNITY_POS) * maxDb;
}

function wireGain({ paramId, sliderId, labelId, minDb, maxDb, stepDb, defaultDb }) {
    debugLog('wireGain: Starting...');

    const platform = navigator.platform.toLowerCase();
    const isWindows = platform.includes('win');
    debugLog(`Platform detected: ${platform} (Windows: ${isWindows})`);

    const slider = document.getElementById(sliderId);
    const label = document.getElementById(labelId);
    if (!slider || !label) {
        debugLog(`✗ ${sliderId} or ${labelId} element not found`, 'ERROR');
        return;
    }

    debugLog('✓ Slider elements found');

    let state = null;
    try {
        state = Juce.getSliderState(paramId);
    } catch (e) {
        debugLog(`✗ Error getting slider state for ${paramId}: ${e.message}`, 'ERROR');
        return;
    }

    if (!state) {
        debugLog('✗ getSliderState returned null', 'ERROR');
        return;
    }

    debugLog('✓ Slider state obtained', 'SUCCESS');
    
    const fallbackRange = { start: minDb, end: maxDb, skew: 1 };
    slider.min = "0";
    slider.max = "1";
    slider.step = "any";

    const sync = () => {
        try {
            const range = getParamRange(state, fallbackRange);

            // Legge il valore 0-1 da JUCE e lo mappa nella scala reale dB del tuo algoritmo
            const norm = state.getNormalisedValue();
            const db = Math.max(minDb, Math.min(maxDb, normalisedToDb(norm, range)));

            debugLog(`sync() from C++ - normalized: ${norm.toFixed(4)}, dB: ${db.toFixed(1)}`);

            label.textContent = db.toFixed(1) + ' dB';

            if (document.activeElement !== slider) {
                slider.value = gainDbToSliderPos(db, minDb, maxDb).toFixed(4);
                debugLog(`  Updated slider visual to: ${slider.value}`);
            }
        } catch (e) {
            debugLog(`✗ Error in sync: ${e.message}`, 'ERROR');
        }
    };

    const setGainDb = (db) => {
        try {
            const range = getParamRange(state, fallbackRange);
            const normalised = dbToNormalised(db, range);

            debugLog(`setGainDb: User input: ${db.toFixed(1)} dB -> Normalised: ${normalised.toFixed(4)}`);

            state.setNormalisedValue(normalised);

            label.textContent = Math.max(range.start, Math.min(range.end, db)).toFixed(1) + ' dB';
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

    // Delay iniziale per dare tempo al backend di stabilizzarsi
    setTimeout(() => {
        debugLog(`Performing delayed sync for ${paramId}...`);
        sync();
    }, 50);

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
        const pos = parseFloat(slider.value.replace(',', '.'));
        setGainDb(sliderPosToGainDb(pos, minDb, maxDb));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        debugLog('Slider double-clicked, resetting to default (0 dB)');
        setGainDb(defaultDb);
        slider.blur();
        sync();
    });

    debugLog('✓ wireGain completed successfully', 'SUCCESS');
}

function wireNumericParameter({ paramId, inputId, fallbackRange, step, defaultValue, decimals = 1 }) {
    debugLog(`wireNumericParameter(${paramId}): Starting...`);

    const input = document.getElementById(inputId);
    if (!input) {
        debugLog(`✗ ${inputId} element not found`, 'ERROR');
        return;
    }

    let state = null;
    try {
        state = Juce.getSliderState(paramId);
    } catch (e) {
        debugLog(`✗ Error getting ${paramId} state: ${e.message}`, 'ERROR');
        return;
    }

    if (!state) {
        debugLog(`✗ getSliderState returned null for ${paramId}`, 'ERROR');
        return;
    }

    const updateInputProperties = () => {
        const range = getParamRange(state, fallbackRange);
        input.min = formatNumericValue(range.start, decimals);
        input.max = formatNumericValue(range.end, decimals);
        input.step = String(step);
    };

    const sync = () => {
        try {
            const range = getParamRange(state, fallbackRange);
            const value = clampToRange(normalisedToValue(state.getNormalisedValue(), range), range);

            updateInputProperties();
            input.classList.remove('invalid');

            if (document.activeElement !== input)
                input.value = formatNumericValue(value, decimals);
        } catch (e) {
            debugLog(`✗ Error syncing ${paramId}: ${e.message}`, 'ERROR');
        }
    };

    const setParameterValue = (value, updateInput = false) => {
        if (!Number.isFinite(value)) {
            input.classList.add('invalid');
            return;
        }

        try {
            const range = getParamRange(state, fallbackRange);
            const clamped = clampToRange(value, range);
            state.setNormalisedValue(valueToNormalised(clamped, range));

            input.classList.remove('invalid');
            if (updateInput)
                input.value = formatNumericValue(clamped, decimals);
        } catch (e) {
            debugLog(`✗ Error setting ${paramId}: ${e.message}`, 'ERROR');
        }
    };

    const commitInput = () => {
        const value = parseNumericInputValue(input.value);

        if (!Number.isFinite(value)) {
            sync();
            return;
        }

        setParameterValue(value, true);
    };

    let editGestureActive = false;
    const beginEditGesture = () => {
        if (editGestureActive)
            return;

        editGestureActive = true;
        try { state.sliderDragStarted(); } catch (e) { debugLog(`Error starting ${paramId} edit: ${e.message}`, 'ERROR'); }
    };

    const endEditGesture = () => {
        commitInput();

        if (!editGestureActive)
            return;

        editGestureActive = false;
        try { state.sliderDragEnded(); } catch (e) { debugLog(`Error ending ${paramId} edit: ${e.message}`, 'ERROR'); }
    };

    try {
        state.valueChangedEvent.addListener(sync);
        state.propertiesChangedEvent.addListener(sync);
        debugLog(`✓ Event listeners added for ${paramId}`, 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding ${paramId} listeners: ${e.message}`, 'ERROR');
    }

    updateInputProperties();
    sync();

    input.addEventListener('focus', beginEditGesture);
    input.addEventListener('pointerdown', beginEditGesture);

    input.addEventListener('input', () => {
        beginEditGesture();
        setParameterValue(parseNumericInputValue(input.value));
    });

    input.addEventListener('change', commitInput);
    input.addEventListener('blur', endEditGesture);

    input.addEventListener('keydown', (event) => {
        if (event.key === 'Enter') {
            event.preventDefault();
            commitInput();
            input.blur();
        } else if (event.key === 'Escape') {
            event.preventDefault();
            sync();
            input.blur();
        }
    });

    input.addEventListener('dblclick', (event) => {
        event.preventDefault();
        beginEditGesture();
        setParameterValue(defaultValue, true);
        endEditGesture();
    });

    debugLog(`✓ wireNumericParameter(${paramId}) completed successfully`, 'SUCCESS');
}

function wirePhaseControls() {
    wireNumericParameter({
        paramId: 'temperature',
        inputId: 'temperatureInput',
        fallbackRange: TEMP_RANGE,
        step: TEMP_STEP,
        defaultValue: TEMP_DEFAULT,
    });

    wireNumericParameter({
        paramId: 'distance',
        inputId: 'distanceInput',
        fallbackRange: DIST_RANGE,
        step: DIST_STEP,
        defaultValue: DIST_DEFAULT,
    });

    wireToggleParameter({
        paramId: 'phase_inv',
        buttonId: 'phaseToggleBtn',
        activeClass: 'active'
    });
}

function setDelayTime(delayValue, sampleRate) {
    const delayLabel = document.getElementById('delayTimeValue');
    if (!delayLabel) return;

    let text = formatNumericValue(delayValue, 1) + ' ms';
    if (Number.isFinite(sampleRate) && sampleRate > 0) {
        const samples = (delayValue * sampleRate) / 1000;
        text += ` (${Math.round(samples)} sa)`;
    }
    delayLabel.textContent = text;
}

function wireToggleParameter({ paramId, buttonId, activeClass = 'active' }) {
    debugLog(`wireToggleParameter(${paramId}): Starting via getToggleState...`);

    const button = document.getElementById(buttonId);
    if (!button) {
        debugLog(`✗ ${buttonId} element not found`, 'ERROR');
        return;
    }

    let state = null;
    try {
        // Utilizziamo il metodo nativo del framework JUCE 8 / RNBO Template
        state = Juce.getToggleState(paramId);
    } catch (e) {
        debugLog(`✗ Error getting ${paramId} toggle state: ${e.message}`, 'ERROR');
        return;
    }

    if (!state) {
        debugLog(`✗ getToggleState returned null for ${paramId}`, 'ERROR');
        return;
    }

    // Sincronizza lo stato del bottone HTML con il valore booleano del backend
    const sync = () => {
        try {
            // getToggleState espone direttamente un valore booleano (true/false)
            const isActive = state.getValue();

            debugLog(`sync() ${paramId} - isActive: ${isActive}`);

            if (isActive) {
                button.classList.add(activeClass);
                button.setAttribute('aria-checked', 'true');
            } else {
                button.classList.remove(activeClass);
                button.setAttribute('aria-checked', 'false');
            }
        } catch (e) {
            debugLog(`✗ Error syncing ${paramId}: ${e.message}`, 'ERROR');
        }
    };

    // Inverte lo stato booleano al click
    const toggle = () => {
        try {
            const currentState = state.getValue();
            const newValue = !currentState;

            // Invia il nuovo stato booleano al backend C++
            state.setValue(newValue);

            debugLog(`toggle ${paramId}: Sent value ${newValue}`);
        } catch (e) {
            debugLog(`✗ Error toggling ${paramId}: ${e.message}`, 'ERROR');
        }
    };

    try {
        // Ascolta i cambiamenti provenienti dalla DAW o dal backend
        state.valueChangedEvent.addListener(sync);
        state.propertiesChangedEvent.addListener(sync);
        debugLog(`✓ Event listeners added for toggle ${paramId}`, 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding ${paramId} toggle listeners: ${e.message}`, 'ERROR');
    }

    // Esegui la sincronizzazione iniziale con un piccolo delay per dare tempo al backend
    setTimeout(() => {
        debugLog(`Performing delayed sync for ${paramId}...`);
        sync();
    }, 50);

    // Incolla l'evento click del mouse
    button.addEventListener('click', toggle);

    debugLog(`✓ wireToggleParameter(${paramId}) completed successfully`, 'SUCCESS');
}

function wireMSMatrix() {
    debugLog("wireMSMatrix(): Inizializzazione controlli Mid/Side...", "INFO");

    // --- CANALE MID ---
    wireGain({
        paramId: 'mid_gain',
        sliderId: 'midGainSlider', // Assicurati che l'ID nell'HTML sia un input range
        labelId: 'midGainValue',   // L'elemento span/div affiancato per il testo dei dB
        minDb: MID_GAIN_MIN_DB,
        maxDb: MID_GAIN_MAX_DB,
        stepDb: MID_GAIN_STEP_DB,
        defaultDb: MID_GAIN_DEFAULT_DB
    });

    wireToggleParameter({
        paramId: 'mid_mute',
        buttonId: 'midMuteBtn',
        activeClass: 'muted'
    });

    // --- CANALE SIDE ---
    wireGain({
        paramId: 'side_gain',
        sliderId: 'sideGainSlider',
        labelId: 'sideGainValue',
    	minDb: SIDE_GAIN_MIN_DB,
        maxDb: SIDE_GAIN_MAX_DB,
        stepDb: SIDE_GAIN_STEP_DB,
        defaultDb: SIDE_GAIN_DEFAULT_DB
    });

    wireToggleParameter({
        paramId: 'side_mute',
        buttonId: 'sideMuteBtn',
        activeClass: 'muted'
    });
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
            setDelayTime(data.delayTime, data.sampleRate);

            if (data.outL !== undefined && data.inL !== undefined) {
                const inLdb  = normalizeDb(data.inL);
                const inRdb  = normalizeDb(data.inR);
                const outLdb = normalizeDb(data.outL);
                const outRdb = normalizeDb(data.outR);
                if (Number.isFinite(inLdb) && Number.isFinite(outLdb)) {
                    const rawL = outLdb - inLdb;
                    outStatsState.maxDeltaL = Number.isFinite(outStatsState.maxDeltaL)
                        ? DELTA_SMOOTH * outStatsState.maxDeltaL + (1 - DELTA_SMOOTH) * rawL
                        : rawL;
                }
                if (Number.isFinite(inRdb) && Number.isFinite(outRdb)) {
                    const rawR = outRdb - inRdb;
                    outStatsState.maxDeltaR = Number.isFinite(outStatsState.maxDeltaR)
                        ? DELTA_SMOOTH * outStatsState.maxDeltaR + (1 - DELTA_SMOOTH) * rawR
                        : rawR;
                }
            }
            processOutputStats(data.outPeakL, data.outPeakR, data.outL, data.outR);
            updateOutputStatsUI();

            if (data.correlationValue !== undefined) {
                updateCorrelationUI(data.correlationValue);
            }
            
            if (data.scopeBatchX && data.scopeBatchX.length > 0)
                _vsPendingBatch = { bx: data.scopeBatchX, by: data.scopeBatchY };
        });
        debugLog('✓ meterLevels event listener added', 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding meterLevels listener: ${e.message}`, 'ERROR');
    }
    setupVectorscopeZoom();
    drawVectorscope();
}

function changeVsZoom(direction) {
    _vsZoom = direction > 0
        ? Math.min(VS_ZOOM_MAX, _vsZoom * VS_ZOOM_STEP)
        : Math.max(VS_ZOOM_MIN, _vsZoom / VS_ZOOM_STEP);

    _vsLastX = null;
    _vsLastY = null;
}

function setupVectorscopeZoom() {
    _vsOverlayCanvas = document.getElementById('vectorscopeOverlay');

    document.getElementById('vsZoomIn').addEventListener('click', () => changeVsZoom(1));
    document.getElementById('vsZoomOut').addEventListener('click', () => changeVsZoom(-1));

    const btnOverlay = document.getElementById('vsOverlayToggle');
    btnOverlay.addEventListener('click', () => {
        _vsShowOverlay = !_vsShowOverlay;
        btnOverlay.style.opacity = _vsShowOverlay ? '1' : '0.35';
    });
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
    wireGain({
        paramId: 'gain',
        sliderId: 'gainSlider',
        labelId: 'gainValue',
        minDb: GAIN_MIN_DB,
        maxDb: GAIN_MAX_DB,
        stepDb: GAIN_STEP_DB,
        defaultDb: GAIN_DEFAULT_DB
    });
    wirePhaseControls();
    wireMSMatrix();
    wireMeters();
    initOutputStats();

    wireWindowResize();

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
    wireGain({
        paramId: 'gain',
        sliderId: 'gainSlider',
        labelId: 'gainValue',
        minDb: GAIN_MIN_DB,
        maxDb: GAIN_MAX_DB,
        stepDb: GAIN_STEP_DB,
        defaultDb: GAIN_DEFAULT_DB
    });
    wirePhaseControls();
    wireMSMatrix();
    wireMeters();
    initOutputStats();

    wireWindowResize();

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
