#pragma once

#include "RNBO.h"
#include "RNBO_Utils.h"
#include "RNBO_JuceAudioProcessor.h"
#include "RNBO_BinaryData.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <json/json.hpp>

class CustomAudioProcessor : public RNBO::JuceAudioProcessor
{
public:
    static CustomAudioProcessor* CreateDefault();

    CustomAudioProcessor (const nlohmann::json& patcher_desc,
                          const nlohmann::json& presets,
                          const RNBO::BinaryData& data);

    ~CustomAudioProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;

    void handleMessageEvent (const RNBO::MessageEvent& event) override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    struct MeterLevels
    {
        std::atomic<float> inL  { -100.f };
        std::atomic<float> inR  { -100.f };
        std::atomic<float> outL { -100.f };
        std::atomic<float> outR { -100.f };

        std::atomic<float> inPeakL  { -100.f };
        std::atomic<float> inPeakR  { -100.f };
        std::atomic<float> outPeakL { -100.f };
        std::atomic<float> outPeakR { -100.f };

        std::atomic<float> delayTime { 0.f };

        std::atomic<float> correlationValue { 0.f };

        // LUFS (ITU-R BS.1770): Momentary (400 ms), Short-term (3 s), Integrated (gated)
        std::atomic<float> lufsMomentary  { -100.f };
        std::atomic<float> lufsShortTerm  { -100.f };
        std::atomic<float> lufsIntegrated { -100.f };
    };

    MeterLevels meterLevels;

    // Reset dell'Integrated LUFS (chiamato dal tasto RESET via native function)
    void resetLufsIntegrated();

    // Cambia il numero di bande dello spettro (96/192/256), richiesto dalla UI.
    // Applicato sul thread audio al prossimo blocco. n viene limitato a [32, kSpecBandsMax].
    void setSpecBandCount (int n);

    static constexpr int kScopeBufferSize = 4096;

    juce::AbstractFifo _scopeFifo { kScopeBufferSize };
    float _scopeBufX[kScopeBufferSize] = {};
    float _scopeBufY[kScopeBufferSize] = {};

    static constexpr int kFftOrder    = 11;
    static constexpr int kFftSize     = 1 << kFftOrder;     // 2048 pt @ full rate (medie/alte)
    // Hop = N/4 (75% overlap): raddoppia il frame rate delle medie/alte (~12 ms invece di
    // ~23 ms) per più reattività. Costa 2× FFT sul path alto (ora fuori dal thread audio).
    static constexpr int kFftHop      = kFftSize / 4;
    // Path basse: il segnale viene filtrato (anti-alias) e decimato di kSpecDecim prima
    // della FFT. Una FFT da 1024 pt a sr/16 copre la stessa finestra temporale (~372 ms a
    // 44100 Hz) e la stessa risoluzione (~2.7 Hz/bin) di una 16384 pt a full rate, ma costa
    // ~16× meno → hop piccolo e frame rate alto (~23 ms) a CPU molto ridotta.
    static constexpr int kSpecDecim   = 16;
    static constexpr int kFftLowOrder = 10;                 // 1024 pt @ rate decimato
    static constexpr int kFftLowSize  = 1 << kFftLowOrder;
    static constexpr int kFftLowHop   = kFftLowSize / 16;   // 64 camp. decimati ≈ 23 ms
    static constexpr int kSpecBands    = 96;
    static constexpr int kSpecBandsMax = 256;
    // Crossover a crossfade invece che a gradino: sotto kSpecXfadeLoHz le bande vengono
    // SOLO dalla FFT fine decimata (alta risoluzione, ma temporalmente "statica" per via
    // della finestra lunga ~372 ms); sopra kSpecXfadeHiHz SOLO dalla FFT corta (vivace).
    // In mezzo le due stime vengono fuse con peso graduale → niente linea netta e la
    // risoluzione fine "arriva" più in alto (fino a ~900 Hz) rispetto al taglio secco a 500.
    // Limite superiore vincolato dalla Nyquist decimata (~1378 Hz) e dal filtro anti-alias
    // (passabanda piatto fin ~900 Hz): oltre, il path basso non è più affidabile.
    static constexpr float kSpecXfadeLoHz = 500.0f;
    static constexpr float kSpecXfadeHiHz = 900.0f;

