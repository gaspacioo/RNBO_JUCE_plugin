#pragma once

#include "RNBO.h"
#include "RNBO_Utils.h"
#include "RNBO_JuceAudioProcessor.h"
#include "RNBO_BinaryData.h"
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

private:
    void measurePeaks (const juce::AudioBuffer<float>& buffer, bool isOutput);
    void fillScopeFifo (const juce::AudioBuffer<float>& buffer);

    float _peakDecayDbPerBlock = 0.05f;
    int   _scopeDownsample     = 4;

    static constexpr RNBO::MessageTag tagInRmsL  = RNBO::TAG ("in_rms_L");
    static constexpr RNBO::MessageTag tagInRmsR  = RNBO::TAG ("in_rms_R");
    static constexpr RNBO::MessageTag tagOutRmsL = RNBO::TAG ("out_rms_L");
    static constexpr RNBO::MessageTag tagOutRmsR = RNBO::TAG ("out_rms_R");

    static constexpr RNBO::MessageTag tagDelayTime = RNBO::TAG ("delay_time");

    static constexpr RNBO::MessageTag tagCorrelationValue = RNBO::TAG ("correlation_value");

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioProcessor)
};
