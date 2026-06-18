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
    MIN_WIDTH: 870,
    MAX_WIDTH: 1500,
    MIN_HEIGHT: 360,
    MAX_HEIGHT: 900
};

const METER_MIN_DB = -60;
const METER_MAX_DB = 0;
const CLIP_THRESHOLD_DB = -0.1;
const CLIP_HOLD_MS = 400;
const SILENCE_DB = -90;
const PEAK_HOLD_MS = 2000;    // ms di hold prima del decadimento
const PEAK_DECAY_DB_S = 8;    // dB/s di discesa dopo lo scadere del hold

const _meterPeak = {
    inL:  { db: -Infinity, heldAt: 0 },
    inR:  { db: -Infinity, heldAt: 0 },
    outL: { db: -Infinity, heldAt: 0 },
    outR: { db: -Infinity, heldAt: 0 },
};
const DELTA_SMOOTH = 0.97;  // EMA α — ~0.5s time constant at 60 Hz

const GAIN_MIN_DB = -12;
const GAIN_MAX_DB = 12;
const GAIN_DEFAULT_DB = 0;
const GAIN_STEP_DB = 0.1;

const MID_GAIN_MIN_DB = -12;
const MID_GAIN_MAX_DB = 12;
const MID_GAIN_DEFAULT_DB = 0;
const MID_GAIN_STEP_DB = 0.1;

const SIDE_GAIN_MIN_DB = -12;
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
let _vsPendingBatch = null;

const VS_ZOOM_MIN  = 0.25;
const VS_ZOOM_MAX  = 8.0;
const VS_ZOOM_STEP = 1.2;
let _vsZoom          = 1.0;
let _vsShowOverlay   = true;
let _vsOverlayCanvas = null;
let _vsDotsMode      = true;   // true: nuvola di puntini, false: linea continua + cursore
let _vsLastX = null;           // continuità della traccia in modalità linea
let _vsLastY = null;

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

// Evita "-0.0": valori che toFixed(1) arrotonderebbe a "0.0" vengono forzati a +0
function snapZero(db) {
    return Math.abs(db) < 0.05 ? 0 : db;
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
            
            resetSpectrum();
            resetVectorscope();

            // L'Integrated LUFS si azzera solo qui (lato C++ via native function)
            if (_resetLufsNative) { try { _resetLufsNative(); } catch (err) {} }
            _lufsLatest[2] = -Infinity;
            updateLufsUI();

            updateOutputStatsUI();
            debugLog('✓ Output Stats Reset', 'SUCCESS');
        });
    }
}

// ===== LUFS (Momentary / Short-term / Integrated) =====
const _LUFS_MODES = ['M', 'S', 'I'];
let _lufsMode = 0;                              // 0=M, 1=S, 2=I
let _lufsLatest = [-Infinity, -Infinity, -Infinity];
let _resetLufsNative = null;

function setupLufs() {
    try { _resetLufsNative = Juce.getNativeFunction('resetLufs'); }
    catch (e) { _resetLufsNative = null; }

    const box = document.getElementById('lufsBox');
    if (box) box.addEventListener('click', () => {
        _lufsMode = (_lufsMode + 1) % _LUFS_MODES.length;
        updateLufsUI();
    });
    updateLufsUI();
}

function updateLufsUI(data) {
    if (data) {
        if (data.lufsM !== undefined) _lufsLatest[0] = data.lufsM;
        if (data.lufsS !== undefined) _lufsLatest[1] = data.lufsS;
        if (data.lufsI !== undefined) _lufsLatest[2] = data.lufsI;
    }
    const label = document.getElementById('lufsLabel');
    const val   = document.getElementById('lufsValue');
    if (label) label.textContent = 'LUFS ' + _LUFS_MODES[_lufsMode];
    if (val) {
        const v = _lufsLatest[_lufsMode];
        // -100 è il sentinella "nessuna misura"; sotto -70 (gate) mostro −∞
        val.textContent = (!Number.isFinite(v) || v <= -99) ? '−∞' : v.toFixed(1);
    }
}

// Azzera le barre dello spettro (livelli, peak hold e media a lungo termine)
function resetSpectrum() {
    _specTargetL.fill(SPEC_FLOOR_DB - 1);
    _specTargetR.fill(SPEC_FLOOR_DB - 1);
    _specDrawL.fill(SPEC_FLOOR_DB - 1);
    _specDrawR.fill(SPEC_FLOOR_DB - 1);
    _specPeakL.fill(SPEC_FLOOR_DB - 1);
    _specPeakR.fill(SPEC_FLOOR_DB - 1);
    _specPeakAgeL.fill(0);
    _specPeakAgeR.fill(0);
    _specTargetMid.fill(SPEC_FLOOR_DB - 1);
    _specTargetSide.fill(SPEC_FLOOR_DB - 1);
    _specDrawMid.fill(SPEC_FLOOR_DB - 1);
    _specDrawSide.fill(SPEC_FLOOR_DB - 1);
    _specPeakMid.fill(SPEC_FLOOR_DB - 1);
    _specPeakAgeMid.fill(0);
    _specAvgL.fill(0);
    _specAvgR.fill(0);
    _specAvgMid.fill(0);
    _specAvgSide.fill(0);
    _specAvgCount = 0;
}