    float _specMagL[kSpecBandsMax] = {};
    float _specMagR[kSpecBandsMax] = {};
    float _specMagMid[kSpecBandsMax]  = {};
    float _specMagSide[kSpecBandsMax] = {};

    // Ultima magnitudine grezza (dB, non smussata) calcolata da ciascun path, per fondere
    // i due path nelle bande di transizione. Il path basso si aggiorna più di rado ma
    // cambia lentamente, quindi il suo ultimo valore è sempre buono per la fusione.
    float _specRawLowL[kSpecBandsMax]    = {};
    float _specRawLowR[kSpecBandsMax]    = {};
    float _specRawLowMid[kSpecBandsMax]  = {};
    float _specRawLowSide[kSpecBandsMax] = {};
    float _specRawHighL[kSpecBandsMax]    = {};
    float _specRawHighR[kSpecBandsMax]    = {};
    float _specRawHighMid[kSpecBandsMax]  = {};
    float _specRawHighSide[kSpecBandsMax] = {};

    std::atomic<int>      _specBandCount { kSpecBands };   // bande attive correnti
    std::atomic<bool>     _specNewData { false };
    juce::CriticalSection _specLock;

private:
    // Frame dello spettro catturato dal thread audio e consumato dal thread spettro.
    // Tiene una copia linearizzata (ordine cronologico) di L/R alla risoluzione del path.
    struct SpecFrame
    {
        int   len      = 0;        // kFftSize (path alto) o kFftLowSize (path basso)
        bool  lowBands = false;
        float L[kFftSize] = {};
        float R[kFftSize] = {};
    };

    void measurePeaks (const juce::AudioBuffer<float>& buffer, bool isOutput);
    void fillScopeFifo (const juce::AudioBuffer<float>& buffer);
    void feedSpectrum (const juce::AudioBuffer<float>& buffer);
    void enqueueSpecFrame (bool lowBands);              // thread audio: cattura un frame e sveglia il consumer
    void specThreadRun();                               // loop del thread spettro (FFT fuori dal callback audio)
    void computeSpectrumFrame (const SpecFrame& frame); // thread spettro: FFT + bande + smoothing
    void rebuildSpecBands (int count);          // ricalcola la band-map (thread spettro)
    void prepareDecimationFilter (double sampleRate);   // coeff. Butterworth anti-alias per il path basse

    // ===== LUFS (ITU-R BS.1770) =====
    void prepareLufs (double sampleRate);
    void feedLufs (const juce::AudioBuffer<float>& buffer);
    void pushLufsSubBlock();      // chiude un sotto-blocco da 100 ms
    void updateLufsIntegrated();  // ricalcola l'Integrated dall'istogramma con gating

    // Biquad K-weighting in cascata (stadio 1 shelf + stadio 2 high-pass), per canale.
    // Direct Form I: serve x[n-1], x[n-2], y[n-1], y[n-2] per stadio/canale.
    struct Biquad { double b0=1, b1=0, b2=0, a1=0, a2=0; };
    Biquad _kStage1, _kStage2;
    struct BiquadState { double x1=0, x2=0, y1=0, y2=0; };
    BiquadState _kS1[2], _kS2[2];   // [canale]

    // Sotto-blocchi da 100 ms: accumulo somma dei quadrati K-weighted per canale
    double _lufsSubSumL = 0.0, _lufsSubSumR = 0.0;
    int    _lufsSubCount = 0;
    int    _lufsSubLen   = 4800;    // campioni in 100 ms (ricalcolato in prepare)

    // Ring dei mean-square per canale degli ultimi sotto-blocchi (per M e S)
    static constexpr int kLufsMaxBlocks = 30;   // 3 s = 30 × 100 ms
    double _lufsBlockL[kLufsMaxBlocks] = {};
    double _lufsBlockR[kLufsMaxBlocks] = {};
    int    _lufsBlockPos   = 0;
    int    _lufsBlockFilled = 0;

    // Istogramma dei gating block (400 ms) per l'Integrated, da -70 a +5 LUFS, passo 0.1
    static constexpr int   kLufsHistBins = 751;
    static constexpr double kLufsHistMin = -70.0;
    static constexpr double kLufsHistStep = 0.1;
    int _lufsHist[kLufsHistBins] = {};

