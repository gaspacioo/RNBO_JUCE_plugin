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

    static constexpr int kScopeBufferSize = 4096;

    juce::AbstractFifo _scopeFifo { kScopeBufferSize };
    float _scopeBufX[kScopeBufferSize] = {};
    float _scopeBufY[kScopeBufferSize] = {};

    static constexpr int kFftOrder    = 11;
    static constexpr int kFftSize     = 1 << kFftOrder;
    static constexpr int kFftHop      = kFftSize / 2;
    static constexpr int kFftLowOrder = 13;
    static constexpr int kFftLowSize  = 1 << kFftLowOrder;
    static constexpr int kFftLowHop   = kFftLowSize / 4;
    static constexpr int kSpecBands   = 96;
    static constexpr float kSpecLowCrossHz = 300.0f;

    float _specMagL[kSpecBands] = {};
    float _specMagR[kSpecBands] = {};
    float _specMagMid[kSpecBands]  = {};
    float _specMagSide[kSpecBands] = {};
    std::atomic<bool>     _specNewData { false };
    juce::CriticalSection _specLock;

private:
    void measurePeaks (const juce::AudioBuffer<float>& buffer, bool isOutput);
    void fillScopeFifo (const juce::AudioBuffer<float>& buffer);
    void feedSpectrum (const juce::AudioBuffer<float>& buffer);
    void computeSpectrumFrame (bool lowBands);

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

    float _fftRingL[kFftLowSize] = {};
    float _fftRingR[kFftLowSize] = {};
    float _fftRingMid[kFftLowSize]  = {};
    float _fftRingSide[kFftLowSize] = {};
    int   _fftRingPos    = 0;
    int   _fftHopCount   = 0;
    int   _fftLowHopCount = 0;
    float _fftWorkBuf[kFftLowSize * 2] = {};

    int  _bandLo[kSpecBands]     = {};
    int  _bandHi[kSpecBands]     = {};
    bool _bandUseLow[kSpecBands] = {};

    static constexpr RNBO::MessageTag tagInRmsL  = RNBO::TAG ("in_rms_L");
    static constexpr RNBO::MessageTag tagInRmsR  = RNBO::TAG ("in_rms_R");
    static constexpr RNBO::MessageTag tagOutRmsL = RNBO::TAG ("out_rms_L");
    static constexpr RNBO::MessageTag tagOutRmsR = RNBO::TAG ("out_rms_R");

    static constexpr RNBO::MessageTag tagDelayTime = RNBO::TAG ("delay_time");

    static constexpr RNBO::MessageTag tagCorrelationValue = RNBO::TAG ("correlation_value");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioProcessor)
};