// Cancella la persistenza (puntini fosforo) del vectorscope
function resetVectorscope() {
    _vsPendingBatch = null;
    _vsLastX = null;
    _vsLastY = null;
    const canvas = document.getElementById('vectorscopeCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = _vsW || canvas.width;
    const h = _vsH || canvas.height;
    ctx.save();
    ctx.setTransform(_dpr, 0, 0, _dpr, 0, 0);
    ctx.clearRect(0, 0, w, h);
    ctx.restore();
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

function getHeldPeak(key, incoming) {
    const now = performance.now();
    const p = _meterPeak[key];
    const db = normalizeDb(incoming);

    // Valore attualmente visualizzato, tenendo conto dell'eventuale decadimento in corso.
    let current = -Infinity;
    if (Number.isFinite(p.db)) {
        const elapsed = now - p.heldAt;
        current = elapsed <= PEAK_HOLD_MS
            ? p.db
            : p.db - (elapsed - PEAK_HOLD_MS) / 1000 * PEAK_DECAY_DB_S;
    }

    // Se il nuovo livello supera quello mostrato (anche se in discesa), riaggancia e
    // riparte l'hold. Confrontiamo col valore corrente decaduto, non con l'originale.
    if (Number.isFinite(db) && db > current) {
        p.db = db;
        p.heldAt = now;
        return db;
    }

    return current;
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

    if (fill) fill.style.height = (100 - dbToPercent(rmsDb)) + '%';
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

// ---- Segmented LED meter (shared by balance and correlation) ----
const _SEG_N      = 31;   // dispari: il segmento centrale corrisponde esattamente allo zero
const _SEG_GAP    = 1.5;
const _SEG_MIX    = (c1, c2, t) => c1.map((v, i) => Math.round(v + (c2[i] - v) * t));
const _SEG_WHITE  = [255, 255, 255];
const _SEG_ORANGE = [230, 160, 80];
const _SEG_GREEN  = [70, 210, 90];
const _SEG_RED    = [225, 60, 60];

function _segCorrColor(segVal) {
    const absV = Math.abs(segVal);
    if (absV <= 0.5) return _SEG_MIX(_SEG_WHITE, _SEG_ORANGE, absV * 2);
    const target = segVal >= 0 ? _SEG_GREEN : _SEG_RED;
    return _SEG_MIX(_SEG_ORANGE, target, Math.min(1, (absV - 0.5) / 0.45));
}

function _drawSegmentMeter(canvasId, value, mode) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    const dpr  = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const logW = Math.round(rect.width)  || 156;
    const logH = Math.round(rect.height) || 10;
    if (canvas.width !== Math.round(logW * dpr) || canvas.height !== Math.round(logH * dpr)) {
        canvas.width  = Math.round(logW * dpr);
        canvas.height = Math.round(logH * dpr);
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, logW, logH);

    const n    = _SEG_N;
    const segW = (logW - _SEG_GAP * (n - 1)) / n;
    const clamped = Math.max(-1, Math.min(1, value));
    // posizione frazionaria del valore sulla scala dei segmenti
    const posF = (clamped + 1) / 2 * (n - 1);

    for (let i = 0; i < n; i++) {
        const x      = i * (segW + _SEG_GAP);
        const segVal = (i / (n - 1)) * 2 - 1;
        let r, g, b, a;

        if (mode === 'balance') {
            // Indicatore bianco che scorre: il segmento più vicino è acceso,
            // quello adiacente si accende in proporzione alla parte frazionaria
            [r, g, b] = _SEG_WHITE;
            const lit = Math.max(0, 1 - Math.abs(i - posF));
            a = 0.10 + lit * 0.85;
        } else {
            // Correlazione: si accendono i segmenti dal centro (0) fino al valore,
            // gli altri restano visibili in trasparenza col loro colore
            [r, g, b] = _segCorrColor(segVal);
            const lit = clamped >= 0
                ? (segVal >= -1e-9 && segVal <= clamped + 1e-9)
                : (segVal <=  1e-9 && segVal >= clamped - 1e-9);
            a = lit ? 1.0 : 0.22;
        }

        ctx.fillStyle = `rgba(${r},${g},${b},${a})`;
        ctx.fillRect(x, 0, segW, logH);
    }
}

function updateCorrelationUI(val) {
    _drawSegmentMeter('correlationCanvas', val, 'correlation');
}

// Bilanciamento energetico L/R: -1 = tutto a sinistra, +1 = tutto a destra
let _balanceSmooth = 0;

function updateBalanceUI(lDbRaw, rDbRaw) {
    const lDb = normalizeDb(lDbRaw);
    const rDb = normalizeDb(rDbRaw);
    const pL  = Number.isFinite(lDb) ? Math.pow(10, lDb / 10) : 0;
    const pR  = Number.isFinite(rDb) ? Math.pow(10, rDb / 10) : 0;
    const sum = pL + pR;
    const target = sum > 1e-12 ? (pR - pL) / sum : 0;
    _balanceSmooth += (target - _balanceSmooth) * 0.25;
    _drawSegmentMeter('balanceCanvas', _balanceSmooth, 'balance');
}

function drawVectorscope() {
    const canvas = document.getElementById('vectorscopeCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w  = _vsW || canvas.width;
    const h  = _vsH || canvas.height;
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

        if (_vsDotsMode) {
            // Nuvola di puntini giallo pallido (scatter), senza glow né linee
            ctx.shadowBlur = 0;
            ctx.fillStyle = 'rgba(235, 235, 150, 0.85)';

            for (let i = 0; i < bx.length; i++) {
                const x = cx - bx[i] * cx * _vsZoom;  // negato: L→sinistra, R→destra
                const y = cy - by[i] * cy * _vsZoom;
                ctx.fillRect(x - 0.6, y - 0.6, 1.2, 1.2);
                _vsLastX = x;
                _vsLastY = y;
            }
        } else {
            // Linea continua con glow (fosforo CRT)
            // shadowBlur è in pixel fisici, non segue la transform: va scalato a mano
            ctx.shadowBlur = 5 * _dpr;
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
        }

        ctx.restore();
    }


    if (_vsOverlayCanvas) {
        const oc   = _vsOverlayCanvas;
        const octx = oc.getContext('2d');
        octx.clearRect(0, 0, w, h);

        if (_vsShowOverlay)
            drawVectorscopeOverlay(octx, w, h, cx, cy);

        // Pallino cursore sull'ultima posizione (solo in modalità linea)
        if (!_vsDotsMode && _vsLastX !== null) {
            octx.save();
            octx.shadowBlur = 8 * _dpr;
            octx.shadowColor = 'rgba(255, 220, 100, 1)';
            octx.fillStyle = 'rgb(255, 240, 180)';
            octx.beginPath();
            octx.arc(_vsLastX, _vsLastY, 2, 0, Math.PI * 2);
            octx.fill();
            octx.restore();
        }
    }

    requestAnimationFrame(drawVectorscope);
}

function drawVectorscopeOverlay(ctx, w, h, cx, cy) {
    const r = Math.min(cx, cy) - 1;   // semidiagonale del rombo (ampiezza unitaria)

    ctx.save();
    ctx.shadowBlur = 0;
    ctx.strokeStyle = 'rgba(140, 140, 140, 0.45)';
    ctx.lineWidth = 1;

    // Rombo (quadrato ruotato di 45°): limite di ampiezza unitaria
    ctx.beginPath();
    ctx.moveTo(cx, cy - r);
    ctx.lineTo(cx + r, cy);
    ctx.lineTo(cx, cy + r);
    ctx.lineTo(cx - r, cy);
    ctx.closePath();
    ctx.stroke();

    // Diagonali L/R a ±45° estese fino agli angoli del canvas
    ctx.beginPath();
    ctx.moveTo(0, 0); ctx.lineTo(w, h);   // R channel
    ctx.moveTo(w, 0); ctx.lineTo(0, h);   // L channel
    ctx.stroke();

    ctx.restore();
}

// ===== HiDPI CANVAS & PERSISTENZA STATO UI =====
const UI_STATE_KEY = 'simplemeter.uiState';

let _dpr = 1;
// Dimensioni logiche (CSS px) dei canvas dopo lo scaling HiDPI
let _vsW = 0, _vsH = 0;
let _specW = 0, _specH = 0;

// Ridimensiona il backing store leggendo la dimensione CSS reale del canvas
// (getBoundingClientRect), così funziona sia con dimensioni fisse che con
// canvas flessibili (100% width/height nel nuovo layout a 5 colonne).
function setupHiDPICanvas(canvas) {
    const dpr  = window.devicePixelRatio || 1;
    _dpr = dpr;
    const rect = canvas.getBoundingClientRect();
    const w = Math.max(1, Math.round(rect.width  || parseInt(canvas.getAttribute('width'))  || 180));
    const h = Math.max(1, Math.round(rect.height || parseInt(canvas.getAttribute('height')) || 180));
    canvas.width  = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
    canvas.getContext('2d').setTransform(dpr, 0, 0, dpr, 0, 0);
    return { w, h };
}

// Aggiorna il backing store HiDPI dei canvas del vectorscope.
// Chiamata all'avvio (dopo layout) e quando la finestra viene ridimensionata.
function updateVsSize() {
    const vsCanvas = document.getElementById('vectorscopeCanvas');
    if (vsCanvas) {
        const { w, h } = setupHiDPICanvas(vsCanvas);
        _vsW = w; _vsH = h;
    }
    if (_vsOverlayCanvas) setupHiDPICanvas(_vsOverlayCanvas);
}

function setupCanvasResizeObserver() {
    const specCanvas = document.getElementById('spectrumCanvas');
    const vsCanvas   = document.getElementById('vectorscopeCanvas');
    if (!specCanvas && !vsCanvas) return;

    const refresh = () => {
        if (specCanvas) {
            const d = setupHiDPICanvas(specCanvas);
            _specW = d.w; _specH = d.h;
        }
        if (vsCanvas) {
            const d = setupHiDPICanvas(vsCanvas);
            _vsW = d.w; _vsH = d.h;
            if (_vsOverlayCanvas) setupHiDPICanvas(_vsOverlayCanvas);
        }
    };

    const ro = new ResizeObserver(refresh);
    if (specCanvas) ro.observe(specCanvas);
    if (vsCanvas)   ro.observe(vsCanvas);
}

function loadUiState() {
    try {
        return JSON.parse(localStorage.getItem(UI_STATE_KEY)) || {};
    } catch (e) {
        return {};
    }
}

function saveUiState(patch) {
    try {
        localStorage.setItem(UI_STATE_KEY, JSON.stringify(Object.assign(loadUiState(), patch)));
    } catch (e) {
        // storage non disponibile: lo stato vive solo per la sessione
    }
}

// ===== SPECTRUM ANALYZER =====
const SPEC_BAND_CAP = 256;   // massimo allocato; il C++ cicla 96/192/256
const SPEC_BAND_STEPS = [96, 192, 256];
let _specBands = 96;         // conteggio attivo, seguito dal valore confermato dal C++
const SPEC_DB_RANGES = [-60, -90, -120];  // range ciclabili dal toggle dB
const SPEC_DB_MAX  = 0;
const SPEC_ATTACK  = 0.55;  // lerp per frame verso il target in salita
const SPEC_RELEASE = 0.16;  // discesa più lenta (stile ballistics analogiche)
const SPEC_F_MIN   = 20;
const SPEC_F_MAX   = 20000;

const SPEC_PEAK_DECAY = 0.1;        // dB per frame durante il decadimento (~6 dB/s a 60 fps)
const SPEC_PEAK_HOLD_FRAMES = 120;  // frame di hold prima del decadimento (~2s a 60 fps)
const SPEC_TILT_DB_OCT = 3;    // pendenza pinking: rumore rosa appare piatto
const SPEC_TILT_PIVOT  = 1000; // Hz — frequenza a guadagno zero del tilt
// Sotto questa soglia la banda è considerata silenzio assoluto: nessuna barra
// né peak tick, anche se il tilt porterebbe il valore dentro il range visibile
const SPEC_FLOOR_DB    = -96;

let _specDbMin   = -90;   // minimo del range visualizzato, ciclato dal toggle dB

let _specTargetL = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specTargetR = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specDrawL   = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specDrawR   = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specPeakL    = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specPeakR    = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specPeakAgeL = new Uint16Array(SPEC_BAND_CAP);  // frame dall'ultimo aggiornamento del peak
let _specPeakAgeR = new Uint16Array(SPEC_BAND_CAP);
// Spettri Mid/Side (vista alternativa a L/R)
let _specMsMode    = false;   // false = L/R, true = M/S
let _specTargetMid  = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specTargetSide = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specDrawMid    = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specDrawSide   = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specPeakMid     = new Float32Array(SPEC_BAND_CAP).fill(SPEC_FLOOR_DB - 1);
let _specPeakAgeMid  = new Uint16Array(SPEC_BAND_CAP);
let _specSideT       = new Float32Array(SPEC_BAND_CAP);   // ampiezza Side norm. per banda (linea)
let _specPeakHold = false;
let _specTiltOn   = false;
let _specBarsOn   = true;    // false = nasconde le barre dello spettro (ma tiene grid + overlay P)
// IQ panorama: bilanciamento stereo per banda (-1 = L, +1 = R), smussato nel tempo
let _specIqOn = false;
let _specBal      = new Float32Array(SPEC_BAND_CAP);  // bilanciamento smussato nel tempo
let _specBalDraw  = new Float32Array(SPEC_BAND_CAP);  // dopo smoothing in frequenza
let _specIqActive = new Uint8Array(SPEC_BAND_CAP);    // 1 = banda sopra soglia
// Media a lungo termine: media incrementale della potenza lineare per banda
// (mediare in potenza, non in dB, dà il giusto peso ai passaggi forti)
let _specAvgOn    = false;
let _specAvgL     = new Float64Array(SPEC_BAND_CAP);
let _specAvgR     = new Float64Array(SPEC_BAND_CAP);
let _specAvgMid   = new Float64Array(SPEC_BAND_CAP);
let _specAvgSide  = new Float64Array(SPEC_BAND_CAP);
let _specAvgCount = 0;
let _specTiltOffsets = null;  // offset dB per banda, precalcolati al primo uso
let _specMouse    = null;     // {x, y} in coordinate canvas, null se fuori
let _specColors  = null;
let _specFreqColors = null;   // colore arcobaleno per banda (rosso=basse, viola=alte)

function hslToRgbStr(h, s, l) {
    s /= 100; l /= 100;
    const c = (1 - Math.abs(2 * l - 1)) * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = l - c / 2;
    let r = 0, g = 0, b = 0;
    if      (h < 60)  { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else              { r = c; b = x; }
    return `rgb(${Math.round((r + m) * 255)}, ${Math.round((g + m) * 255)}, ${Math.round((b + m) * 255)})`;
}

// Arcobaleno per frequenza: banda 0 (basse) = rosso → banda alta = viola
function buildSpecFreqColors() {
    _specFreqColors = new Array(SPEC_BAND_CAP);
    const HUE_LOW  = 0;     // rosso
    const HUE_HIGH = 285;   // viola
    for (let b = 0; b < _specBands; b++) {
        const t = b / (_specBands - 1);
        const hue = HUE_LOW + (HUE_HIGH - HUE_LOW) * t;
        _specFreqColors[b] = hslToRgbStr(hue, 85, 55);
    }
}

function buildSpecTiltOffsets() {
    _specTiltOffsets = new Float32Array(SPEC_BAND_CAP);
    for (let b = 0; b < _specBands; b++) {
        const freq = SPEC_F_MIN * Math.pow(SPEC_F_MAX / SPEC_F_MIN, (b + 0.5) / _specBands);
        _specTiltOffsets[b] = SPEC_TILT_DB_OCT * Math.log2(freq / SPEC_TILT_PIVOT);
    }
}

let _setSpecBandsNative = null;

// Applica il nuovo conteggio di bande (confermato dal C++): ricostruisce le tabelle
// dipendenti dalla mappa freq→banda e riazzera gli stati, perché il binning è cambiato.
function applySpecBandCount(n) {
    n = Math.max(32, Math.min(SPEC_BAND_CAP, n | 0));
    _specBands = n;

    buildSpecFreqColors();
    buildSpecTiltOffsets();

    const FLOOR = SPEC_FLOOR_DB - 1;
    _specTargetL.fill(FLOOR);   _specTargetR.fill(FLOOR);
    _specDrawL.fill(FLOOR);     _specDrawR.fill(FLOOR);
    _specPeakL.fill(FLOOR);     _specPeakR.fill(FLOOR);
    _specPeakAgeL.fill(0);      _specPeakAgeR.fill(0);
    _specTargetMid.fill(FLOOR); _specTargetSide.fill(FLOOR);
    _specDrawMid.fill(FLOOR);   _specDrawSide.fill(FLOOR);
    _specPeakMid.fill(FLOOR);   _specPeakAgeMid.fill(0);
    _specSideT.fill(0);
    _specBal.fill(0);  _specBalDraw.fill(0);  _specIqActive.fill(0);
    _specAvgL.fill(0); _specAvgR.fill(0); _specAvgMid.fill(0); _specAvgSide.fill(0);
    _specAvgCount = 0;

    const lbl = document.getElementById('specBandsToggle');
    if (lbl) lbl.textContent = String(n);
}

// Richiede al C++ il prossimo conteggio nel ciclo 96 → 192 → 256 → 96
function cycleSpecBandCount() {
    const i = SPEC_BAND_STEPS.indexOf(_specBands);
    const next = SPEC_BAND_STEPS[(i + 1) % SPEC_BAND_STEPS.length];
    if (_setSpecBandsNative) {
        try { _setSpecBandsNative(next); } catch (e) {}
    }
    saveUiState({ specBands: next });
    // Aggiorna subito l'etichetta; il rendering segue il conteggio confermato dal C++
    const lbl = document.getElementById('specBandsToggle');
    if (lbl) lbl.textContent = String(next);
}

function buildSpecPalette() {
    const stops = [
        [0.00,  40,  26,  12],
        [0.40, 194, 146,  68],
        [0.75, 255, 210, 110],
        [1.00, 255, 250, 230]
    ];
    _specColors = new Array(101);
    for (let i = 0; i <= 100; i++) {
        const t = i / 100;
        let s = 0;
        while (s < stops.length - 2 && t > stops[s + 1][0]) s++;
        const [t0, r0, g0, b0] = stops[s];
        const [t1, r1, g1, b1] = stops[s + 1];
        const f = Math.min(1, Math.max(0, (t - t0) / (t1 - t0)));
        _specColors[i] = `rgb(${Math.round(r0 + (r1 - r0) * f)}, ${Math.round(g0 + (g1 - g0) * f)}, ${Math.round(b0 + (b1 - b0) * f)})`;
    }
}

function specFreqY(freq, h) {
    return h - Math.log(freq / SPEC_F_MIN) / Math.log(SPEC_F_MAX / SPEC_F_MIN) * h;
}

function drawSpectrumGrid(ctx, w, h, midX) {
    ctx.font         = '7px monospace';
    ctx.textBaseline = 'middle';
    ctx.lineWidth    = 0.5;

    // Linee orizzontali di riferimento frequenza (per ottave), etichette sul bordo destro
    ctx.strokeStyle = 'rgba(194, 146, 68, 0.12)';
    ctx.fillStyle   = 'rgba(194, 146, 68, 0.45)';
    ctx.textAlign   = 'right';
    const freqMarks = [
        [30, '30'], [63, '63'], [125, '125'], [250, '250'], [500, '500'],
        [1000, '1k'], [2000, '2k'], [4000, '4k'], [8000, '8k'],
        [16000, '16k']
    ];
    for (const [freq, lbl] of freqMarks) {
        const y = specFreqY(freq, h);
        ctx.beginPath();
        ctx.moveTo(0, y); ctx.lineTo(w, y);
        ctx.stroke();
        // Vicino al bordo superiore l'etichetta va sotto la linea, non sopra
        ctx.fillText(lbl, w - 2, y < 12 ? y + 6 : y - 5);
    }

    ctx.strokeStyle = 'rgba(194, 146, 68, 0.08)';
    const halfW = midX - 1;
    for (let db = _specDbMin + 30; db <= -30; db += 30) {
        const t  = (db - _specDbMin) / (SPEC_DB_MAX - _specDbMin);
        const dx = t * halfW;
        ctx.beginPath();
        ctx.moveTo(midX - dx, 0); ctx.lineTo(midX - dx, h);
        ctx.moveTo(midX + dx, 0); ctx.lineTo(midX + dx, h);
        ctx.stroke();
    }

    ctx.textAlign = 'left';
    ctx.fillText(_specDbMin + ' dB', 2, h - 6);

    ctx.strokeStyle = 'rgba(194, 146, 68, 0.25)';
    ctx.beginPath();
    ctx.moveTo(midX, 0); ctx.lineTo(midX, h);
    ctx.stroke();
}

function drawSpectrum() {
    const canvas = document.getElementById('spectrumCanvas');
    if (!canvas) return;
    if (!_specColors) buildSpecPalette();

    if (!_specFreqColors) buildSpecFreqColors();

    const ctx  = canvas.getContext('2d');
    const w    = _specW || canvas.width;
    const h    = _specH || canvas.height;
    const midX = w * 0.5;
    const gap  = 1;                       // spazio ai lati della linea centrale
    const halfW = midX - gap - 1;
    const rowH = h / _specBands;
    const dbRange = SPEC_DB_MAX - _specDbMin;

    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, w, h);
    drawSpectrumGrid(ctx, w, h, midX);

    if (_specBarsOn && _specAvgOn && _specAvgCount > 0) {
        const FILL       = { fill: 'rgba(194, 146, 68, 0.10)', stroke: 'rgba(194, 146, 68, 0.35)' };
        const WHITE_FILL = { fill: 'rgba(255, 255, 255, 0.10)', stroke: 'rgba(255, 255, 255, 0.40)' };
        if (!_specMsMode) {
            drawSpectrumAvgSide(ctx, -1, _specAvgL, midX, gap, halfW, h, rowH, dbRange, FILL);
            drawSpectrumAvgSide(ctx, +1, _specAvgR, midX, gap, halfW, h, rowH, dbRange, FILL);
        } else {
            // Media M/S: Mid riempimento dorato + Side riempimento bianco, entrambi simmetrici
            drawSpectrumAvgSide(ctx, -1, _specAvgMid,  midX, gap, halfW, h, rowH, dbRange, FILL);
            drawSpectrumAvgSide(ctx, +1, _specAvgMid,  midX, gap, halfW, h, rowH, dbRange, FILL);
            drawSpectrumAvgSide(ctx, -1, _specAvgSide, midX, gap, halfW, h, rowH, dbRange, WHITE_FILL);
            drawSpectrumAvgSide(ctx, +1, _specAvgSide, midX, gap, halfW, h, rowH, dbRange, WHITE_FILL);
        }
    }

    const norm = (db, tilt) => db <= SPEC_FLOOR_DB ? 0
        : Math.min(1, Math.max(0, (db + tilt - _specDbMin) / dbRange));

    for (let b = 0; b < _specBands; b++) {
        // Smoothing temporale di tutte e 4 le serie (L/R servono anche all'overlay IQ)
        _specDrawL[b]    += (_specTargetL[b]    - _specDrawL[b])    * (_specTargetL[b]    > _specDrawL[b]    ? SPEC_ATTACK : SPEC_RELEASE);
        _specDrawR[b]    += (_specTargetR[b]    - _specDrawR[b])    * (_specTargetR[b]    > _specDrawR[b]    ? SPEC_ATTACK : SPEC_RELEASE);
        _specDrawMid[b]  += (_specTargetMid[b]  - _specDrawMid[b])  * (_specTargetMid[b]  > _specDrawMid[b]  ? SPEC_ATTACK : SPEC_RELEASE);
        _specDrawSide[b] += (_specTargetSide[b] - _specDrawSide[b]) * (_specTargetSide[b] > _specDrawSide[b] ? SPEC_ATTACK : SPEC_RELEASE);

        // Le barre si disegnano solo se abilitate; lo smoothing sopra resta sempre
        // attivo perché alimenta l'overlay P (panorama / correlazione di fase).
        if (!_specBarsOn) continue;

        const tilt = _specTiltOn ? _specTiltOffsets[b] : 0;
        const y  = h - (b + 1) * rowH;
        const rh = Math.max(1, rowH - 1);
        const bandColor = _specFreqColors[b];

        if (!_specMsMode) {
            // ---- Vista L/R: L a sinistra, R a destra ----
            const tL = norm(_specDrawL[b], tilt);
            const tR = norm(_specDrawR[b], tilt);
            if (tL > 0.003) { ctx.fillStyle = bandColor; ctx.fillRect(midX - gap - Math.max(1, tL * halfW), y, Math.max(1, tL * halfW), rh); }
            if (tR > 0.003) { ctx.fillStyle = bandColor; ctx.fillRect(midX + gap, y, Math.max(1, tR * halfW), rh); }

            if (_specPeakHold) {
                if (_specDrawL[b] >= _specPeakL[b]) { _specPeakL[b] = _specDrawL[b]; _specPeakAgeL[b] = 0; }
                else { _specPeakAgeL[b]++; if (_specPeakAgeL[b] > SPEC_PEAK_HOLD_FRAMES) _specPeakL[b] = Math.max(_specPeakL[b] - SPEC_PEAK_DECAY, SPEC_FLOOR_DB - 1); }
                if (_specDrawR[b] >= _specPeakR[b]) { _specPeakR[b] = _specDrawR[b]; _specPeakAgeR[b] = 0; }
                else { _specPeakAgeR[b]++; if (_specPeakAgeR[b] > SPEC_PEAK_HOLD_FRAMES) _specPeakR[b] = Math.max(_specPeakR[b] - SPEC_PEAK_DECAY, SPEC_FLOOR_DB - 1); }

                const pL = norm(_specPeakL[b], tilt);
                const pR = norm(_specPeakR[b], tilt);
                ctx.fillStyle = 'rgba(255, 235, 170, 0.85)';
                if (pL > 0.01) ctx.fillRect(midX - gap - pL * halfW, y, 1, rh);
                if (pR > 0.01) ctx.fillRect(midX + gap + pR * halfW - 1, y, 1, rh);
            }
        } else {
            // ---- Vista M/S: Mid riempito simmetrico + Side come linea sui due lati ----
            const tMid  = norm(_specDrawMid[b],  tilt);
            const tSide = norm(_specDrawSide[b], tilt);

            // Mid: area piena arcobaleno, specchiata su entrambi i lati del centro
            if (tMid > 0.003) {
                const len = Math.max(1, tMid * halfW);
                ctx.fillStyle = bandColor;
                ctx.fillRect(midX - gap - len, y, len, rh);
                ctx.fillRect(midX + gap, y, len, rh);
            }

            // Side: memorizza l'ampiezza, la linea continua si disegna dopo il loop
            _specSideT[b] = tSide;

            // Peak hold sul Mid (tick tenue, simmetrico)
            if (_specPeakHold) {
                if (_specDrawMid[b] >= _specPeakMid[b]) { _specPeakMid[b] = _specDrawMid[b]; _specPeakAgeMid[b] = 0; }
                else { _specPeakAgeMid[b]++; if (_specPeakAgeMid[b] > SPEC_PEAK_HOLD_FRAMES) _specPeakMid[b] = Math.max(_specPeakMid[b] - SPEC_PEAK_DECAY, SPEC_FLOOR_DB - 1); }

                const pM = norm(_specPeakMid[b], tilt);
                if (pM > 0.01) {
                    ctx.fillStyle = 'rgba(255, 235, 170, 0.85)';
                    ctx.fillRect(midX - gap - pM * halfW, y, 1, rh);
                    ctx.fillRect(midX + gap + pM * halfW - 1, y, 1, rh);
                }
            }
        }
    }

    // --- Side come linea continua bianca su entrambi i lati (solo M/S) ---
    if (_specBarsOn && _specMsMode) {
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.9)';
        ctx.lineWidth = 1.2;
        ctx.lineJoin = 'round';
        for (const sideSign of [-1, +1]) {
            ctx.beginPath();
            for (let b = 0; b < _specBands; b++) {
                const x = midX + sideSign * (gap + _specSideT[b] * halfW);
                const y = h - (b + 0.5) * rowH;
                b === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
            }
            ctx.stroke();
        }
    }

    // --- Overlay IQ: panorama stereo dell'energia per banda (barre bianche) ---
    if (_specIqOn) drawSpecIq(ctx, midX, halfW, h, rowH);

    if (_specMouse) drawSpectrumReadout(ctx, w, h, midX);

    requestAnimationFrame(drawSpectrum);
}

// Overlay "IQ": per ogni banda mostra dove si concentra l'energia nel panorama
// stereo. Posizione = bilanciamento RMS L/R della banda; soglia per nascondere
// le bande sotto il floor di visualizzazione.
const SPEC_IQ_SMOOTH = 0.30;   // smoothing temporale per banda
const SPEC_IQ_SCALE  = 0.33;   // lunghezza max della barra come frazione di halfW
const SPEC_IQ_FREQ_R = 2;      // raggio finestra smoothing in frequenza (±bande)
function drawSpecIq(ctx, midX, halfW, h, rowH) {
    const ms = _specMsMode;

    // Fase 1: valore per banda con smoothing temporale (target 0 = centro per le
    // bande sotto soglia, così decadono dolcemente verso il centro).
    //  - L/R : panorama energia stereo  -1 (L) .. +1 (R)
    //  - M/S : correlazione di fase  (M²−S²)/(M²+S²)  -1 (contro-fase) .. +1 (in fase)
    for (let b = 0; b < _specBands; b++) {
        const dbA = ms ? _specDrawMid[b]  : _specDrawR[b];   // +target → destra
        const dbB = ms ? _specDrawSide[b] : _specDrawL[b];
        const loud = Math.max(dbA, dbB);
        const active = loud > SPEC_FLOOR_DB && loud >= _specDbMin;
        _specIqActive[b] = active ? 1 : 0;

        let target = 0;
        if (active) {
            const pA = Math.pow(10, dbA / 10);
            const pB = Math.pow(10, dbB / 10);
            const sum = pA + pB;
            target = sum > 1e-20 ? (pA - pB) / sum : 0;
        }
        _specBal[b] += (target - _specBal[b]) * SPEC_IQ_SMOOTH;
    }

    // Fase 2: smoothing in frequenza (media triangolare su ±SPEC_IQ_FREQ_R bande)
    for (let b = 0; b < _specBands; b++) {
        let acc = 0, wsum = 0;
        for (let d = -SPEC_IQ_FREQ_R; d <= SPEC_IQ_FREQ_R; d++) {
            const i = b + d;
            if (i < 0 || i >= _specBands) continue;
            const w = SPEC_IQ_FREQ_R + 1 - Math.abs(d);   // peso triangolare
            acc += _specBal[i] * w;
            wsum += w;
        }
        _specBalDraw[b] = wsum > 0 ? acc / wsum : _specBal[b];
    }

    // Fase 3: disegno. Barra dal centro, solo bande attive.
    //  - L/R : barra bianca verso il lato dominante.
    //  - M/S : barra colorata (verde = in fase a destra, rosso = contro-fase a
    //          sinistra, bianco/arancio vicino a 0) con la palette della correlazione.
    const rh = Math.max(1, rowH);
    for (let b = 0; b < _specBands; b++) {
        if (!_specIqActive[b]) continue;
        const val = _specBalDraw[b];
        const len = val * halfW * SPEC_IQ_SCALE;
        const y = h - (b + 1) * rowH;
        if (ms) {
            const [r, g, bl] = _segCorrColor(val);
            ctx.fillStyle = `rgba(${r},${g},${bl},0.92)`;
        } else {
            ctx.fillStyle = 'rgba(255, 255, 255, 0.92)';
        }
        if (len >= 0) ctx.fillRect(midX, y, len, rh);
        else          ctx.fillRect(midX + len, y, -len, rh);
    }
}

// Sagoma della media a lungo termine per un lato.
// side: -1 = sinistra, +1 = destra; data = array di potenze medie per banda;
// style: { fill, stroke } (fill null = solo profilo a linea).
function drawSpectrumAvgSide(ctx, side, data, midX, gap, halfW, h, rowH, dbRange, style) {
    const xs = new Float32Array(SPEC_BAND_CAP);
    const ys = new Float32Array(SPEC_BAND_CAP);

    for (let b = 0; b < _specBands; b++) {
        const mean = data[b];
        const db   = mean > 1e-12 ? 10 * Math.log10(mean) : -Infinity;
        const tilt = _specTiltOn ? _specTiltOffsets[b] : 0;
        const t    = db <= SPEC_FLOOR_DB ? 0
            : Math.min(1, Math.max(0, (db + tilt - _specDbMin) / dbRange));
        xs[b] = midX + side * (gap + t * halfW);
        ys[b] = h - (b + 0.5) * rowH;
    }

    // Riempimento tenue chiuso sulla linea centrale (opzionale)
    if (style.fill) {
        ctx.beginPath();
        ctx.moveTo(midX + side * gap, ys[0]);
        for (let b = 0; b < _specBands; b++) ctx.lineTo(xs[b], ys[b]);
        ctx.lineTo(midX + side * gap, ys[_specBands - 1]);
        ctx.closePath();
        ctx.fillStyle = style.fill;
        ctx.fill();
    }

    // Profilo sottile della curva
    if (style.stroke) {
        ctx.beginPath();
        for (let b = 0; b < _specBands; b++)
            b === 0 ? ctx.moveTo(xs[b], ys[b]) : ctx.lineTo(xs[b], ys[b]);
        ctx.strokeStyle = style.stroke;
        ctx.lineWidth = 1;
        ctx.stroke();
    }
}

const SPEC_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

function freqToNoteName(freq) {
    const midi = Math.round(12 * Math.log2(freq / 440) + 69);
    if (midi < 0 || midi > 135) return '--';
    return SPEC_NOTE_NAMES[midi % 12] + (Math.floor(midi / 12) - 1);
}

function formatSpecFreq(freq) {
    return freq < 1000 ? Math.round(freq) + ' Hz'
                       : (freq / 1000).toFixed(freq < 10000 ? 2 : 1) + ' kHz';
}

function drawSpectrumReadout(ctx, w, h, midX) {
    const { x, y } = _specMouse;

    // Frequenza dalla posizione verticale (scala log, basse in basso)
    const frac = Math.min(1, Math.max(0, (h - y) / h));
    const freq = SPEC_F_MIN * Math.pow(SPEC_F_MAX / SPEC_F_MIN, frac);

    // Canale dal lato del cursore, dB dalla banda sotto il cursore
    const band = Math.min(_specBands - 1, Math.max(0, Math.floor(frac * _specBands)));
    const isLeft = x < midX;
    let chLabel, db;
    if (_specMsMode) {
        // Mid è simmetrico su entrambi i lati; Side è il contorno sui lati
        chLabel = 'M';
        db = _specDrawMid[band];
    } else {
        chLabel = isLeft ? 'L' : 'R';
        db = isLeft ? _specDrawL[band] : _specDrawR[band];
    }
    const dbTxt = db <= SPEC_FLOOR_DB ? '−∞' : db.toFixed(1) + ' dB';

    // Linea orizzontale di tracking sul cursore
    ctx.strokeStyle = 'rgba(255, 235, 170, 0.35)';
    ctx.lineWidth = 0.5;
    ctx.beginPath();
    ctx.moveTo(0, y); ctx.lineTo(w, y);
    ctx.stroke();

    const text = `${chLabel}  ${formatSpecFreq(freq)}  ${freqToNoteName(freq)}  ${dbTxt}`;

    ctx.font = 'bold 8px monospace';
    ctx.textAlign = 'right';
    ctx.textBaseline = 'bottom';
    const tw = ctx.measureText(text).width;

    ctx.fillStyle = 'rgba(10, 10, 15, 0.85)';
    ctx.fillRect(w - tw - 8, h - 14, tw + 6, 12);
    ctx.fillStyle = 'rgb(255, 235, 170)';
    ctx.fillText(text, w - 5, h - 4);
}

function setupSpectrumControls() {
    const canvas = document.getElementById('spectrumCanvas');
    if (!canvas) return;

    const dims = setupHiDPICanvas(canvas);
    _specW = dims.w;
    _specH = dims.h;

    const saved = loadUiState();

    const btnBars = document.getElementById('specBarsToggle');
    _specBarsOn = saved.specBars !== false;   // default acceso
    if (btnBars) {
        btnBars.style.opacity = _specBarsOn ? '1' : '0.35';
        btnBars.addEventListener('click', () => {
            _specBarsOn = !_specBarsOn;
            btnBars.style.opacity = _specBarsOn ? '1' : '0.35';
            saveUiState({ specBars: _specBarsOn });
        });
    }

    const btnPeak = document.getElementById('specPeakToggle');
    _specPeakHold = saved.specPeak === true;
    btnPeak.style.opacity = _specPeakHold ? '1' : '0.35';
    btnPeak.addEventListener('click', () => {
        _specPeakHold = !_specPeakHold;
        btnPeak.style.opacity = _specPeakHold ? '1' : '0.35';
        if (!_specPeakHold) {
            _specPeakL.fill(SPEC_FLOOR_DB - 1);
            _specPeakR.fill(SPEC_FLOOR_DB - 1);
            _specPeakAgeL.fill(0);
            _specPeakAgeR.fill(0);
            _specPeakMid.fill(SPEC_FLOOR_DB - 1);
            _specPeakAgeMid.fill(0);
        }
        saveUiState({ specPeak: _specPeakHold });
    });

    const btnTilt = document.getElementById('specTiltToggle');
    _specTiltOn = saved.specTilt === true;
    if (_specTiltOn && !_specTiltOffsets) buildSpecTiltOffsets();
    btnTilt.style.opacity = _specTiltOn ? '1' : '0.35';
    btnTilt.addEventListener('click', () => {
        _specTiltOn = !_specTiltOn;
        btnTilt.style.opacity = _specTiltOn ? '1' : '0.35';
        if (_specTiltOn && !_specTiltOffsets) buildSpecTiltOffsets();
        saveUiState({ specTilt: _specTiltOn });
    });

    const btnAvg = document.getElementById('specAvgToggle');
    _specAvgOn = saved.specAvg === true;
    btnAvg.style.opacity = _specAvgOn ? '1' : '0.35';
    btnAvg.addEventListener('click', () => {
        _specAvgOn = !_specAvgOn;
        btnAvg.style.opacity = _specAvgOn ? '1' : '0.35';
        if (!_specAvgOn) {
            // Spegnere il toggle azzera l'accumulo: alla riattivazione si riparte da zero
            _specAvgL.fill(0);
            _specAvgR.fill(0);
            _specAvgMid.fill(0);
            _specAvgSide.fill(0);
            _specAvgCount = 0;
        }
        saveUiState({ specAvg: _specAvgOn });
    });

    const btnIq = document.getElementById('specIqToggle');
    _specIqOn = saved.specIq === true;
    btnIq.style.opacity = _specIqOn ? '1' : '0.35';
    btnIq.addEventListener('click', () => {
        _specIqOn = !_specIqOn;
        btnIq.style.opacity = _specIqOn ? '1' : '0.35';
        if (!_specIqOn) _specBal.fill(0);   // reset al centro alla disattivazione
        saveUiState({ specIq: _specIqOn });
    });

    const btnMs = document.getElementById('specMsToggle');
    _specMsMode = saved.specMs === true;
    btnMs.style.opacity = _specMsMode ? '1' : '0.35';
    btnMs.addEventListener('click', () => {
        _specMsMode = !_specMsMode;
        btnMs.style.opacity = _specMsMode ? '1' : '0.35';
        // Il pannello P cambia significato (panorama L/R ↔ correlazione di fase):
        // azzera lo smoothing per non far scivolare i valori dalla metrica precedente
        _specBal.fill(0);
        _specBalDraw.fill(0);
        saveUiState({ specMs: _specMsMode });
    });

    const btnBands = document.getElementById('specBandsToggle');
    if (btnBands) {
        try { _setSpecBandsNative = Juce.getNativeFunction('setSpecBands'); }
        catch (e) { _setSpecBandsNative = null; }
        const wanted = SPEC_BAND_STEPS.includes(saved.specBands) ? saved.specBands : 96;
        btnBands.textContent = String(_specBands);
        btnBands.title = 'Bande spettro (click per ciclare 96 / 192 / 256)';
        btnBands.addEventListener('click', cycleSpecBandCount);
        // Ripristina il conteggio salvato richiedendolo al C++ (default 96)
        if (wanted !== 96 && _setSpecBandsNative) {
            try { _setSpecBandsNative(wanted); } catch (e) {}
            btnBands.textContent = String(wanted);
        }
    }

    const btnRange = document.getElementById('specRangeToggle');
    if (SPEC_DB_RANGES.includes(saved.specDbMin))
        _specDbMin = saved.specDbMin;
    btnRange.title = `Range dB: ${_specDbMin} → 0 (click per ciclare)`;
    btnRange.addEventListener('click', () => {
        const i = SPEC_DB_RANGES.indexOf(_specDbMin);
        _specDbMin = SPEC_DB_RANGES[(i + 1) % SPEC_DB_RANGES.length];
        btnRange.title = `Range dB: ${_specDbMin} → 0 (click per ciclare)`;
        saveUiState({ specDbMin: _specDbMin });
    });

    canvas.addEventListener('mousemove', (e) => {
        const rect = canvas.getBoundingClientRect();
        // Coordinate logiche: il backing store è scalato per dpr, il mouse no
        _specMouse = {
            x: (e.clientX - rect.left) * _specW / rect.width,
            y: (e.clientY - rect.top)  * _specH / rect.height
        };
    });
    canvas.addEventListener('mouseleave', () => { _specMouse = null; });
}

// ===== CORE LOGIC: SLIDER E PARAMETRI =====
const FADER_UNITY_POS = 0.5;

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

// Rende un'etichetta dB editabile con doppio click.
// onCommit(db) viene chiamato con il valore inserito dall'utente; onCancel() ripristina.
// Rileva il doppio click manualmente tramite due 'click' ravvicinati.
// In questo WebView 'dblclick' non scatta in modo affidabile sugli elementi
// con user-select:none (ereditato da .panel); i 'click' invece arrivano sempre.
function onManualDblClick(el, handler) {
    let lastT = 0;
    el.addEventListener('click', (e) => {
        const now = Date.now();
        if (now - lastT < 350) {
            lastT = 0;
            handler(e);
        } else {
            lastT = now;
        }
    });
}

function makeEditableLabel(labelEl, minDb, maxDb, onCommit) {
    labelEl.style.cursor = 'text';
    labelEl.title = 'Doppio click per inserire un valore';

    onManualDblClick(labelEl, (e) => {
        e.stopPropagation();
        const current = parseFloat(labelEl.textContent) || 0;

        const inp = document.createElement('input');
        inp.type = 'number';
        inp.value = current.toFixed(1);
        inp.min = String(minDb);
        inp.max = String(maxDb);
        inp.step = '0.1';
        inp.style.cssText = [
            'width:52px', 'background:#1a1a1a', 'color:rgb(194,146,68)',
            'border:1px solid rgb(194,146,68)', 'border-radius:2px',
            'font:inherit', 'font-size:inherit', 'text-align:center',
            'padding:0 2px', 'outline:none', 'appearance:textfield',
            '-moz-appearance:textfield'
        ].join(';');

        let done = false;
        const restore = () => {
            if (done) return;
            done = true;
            if (inp.parentNode) labelEl.replaceChild(document.createTextNode(labelEl._savedText), inp);
        };

        labelEl._savedText = labelEl.textContent;
        labelEl.textContent = '';
        labelEl.appendChild(inp);
        setTimeout(() => { inp.focus(); inp.select(); }, 0);

        const commit = () => {
            if (done) return;
            done = true;
            const db = Math.max(minDb, Math.min(maxDb, parseFloat(inp.value.replace(',', '.')) || 0));
            onCommit(db);
            inp.remove();
        };

        inp.addEventListener('keydown', (ev) => {
            if (ev.key === 'Enter')  { ev.preventDefault(); commit(); }
            if (ev.key === 'Escape') { ev.preventDefault(); restore(); }
        });
        inp.addEventListener('blur', commit);
    });
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

            label.textContent = snapZero(db).toFixed(1) + ' dB';

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

            label.textContent = snapZero(Math.max(range.start, Math.min(range.end, db))).toFixed(1) + ' dB';
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

    makeEditableLabel(label, minDb, maxDb, (db) => {
        setGainDb(db);
        slider.value = gainDbToSliderPos(db, minDb, maxDb).toFixed(4);
    });

    debugLog('✓ wireGain completed successfully', 'SUCCESS');
}

function wireNumericParameter({ paramId, inputId, fallbackRange, step, defaultValue, decimals = 1, onValueChange }) {
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

            if (onValueChange) onValueChange(value);
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

            if (onValueChange) onValueChange(clamped);
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

    // API per pilotare il parametro dall'esterno. NB: si imposta il valore preciso
    // direttamente, senza passare da commitInput() che rileggerebbe il campo
    // arrotondato (es. 0.3434 → "0.3") perdendo precisione.
    return {
        setValue: (value) => {
            try { state.sliderDragStarted(); } catch (e) {}
            setParameterValue(value, true);
            try { state.sliderDragEnded(); } catch (e) {}
        },
    };
}

// Stato condiviso per la modifica manuale del delay (campo "Delay")
let _phaseTempVal   = TEMP_DEFAULT;
let _phaseSampleRate = 0;

function wirePhaseControls() {
    wireNumericParameter({
        paramId: 'temperature',
        inputId: 'temperatureInput',
        fallbackRange: TEMP_RANGE,
        step: TEMP_STEP,
        defaultValue: TEMP_DEFAULT,
        onValueChange: (v) => { _phaseTempVal = v; },
    });

    const distanceApi = wireNumericParameter({
        paramId: 'distance',
        inputId: 'distanceInput',
        fallbackRange: DIST_RANGE,
        step: DIST_STEP,
        defaultValue: DIST_DEFAULT,
    });

    // Campo Delay editabile: l'utente inserisce ms (o samples col suffisso "sa"/"s"),
    // si ricava la distanza con la formula inversa della patch e si scrive su `distance`.
    setupDelayEditing(distanceApi);

    wireToggleParameter({
        paramId: 'phase_inv',
        buttonId: 'phaseToggleBtn',
        activeClass: 'active'
    });

    // Modal Phase Alignment (stesso comportamento del menu About)
    const alignBtn  = document.getElementById('phaseAlignBtn');
    const modal     = document.getElementById('phaseAlignPopup');
    const closeBtn  = document.getElementById('phaseAlignCloseBtn');
    if (alignBtn && modal) {
        const toggleModal = () => {
            const isOpen = window.getComputedStyle(modal).display === 'flex';
            modal.style.display = isOpen ? 'none' : 'flex';
            alignBtn.classList.toggle('active', !isOpen);
        };
        alignBtn.onclick = toggleModal;
        if (closeBtn) closeBtn.onclick = toggleModal;
        // Chiude cliccando sullo sfondo del modal
        modal.addEventListener('click', (e) => {
            if (e.target === modal) toggleModal();
        });
    }
}

// Velocità del suono usata nella patch RNBO: c = 331.4 + 0.6·T  (m/s)
function speedOfSound(tempC) {
    return 331.4 + 0.6 * tempC;
}

// delay (ms) → distanza (m), inversa di:  delay_ms = distance / c · 1000
function delayMsToDistance(delayMs, tempC) {
    return (delayMs / 1000) * speedOfSound(tempC);
}

function setupDelayEditing(distanceApi) {
    const labelEl = document.getElementById('delayTimeValue');
    const editBtn = document.getElementById('delayEditBtn');
    if (!labelEl || !editBtn || !distanceApi) return;

    let _editing = false;

    editBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        if (_editing) return;
        _editing = true;

        // ms correnti dal testo "X.X ms (...)"
        const currentMs = parseFloat(labelEl.textContent) || 0;

        const inp = document.createElement('input');
        inp.type = 'text';
        inp.value = currentMs.toFixed(1);
        inp.style.cssText = [
            'width:70px', 'background:#1a1a1a', 'color:rgb(194,146,68)',
            'border:1px solid rgb(194,146,68)', 'border-radius:2px',
            'font:inherit', 'font-size:inherit', 'text-align:center',
            'padding:0 2px', 'outline:none', 'user-select:text'
        ].join(';');

        const saved = labelEl.textContent;
        let done = false;

        const finish = (apply) => {
            if (done) return;
            done = true;
            _editing = false;

            if (apply) {
                const raw = inp.value.trim().toLowerCase().replace(',', '.');
                const num = parseFloat(raw);
                if (Number.isFinite(num)) {
                    // Suffisso "sa"/"s"/"samples" ⇒ il valore è in sample
                    const isSamples = /(sa|s|samples)$/.test(raw) && !/ms$/.test(raw);
                    let delayMs = num;
                    if (isSamples && _phaseSampleRate > 0)
                        delayMs = num / _phaseSampleRate * 1000;

                    const distance = delayMsToDistance(delayMs, _phaseTempVal);
                    distanceApi.setValue(distance);   // clampata a [0, 60] m dal parametro
                }
            }

            if (inp.parentNode) labelEl.replaceChild(document.createTextNode(saved), inp);
        };

        labelEl.textContent = '';
        labelEl.appendChild(inp);

        // focus/select nel tick successivo: evita il blur immediato post-dblclick
        setTimeout(() => { inp.focus(); inp.select(); }, 0);

        inp.addEventListener('keydown', (ev) => {
            if (ev.key === 'Enter')  { ev.preventDefault(); finish(true); }
            if (ev.key === 'Escape') { ev.preventDefault(); finish(false); }
        });
        inp.addEventListener('blur', () => finish(true));
    });
}