    float _peakDecayDbPerBlock = 0.05f;
    int   _scopeDownsample     = 4;

    juce::dsp::FFT _fft    { kFftOrder };
    juce::dsp::FFT _fftLow { kFftLowOrder };
    float _hannWindow[kFftSize]       = {};
    float _hannWindowLow[kFftLowSize] = {};

    // Ring per il path medie/alte (full rate, finestra = kFftSize). Solo L/R: gli
    // spettri Mid/Side si ricavano in frequenza in computeSpectrumFrame.
    float _rawRingL[kFftSize] = {};
    float _rawRingR[kFftSize] = {};
    int   _rawRingPos  = 0;
    int   _fftHopCount = 0;

    // Ring per il path basse (rate decimato sr/kSpecDecim, finestra = kFftLowSize)
    float _decRingL[kFftLowSize] = {};
    float _decRingR[kFftLowSize] = {};
    int   _decRingPos     = 0;
    int   _fftLowHopCount = 0;
    int   _decimCounter   = 0;   // campioni raw accumulati verso il prossimo campione decimato

    // Filtro anti-aliasing (Butterworth 6° ordine = 3 biquad) applicato prima della
    // decimazione; coefficienti condivisi, stato indipendente per canale L/R.
    static constexpr int kAaStages = 3;
    Biquad      _aaCoef[kAaStages];
    BiquadState _aaState[2][kAaStages];

    // Due buffer di lavoro FFT: L in _fftWorkBuf, R in _fftWorkBufR, così i bin di
    // entrambi sono disponibili insieme per derivare Mid/Side.
    float _fftWorkBuf[kFftSize * 2]  = {};
    float _fftWorkBufR[kFftSize * 2] = {};

    // Range di bin per ciascun path. Una banda nella zona di transizione ha entrambi
    // validi; sotto la zona solo Low, sopra solo High. _bandBlend pesa la fusione:
    // 0 = solo path basso (fine), 1 = solo path alto (vivace), in mezzo crossfade.
    int   _bandLoLow[kSpecBandsMax]  = {};
    int   _bandHiLow[kSpecBandsMax]  = {};
    int   _bandLoHigh[kSpecBandsMax] = {};
    int   _bandHiHigh[kSpecBandsMax] = {};
    float _bandBlend[kSpecBandsMax]  = {};

    // Smoothing per-banda (attacco/decadimento): graduato per frequenza così la risposta
    // varia con continuità e la cucitura tra path decimato e path corto è impercettibile.
    float _bandAtk[kSpecBandsMax] = {};
    float _bandDcy[kSpecBandsMax] = {};

    double _specSampleRate = 48000.0;        // memorizzato per ricalcolare la band-map
    std::atomic<int> _pendingSpecBandCount { 0 };   // 0 = nessun cambio in attesa

    // ===== Handoff spettro audio → thread dedicato =====
    // Il thread audio cattura i frame (enqueueSpecFrame) in una coda SPSC lock-free;
    // il thread spettro li consuma e fa la FFT fuori dal callback real-time.
    static constexpr int kSpecFrameSlots = 16;
    SpecFrame         _specFrames[kSpecFrameSlots];
    juce::AbstractFifo _specFrameFifo { kSpecFrameSlots };

    class SpecThread : public juce::Thread
    {
    public:
        explicit SpecThread (CustomAudioProcessor& owner)
            : juce::Thread ("SimpleMeter Spectrum"), _owner (owner) {}
        void run() override { _owner.specThreadRun(); }
    private:
        CustomAudioProcessor& _owner;
    };
    SpecThread _specThread { *this };

    static constexpr RNBO::MessageTag tagInRmsL  = RNBO::TAG ("in_rms_L");
    static constexpr RNBO::MessageTag tagInRmsR  = RNBO::TAG ("in_rms_R");
    static constexpr RNBO::MessageTag tagOutRmsL = RNBO::TAG ("out_rms_L");
    static constexpr RNBO::MessageTag tagOutRmsR = RNBO::TAG ("out_rms_R");

    static constexpr RNBO::MessageTag tagDelayTime = RNBO::TAG ("delay_time");

    static constexpr RNBO::MessageTag tagCorrelationValue = RNBO::TAG ("correlation_value");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioProcessor)
};
