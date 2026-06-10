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
    };

    MeterLevels meterLevels;

    // _scopeDownsample viene calcolato in prepareToPlay per mantenere ~200 pt/frame
    static constexpr int kScopeBufferSize = 4096;

    juce::AbstractFifo _scopeFifo { kScopeBufferSize };
    float _scopeBufX[kScopeBufferSize] = {};
    float _scopeBufY[kScopeBufferSize] = {};

    // Spettro multi-risoluzione, bande log-spaced 20 Hz – 20 kHz:
    // FFT corta (2048, hop 50%) per medie/alte, FFT lunga (8192, hop 25%)
    // per le bande sotto kSpecLowCrossHz dove servono più bin per ottava
    static constexpr int kFftOrder    = 11;
    static constexpr int kFftSize     = 1 << kFftOrder;
    static constexpr int kFftHop      = kFftSize / 2;
    static constexpr int kFftLowOrder = 13;
    static constexpr int kFftLowSize  = 1 << kFftLowOrder;
    static constexpr int kFftLowHop   = kFftLowSize / 4;
    static constexpr int kSpecBands   = 96;
    static constexpr float kSpecLowCrossHz = 300.0f;

    // Magnitudini in dB per banda, protette da _specLock (il timer della UI legge da qui)
    float _specMagL[kSpecBands] = {};
    float _specMagR[kSpecBands] = {};
    std::atomic<bool>     _specNewData { false };
    juce::CriticalSection _specLock;

private:
    void measurePeaks (const juce::AudioBuffer<float>& buffer, bool isOutput);
    void fillScopeFifo (const juce::AudioBuffer<float>& buffer);
    void feedSpectrum (const juce::AudioBuffer<float>& buffer);
    void computeSpectrumFrame (bool lowBands);

    float _peakDecayDbPerBlock = 0.05f;
    int   _scopeDownsample     = 4;

    juce::dsp::FFT _fft    { kFftOrder };
    juce::dsp::FFT _fftLow { kFftLowOrder };
    float _hannWindow[kFftSize]       = {};
    float _hannWindowLow[kFftLowSize] = {};

    // Ring buffer condiviso, dimensionato sulla FFT lunga:
    // la FFT corta legge solo gli ultimi kFftSize campioni
    float _fftRingL[kFftLowSize] = {};
    float _fftRingR[kFftLowSize] = {};
    int   _fftRingPos    = 0;
    int   _fftHopCount   = 0;
    int   _fftLowHopCount = 0;
    float _fftWorkBuf[kFftLowSize * 2] = {};

    // Range di bin FFT (inclusivo) coperto da ogni banda di display,
    // riferito alla FFT lunga se _bandUseLow[b] è true
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