function setDelayTime(delayValue, sampleRate) {
    const delayLabel = document.getElementById('delayTimeValue');
    if (!delayLabel) return;

    if (Number.isFinite(sampleRate) && sampleRate > 0)
        _phaseSampleRate = sampleRate;

    // Mentre l'utente sta digitando nel campo, non sovrascrivere il testo
    if (delayLabel.firstElementChild && delayLabel.firstElementChild.tagName === 'INPUT')
        return;

    let text = formatNumericValue(delayValue, 1) + ' ms';
    let samples = 0;
    if (Number.isFinite(sampleRate) && sampleRate > 0) {
        samples = Math.round((delayValue * sampleRate) / 1000);
        text += ` (${samples} sa)`;
    }
    delayLabel.textContent = text;

    // Illumina il button ⧖ se e solo se il ritardo è diverso da 0 ms / 0 sample
    const alignBtn = document.getElementById('phaseAlignBtn');
    if (alignBtn) {
        const engaged = Math.abs(delayValue) > 1e-6 || samples !== 0;
        alignBtn.classList.toggle('engaged', engaged);
    }
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
    debugLog("wireMSMatrix(): Inizializzazione controlli Mid/Side con toggle L/R...", "INFO");

    const GAIN_RANGE_FALLBACK = { start: -12, end: 12, skew: 1 };

    // Ottieni tutti e 4 i gain state upfront
    let ch1Ms, ch2Ms, ch1Lr, ch2Lr;
    let muCh1Ms, muCh2Ms, muCh1Lr, muCh2Lr;

    try {
        ch1Ms = Juce.getSliderState('mid_gain');
        ch2Ms = Juce.getSliderState('side_gain');
        ch1Lr = Juce.getSliderState('l_gain');
        ch2Lr = Juce.getSliderState('r_gain');
    } catch (e) {
        debugLog('wireMSMatrix: gain states non disponibili: ' + e.message, 'ERROR');
        return;
    }

    try {
        muCh1Ms = Juce.getToggleState('mid_mute');
        muCh2Ms = Juce.getToggleState('side_mute');
        muCh1Lr = Juce.getToggleState('l_mute');
        muCh2Lr = Juce.getToggleState('r_mute');
    } catch (e) {
        debugLog('wireMSMatrix: mute states non disponibili: ' + e.message, 'WARN');
    }

    // Oggetto di dispatch mutabile — tutti gli event handler vi leggono tramite ch.gainA/ch.gainB
    // In questo modo il toggle cambia ch.gainA e il comportamento di tutti i listener cambia
    const ch = {
        gainA: ch1Ms, gainB: ch2Ms,
        muteA: muCh1Ms, muteB: muCh2Ms
    };

    // DOM
    const sl1   = document.getElementById('midGainSlider');
    const lb1   = document.getElementById('midGainValue');
    const mu1   = document.getElementById('midMuteBtn');
    const sl2   = document.getElementById('sideGainSlider');
    const lb2   = document.getElementById('sideGainValue');
    const mu2   = document.getElementById('sideMuteBtn');
    const lrBtn = document.getElementById('lrModeToggle');

    if (!sl1 || !sl2) { debugLog('wireMSMatrix: slider non trovati', 'ERROR'); return; }

    const normToDb = (state) => {
        const range = getParamRange(state, GAIN_RANGE_FALLBACK);
        return normalisedToDb(state.getNormalisedValue(), range);
    };
    const dbToNorm = (db, state) => {
        const range = getParamRange(state, GAIN_RANGE_FALLBACK);
        return dbToNormalised(db, range);
    };

    const syncA = () => {
        const db = normToDb(ch.gainA);
        lb1.textContent = snapZero(db).toFixed(1) + ' dB';
        if (document.activeElement !== sl1)
            sl1.value = gainDbToSliderPos(db, -12, 12).toFixed(4);
    };
    const syncB = () => {
        const db = normToDb(ch.gainB);
        lb2.textContent = snapZero(db).toFixed(1) + ' dB';
        if (document.activeElement !== sl2)
            sl2.value = gainDbToSliderPos(db, -12, 12).toFixed(4);
    };
    const syncMuteA = () => {
        if (!ch.muteA) return;
        const v = ch.muteA.getValue();
        mu1?.classList.toggle('muted', v);
        mu1?.setAttribute('aria-checked', String(v));
    };
    const syncMuteB = () => {
        if (!ch.muteB) return;
        const v = ch.muteB.getValue();
        mu2?.classList.toggle('muted', v);
        mu2?.setAttribute('aria-checked', String(v));
    };

    // Registra i listener su TUTTI e 4 gli state: quando un param cambia (automazione DAW)
    // syncA/syncB leggono da ch.gainA/ch.gainB quindi aggiornano l'UI solo se attivo
    ch1Ms.valueChangedEvent.addListener(syncA);
    ch1Ms.propertiesChangedEvent.addListener(syncA);
    ch1Lr.valueChangedEvent.addListener(syncA);
    ch1Lr.propertiesChangedEvent.addListener(syncA);

    ch2Ms.valueChangedEvent.addListener(syncB);
    ch2Ms.propertiesChangedEvent.addListener(syncB);
    ch2Lr.valueChangedEvent.addListener(syncB);
    ch2Lr.propertiesChangedEvent.addListener(syncB);

    if (muCh1Ms) muCh1Ms.valueChangedEvent.addListener(syncMuteA);
    if (muCh1Lr) muCh1Lr.valueChangedEvent.addListener(syncMuteA);
    if (muCh2Ms) muCh2Ms.valueChangedEvent.addListener(syncMuteB);
    if (muCh2Lr) muCh2Lr.valueChangedEvent.addListener(syncMuteB);

    // Slider 1 (MID / LEFT)
    sl1.min = '0'; sl1.max = '1'; sl1.step = 'any';
    sl1.addEventListener('mousedown', () => ch.gainA.sliderDragStarted());
    sl1.addEventListener('touchstart', () => ch.gainA.sliderDragStarted(), { passive: true });
    sl1.addEventListener('mouseup',   () => ch.gainA.sliderDragEnded());
    sl1.addEventListener('touchend',  () => ch.gainA.sliderDragEnded());
    sl1.addEventListener('input', () => {
        const db = sliderPosToGainDb(parseFloat(sl1.value.replace(',', '.')), -12, 12);
        ch.gainA.setNormalisedValue(dbToNorm(db, ch.gainA));
        lb1.textContent = snapZero(db).toFixed(1) + ' dB';
    });
    sl1.addEventListener('dblclick', (e) => {
        e.preventDefault();
        ch.gainA.setNormalisedValue(dbToNorm(0, ch.gainA));
        syncA(); sl1.blur();
    });
    makeEditableLabel(lb1, -12, 12, (db) => {
        ch.gainA.setNormalisedValue(dbToNorm(db, ch.gainA));
        sl1.value = gainDbToSliderPos(db, -12, 12).toFixed(4);
        lb1.textContent = snapZero(db).toFixed(1) + ' dB';
    });

    // Slider 2 (SIDE / RIGHT)
    sl2.min = '0'; sl2.max = '1'; sl2.step = 'any';
    sl2.addEventListener('mousedown', () => ch.gainB.sliderDragStarted());
    sl2.addEventListener('touchstart', () => ch.gainB.sliderDragStarted(), { passive: true });
    sl2.addEventListener('mouseup',   () => ch.gainB.sliderDragEnded());
    sl2.addEventListener('touchend',  () => ch.gainB.sliderDragEnded());
    sl2.addEventListener('input', () => {
        const db = sliderPosToGainDb(parseFloat(sl2.value.replace(',', '.')), -12, 12);
        ch.gainB.setNormalisedValue(dbToNorm(db, ch.gainB));
        lb2.textContent = snapZero(db).toFixed(1) + ' dB';
    });
    sl2.addEventListener('dblclick', (e) => {
        e.preventDefault();
        ch.gainB.setNormalisedValue(dbToNorm(0, ch.gainB));
        syncB(); sl2.blur();
    });
    makeEditableLabel(lb2, -12, 12, (db) => {
        ch.gainB.setNormalisedValue(dbToNorm(db, ch.gainB));
        sl2.value = gainDbToSliderPos(db, -12, 12).toFixed(4);
        lb2.textContent = snapZero(db).toFixed(1) + ' dB';
    });

    // Mute buttons
    if (mu1) mu1.addEventListener('click', () => { if (ch.muteA) { ch.muteA.setValue(!ch.muteA.getValue()); syncMuteA(); } });
    if (mu2) mu2.addEventListener('click', () => { if (ch.muteB) { ch.muteB.setValue(!ch.muteB.getValue()); syncMuteB(); } });

    let _lrActive = false;
    if (lrBtn) {
        lrBtn.addEventListener('click', () => {
            _lrActive = !_lrActive;

            if (_lrActive) {
                ch.gainA = ch1Lr; ch.gainB = ch2Lr;
                ch.muteA = muCh1Lr; ch.muteB = muCh2Lr;
                if (mu1) mu1.textContent = 'Left';
                if (mu2) mu2.textContent = 'Right';
                lrBtn.textContent = 'L/R';
                lrBtn.classList.add('active');
            } else {
                ch.gainA = ch1Ms; ch.gainB = ch2Ms;
                ch.muteA = muCh1Ms; ch.muteB = muCh2Ms;
                if (mu1) mu1.textContent = 'Mid';
                if (mu2) mu2.textContent = 'Side';
                lrBtn.textContent = 'M/S';
                lrBtn.classList.remove('active');
            }

            syncA(); syncB();
            syncMuteA(); syncMuteB();
        });
    }

    const resetGainOnce = (state) => {
        let done = false;
        const tryReset = () => {
            if (done) return;
            const props = state.properties;
            if (!props || !(Number(props.end) - Number(props.start) > PARAM_RANGE_MIN_SPAN)) return;
            done = true;
            try {
                state.setNormalisedValue(dbToNorm(0, state));
            } catch (e) {
                debugLog('wireMSMatrix: reset gain fallito: ' + e.message, 'WARN');
            }
        };
        state.propertiesChangedEvent.addListener(tryReset);
        tryReset();
    };
    resetGainOnce(ch1Ms);
    resetGainOnce(ch2Ms);
    resetGainOnce(ch1Lr);
    resetGainOnce(ch2Lr);
    if (muCh1Lr) { try { muCh1Lr.setValue(false); } catch (e) {} }
    if (muCh2Lr) { try { muCh2Lr.setValue(false); } catch (e) {} }

    // Sync iniziale UI
    setTimeout(() => {
        syncA(); syncB();
        syncMuteA(); syncMuteB();
    }, 50);

    debugLog('✓ wireMSMatrix completato con toggle L/R', 'SUCCESS');
}

