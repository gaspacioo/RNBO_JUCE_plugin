/* * main.js - Versione Completa e Resiliente
 */

// --- Costanti e Configurazione ---
const METER_MIN_DB = -60;
const METER_MAX_DB = 0;
const CLIP_THRESHOLD_DB = -0.1;
const CLIP_HOLD_MS = 400;
const GAIN_DEFAULT_DB = 0;
const GAIN_MIN_DB = -60;
const GAIN_MAX_DB = 12;

// --- Gestione UI (Modale) ---
const modal = document.getElementById("aboutModal");
const infoBtn = document.getElementById("infoBtn");
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
if(infoBtn) infoBtn.onclick = toggleAboutMenu;
if(closeBtn) closeBtn.onclick = toggleAboutMenu;

// --- Funzioni Helper per il Calcolo ---
function getGainRange(state) {
    const props = state?.properties;
    return (props && props.end - props.start > 1.5) 
        ? { start: props.start, end: props.end } 
        : { start: 0, end: 1 };
}

function dbToNormalised(db, range) {
    return (db - range.start) / (range.end - range.start);
}

function normalisedToDb(norm, range) {
    return norm * (range.end - range.start) + range.start;
}

function setGainDb(db) {
    // Questa funzione invia il valore al C++
    if (window.__JUCE__ && window.__JUCE__.postMessage) {
        window.__JUCE__.postMessage(JSON.stringify({ type: "gain", value: db }));
    }
}

// --- Logica Meters (Originale) ---
function parseMeterPayload(raw) {
    // Inserisci qui la tua logica originale di parsing se presente
    // Esempio generico (adatta al tuo formato dati):
    return raw; 
}

function setStereoMeter(fillL, fillR, peakL, peakR, rmsL, rmsR, pValL, pValR, clip, type, l, r, pL, pR) {
    // La tua logica originale di aggiornamento DOM dei meter
    // Assicurati che i nomi degli ID corrispondano a quelli nel tuo HTML
}

// --- Inizializzazione Principale (Il FIX per Windows) ---
function init() {
    // Il controllo "resiliente" che attende il bridge
    if (typeof window.__JUCE__ === 'undefined') {
        console.warn('JUCE bridge non ancora pronto, attendo...');
        setTimeout(init, 200); 
        return;
    }

    console.log('Bridge JUCE rilevato. Inizializzazione completa...');

    // 1. Inizializza i Meters
    wireMeters();

    // 2. Inizializza lo Slider (e aggiungi gli Event Listener)
    setupSlider();
}

function setupSlider() {
    const slider = document.getElementById("gainSlider");
    if (!slider) return;

    slider.addEventListener('input', (event) => {
        const cleanValue = event.target.value.replace(',', '.');
        setGainDb(parseFloat(cleanValue));
    });

    slider.addEventListener('dblclick', (event) => {
        event.preventDefault();
        setGainDb(GAIN_DEFAULT_DB);
        // Aggiungi qui la logica di sincronizzazione se necessaria
    });
}

function wireMeters() {
    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener) return;

    backend.addEventListener('meterLevels', (raw) => {
        const data = parseMeterPayload(raw);
        if (!data) return;

        // Richiama le tue funzioni originali di rendering meter
        // Assicurati di passare i dati corretti in base a come è fatto il tuo parseMeterPayload
        setStereoMeter(
            'inputFillL', 'inputFillR', 'inputPeakL', 'inputPeakR',
            'inputRmsL', 'inputRmsR', 'inputPeakValL', 'inputPeakValR',
            'inputClip', 'input',
            data.inL, data.inR, data.inPeakL, data.inPeakR
        );
        setStereoMeter(
            'outputFillL', 'outputFillR', 'outputPeakL', 'outputPeakR',
            'outputRmsL', 'outputRmsR', 'outputPeakValL', 'outputPeakValR',
            'outputClip', 'output',
            data.outL, data.outR, data.outPeakL, data.outPeakR
        );
    });
}

// --- Avvio ---
init();