/*
import { getSliderState } from 'juce-framework-frontend';

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
        // Se è chiuso, lo apriamo
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

function parseMeterPayload(data) {
    if (!data)
        return null;
    if (typeof data === 'string') {
        try {
            return JSON.parse(data);
        } catch {
            return null;
        }
    }
    return data;
}

function wireGain() {
    const slider = document.getElementById('gainSlider');
    const label = document.getElementById('gainValue');
    if (!slider || !label)
        return;

    const state = getSliderState('gain');
    if (!state)
        return;

    slider.min = String(GAIN_MIN_DB);
    slider.max = String(GAIN_MAX_DB);
    slider.step = String(GAIN_STEP_DB);

    const sync = () => {
        const range = getGainRange(state);
        const db = clampGainDb(state.getScaledValue());
        slider.value = db.toFixed(1);
        label.textContent = db.toFixed(1) + ' dB';
    };

    const setGainDb = (db) => {
        const range = getGainRange(state);
        state.setNormalisedValue(dbToNormalised(db, range));
        label.textContent = clampGainDb(db).toFixed(1) + ' dB';
        slider.value = clampGainDb(db).toFixed(1);
    };

    state.valueChangedEvent.addListener(sync);
    state.propertiesChangedEvent.addListener(sync);

    //sync();

    slider.addEventListener('mousedown', () => state.sliderDragStarted());
    slider.addEventListener('touchstart', () => state.sliderDragStarted(), { passive: true });
    slider.addEventListener('mouseup', () => state.sliderDragEnded());
    slider.addEventListener('touchend', () => state.sliderDragEnded());
    slider.addEventListener('input', () => {
        setGainDb(parseFloat(slider.value));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        setGainDb(GAIN_DEFAULT_DB);
    });
}

function wireMeters() {
    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener)
        return;

    backend.addEventListener('meterLevels', (raw) => {
        const data = parseMeterPayload(raw);
        if (!data)
            return;

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
}

function init() {
    if (typeof window.__JUCE__ === 'undefined') {
        console.warn('JUCE native integration is not available.');
        return;
    }

    wireGain();
    wireMeters();
}

if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', init);
else
    init();
*/
import { getSliderState } from 'juce-framework-frontend';

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
    const slider = document.getElementById('gainSlider');
    const label = document.getElementById('gainValue');
    if (!slider || !label)
        return;

    const state = getSliderState('gain');
    if (!state)
        return;

    slider.min = String(GAIN_MIN_DB);
    slider.max = String(GAIN_MAX_DB);
    slider.step = String(GAIN_STEP_DB);

    const sync = () => {
        const range = getGainRange(state);
        
        // Leggiamo il valore normalizzato pulito dal C++ e lo convertiamo localmente in dB reali
        const norm = state.getNormalisedValue();
        const db = clampGainDb(normalisedToDb(norm, range));
        
        label.textContent = db.toFixed(1) + ' dB';
        
        // Aggiorna lo slider grafico SOLO se l'utente non lo sta attivamente trascinando
        if (document.activeElement !== slider) {
            slider.value = db.toFixed(1).replace(',', '.');
        }
    };

    const setGainDb = (db) => {
        const range = getGainRange(state);
        // Invia il corretto valore normalizzato (0-1) calcolato in base al range reale
        state.setNormalisedValue(dbToNormalised(db, range));
        
        // Aggiorna la label testuale istantaneamente per dare fluidità
        label.textContent = clampGainDb(db).toFixed(1) + ' dB';
    };

    state.valueChangedEvent.addListener(sync);
    state.propertiesChangedEvent.addListener(sync);

    sync();

    slider.addEventListener('mousedown', () => state.sliderDragStarted());
    slider.addEventListener('touchstart', () => state.sliderDragStarted(), { passive: true });
    slider.addEventListener('mouseup', () => state.sliderDragEnded());
    slider.addEventListener('touchend', () => state.sliderDragEnded());
    
    slider.addEventListener('input', () => {
        // Forza la sostituzione della virgola con il punto prima di convertire in numero float
        const cleanValue = slider.value.replace(',', '.');
        setGainDb(parseFloat(cleanValue));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        setGainDb(GAIN_DEFAULT_DB);
        sync(); // Sincronizza immediatamente la posizione dopo il reset
    });
}

function wireMeters() {
    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener)
        return;

    backend.addEventListener('meterLevels', (raw) => {
        const data = parseMeterPayload(raw);
        if (!data)
            return;

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
}

function init() {
    if (typeof window.__JUCE__ === 'undefined') {
        console.warn('JUCE native integration is not available.');
        return;
    }

    wireGain();
    wireMeters();
}

if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', init);
else
    init();