// Popola le scale dBFS dei meter: etichette posizionate con la stessa
// mappatura lineare di dbToPercent (METER_MIN_DB → 0%, METER_MAX_DB → 100%)
function buildMeterScales() {
    const marks = [0, -3, -6, -9, -12, -18, -24, -30, -40, -50, -60];
    document.querySelectorAll('.meter-scale').forEach((el) => {
        el.innerHTML = '';
        for (const db of marks) {
            if (db < METER_MIN_DB || db > METER_MAX_DB) continue;
            const s = document.createElement('span');
            s.textContent = String(db);
            s.style.bottom = ((db - METER_MIN_DB) / (METER_MAX_DB - METER_MIN_DB) * 100) + '%';
            el.appendChild(s);
        }
    });
}

function wireMeters() {
    debugLog('wireMeters: Starting...');
    buildMeterScales();

    // Canvas loop parte sempre — indipendente dalla disponibilità del backend
    setupVectorscopeZoom();
    setupSpectrumControls();
    drawVectorscope();
    drawSpectrum();
    _drawSegmentMeter('balanceCanvas', 0, 'balance');
    _drawSegmentMeter('correlationCanvas', 0, 'correlation');
    setupCanvasResizeObserver();
    requestAnimationFrame(() => requestAnimationFrame(updateVsSize));

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
                'inputClip', 'input',
                data.inPeakL, data.inPeakR,
                getHeldPeak('inL', data.inPeakL), getHeldPeak('inR', data.inPeakR)
            );
            setStereoMeter(
                'outputFillL', 'outputFillR', 'outputPeakL', 'outputPeakR',
                'outputRmsL', 'outputRmsR', 'outputPeakValL', 'outputPeakValR',
                'outputClip', 'output',
                data.outPeakL, data.outPeakR,
                getHeldPeak('outL', data.outPeakL), getHeldPeak('outR', data.outPeakR)
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
            updateLufsUI(data);

            if (data.correlationValue !== undefined) {
                updateCorrelationUI(data.correlationValue);
            }

            updateBalanceUI(data.outL, data.outR);
            
            if (data.scopeBatchX && data.scopeBatchX.length > 0)
                _vsPendingBatch = { bx: data.scopeBatchX, by: data.scopeBatchY };

            // Segui il conteggio confermato dal C++: la lunghezza degli array combacia
            if (typeof data.specBandCount === 'number' && data.specBandCount !== _specBands)
                applySpecBandCount(data.specBandCount);

            if (data.specL && data.specL.length === _specBands) {
                for (let b = 0; b < _specBands; b++) {
                    _specTargetL[b] = data.specL[b];
                    _specTargetR[b] = data.specR[b];
                }
                if (data.specMid && data.specMid.length === _specBands) {
                    for (let b = 0; b < _specBands; b++) {
                        _specTargetMid[b]  = data.specMid[b];
                        _specTargetSide[b] = data.specSide[b];
                    }
                }

                if (_specAvgOn) {
                    _specAvgCount++;
                    const hasMs = data.specMid && data.specMid.length === _specBands;
                    for (let b = 0; b < _specBands; b++) {
                        const pL = Math.pow(10, data.specL[b] / 10);
                        const pR = Math.pow(10, data.specR[b] / 10);
                        _specAvgL[b] += (pL - _specAvgL[b]) / _specAvgCount;
                        _specAvgR[b] += (pR - _specAvgR[b]) / _specAvgCount;
                        if (hasMs) {
                            const pM = Math.pow(10, data.specMid[b]  / 10);
                            const pS = Math.pow(10, data.specSide[b] / 10);
                            _specAvgMid[b]  += (pM - _specAvgMid[b])  / _specAvgCount;
                            _specAvgSide[b] += (pS - _specAvgSide[b]) / _specAvgCount;
                        }
                    }
                }
            }
        });
        debugLog('✓ meterLevels event listener added', 'SUCCESS');
    } catch (e) {
        debugLog(`✗ Error adding meterLevels listener: ${e.message}`, 'ERROR');
    }
}

