/*
  Main.js - Versione Resiliente
  Questa versione attende l'inizializzazione del bridge JUCE prima di avviare l'UI.
*/

// --- Costanti ---
const GAIN_DEFAULT_DB = 0;
const GAIN_STEP_DB = 0.1;
const METER_MIN_DB = -60;
const METER_MAX_DB = 0;

// --- Elementi DOM ---
const modal = document.getElementById("aboutModal");
const infoBtn = document.getElementById("infoBtn");
const closeBtn = document.getElementById("closeBtn");
const slider = document.getElementById("gainSlider"); // Assicurati che l'ID sia corretto
const display = document.getElementById("gainDisplay");

// --- Inizializzazione Resiliente ---
function init() {
    // Verifica se il bridge è pronto
    if (typeof window.__JUCE__ === 'undefined' || typeof window.__JUCE__.backend === 'undefined') {
        console.warn('JUCE bridge non ancora pronto, riprovo tra 100ms...');
        setTimeout(init, 100); 
        return;
    }

    console.log('Bridge JUCE rilevato correttamente. Avvio UI...');
    
    // Ora che siamo sicuri che il bridge è pronto, avviamo tutto il resto
    setupUI();
    wireMeters();
}

// --- Logica UI ---
function setupUI() {
    // Gestione Modale
    if (infoBtn && closeBtn) {
        infoBtn.onclick = toggleAboutMenu;
        closeBtn.onclick = toggleAboutMenu;
    }

    // Inizializzazione Slider (Esempio logica)
    if (slider) {
        slider.addEventListener('input', () => {
            const val = parseFloat(slider.value);
            sendValueToNative("sliderChanged", val);
        });
    }
}

function toggleAboutMenu() {
    const isModalOpen = window.getComputedStyle(modal).display === "flex";
    modal.style.display = isModalOpen ? "none" : "flex";
    infoBtn.classList.toggle("active");
}

// --- Comunicazione Sicura con C++ ---
function sendValueToNative(type, value) {
    if (window.__JUCE__ && window.__JUCE__.postMessage) {
        // Obbligatorio su Windows: stringify del JSON
        const payload = JSON.stringify({ type: type, value: value });
        window.__JUCE__.postMessage(payload);
    }
}

// --- Meters (Preservati) ---
function wireMeters() {
    const backend = window.__JUCE__?.backend;
    if (!backend?.addEventListener) return;

    backend.addEventListener('meterLevels', (raw) => {
        // Qui la tua logica originale di parsing meter
        // Assicurati che 'parseMeterPayload' sia definito o incluso
        try {
            const data = typeof parseMeterPayload === 'function' ? parseMeterPayload(raw) : null;
            if (data) {
                // Esempio chiamate setStereoMeter...
                // setStereoMeter(..., data.inL, data.inR, ...);
            }
        } catch (e) {
            console.error("Errore parsing meter:", e);
        }
    });
}

// Avvio esecuzione
init();