function changeVsZoom(direction) {
    _vsZoom = direction > 0
        ? Math.min(VS_ZOOM_MAX, _vsZoom * VS_ZOOM_STEP)
        : Math.max(VS_ZOOM_MIN, _vsZoom / VS_ZOOM_STEP);

    // Evita il segmento spurio tra la vecchia e la nuova scala in modalità linea
    _vsLastX = null;
    _vsLastY = null;
    saveUiState({ vsZoom: _vsZoom });
}

function setupVectorscopeZoom() {
    _vsOverlayCanvas = document.getElementById('vectorscopeOverlay');

    const vsCanvas = document.getElementById('vectorscopeCanvas');
    const dims = setupHiDPICanvas(vsCanvas);
    _vsW = dims.w;
    _vsH = dims.h;
    setupHiDPICanvas(_vsOverlayCanvas);

    const saved = loadUiState();
    if (typeof saved.vsZoom === 'number')
        _vsZoom = Math.max(VS_ZOOM_MIN, Math.min(VS_ZOOM_MAX, saved.vsZoom));
    if (saved.vsOverlay === false)
        _vsShowOverlay = false;

    document.getElementById('vsZoomIn').addEventListener('click', () => changeVsZoom(1));
    document.getElementById('vsZoomOut').addEventListener('click', () => changeVsZoom(-1));

    const btnOverlay = document.getElementById('vsOverlayToggle');
    btnOverlay.style.opacity = _vsShowOverlay ? '1' : '0.35';
    btnOverlay.addEventListener('click', () => {
        _vsShowOverlay = !_vsShowOverlay;
        btnOverlay.style.opacity = _vsShowOverlay ? '1' : '0.35';
        saveUiState({ vsOverlay: _vsShowOverlay });
    });

    // Toggle modalità traccia: nuvola di puntini ↔ linea continua + cursore
    if (saved.vsDots === false)
        _vsDotsMode = false;

    const btnMode = document.getElementById('vsModeToggle');
    const syncModeBtn = () => { btnMode.textContent = _vsDotsMode ? '∴' : '∿'; };
    syncModeBtn();
    btnMode.addEventListener('click', () => {
        _vsDotsMode = !_vsDotsMode;
        _vsLastX = null;
        _vsLastY = null;
        syncModeBtn();
        saveUiState({ vsDots: _vsDotsMode });
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
    setupLufs();

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
    setupLufs();

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
