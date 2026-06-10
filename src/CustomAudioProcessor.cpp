#include "CustomAudioProcessor.h"
#if defined(RNBO_EDITOR_NATIVE)
#include "CustomAudioEditor.h"
#elif defined(RNBO_EDITOR_WEBVIEW)
#include "WebBrowserAudioEditor.h"
#endif
#include <json/json.hpp>
#include <cmath>

#ifdef RNBO_INCLUDE_DESCRIPTION_FILE
#include <rnbo_description.h>
#endif

namespace
{
    constexpr float kSilenceDb = -100.f;

    float linearPeakToDb (float peak)
    {
        if (peak <= 1e-10f)
            return kSilenceDb;

        return 20.0f * std::log10 (peak);
    }

    float measureChannelPeakDb (const float* samples, int numSamples)
    {
        if (samples == nullptr || numSamples <= 0)
            return kSilenceDb;

        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = std::max (peak, std::abs (samples[i]));

        return linearPeakToDb (peak);
    }

    void updatePeakHold (std::atomic<float>& hold, float blockDb, float decayDbPerBlock)
    {
        const auto current = hold.load (std::memory_order_relaxed);

        if (blockDb > current)
        {
            hold.store (blockDb, std::memory_order_relaxed);
            return;
        }

        const auto decayed = current - decayDbPerBlock;
        hold.store (std::max (blockDb, decayed), std::memory_order_relaxed);
    }
}

CustomAudioProcessor* CustomAudioProcessor::CreateDefault()
{
    nlohmann::json patcher_desc, presets;

#ifdef RNBO_BINARY_DATA_STORAGE_NAME
    extern RNBO::BinaryDataImpl::Storage RNBO_BINARY_DATA_STORAGE_NAME;
    RNBO::BinaryDataImpl::Storage dataStorage = RNBO_BINARY_DATA_STORAGE_NAME;
#else
    RNBO::BinaryDataImpl::Storage dataStorage;
#endif
    RNBO::BinaryDataImpl data (dataStorage);

#ifdef RNBO_INCLUDE_DESCRIPTION_FILE
    patcher_desc = RNBO::patcher_description;
    presets      = RNBO::patcher_presets;
#endif
    return new CustomAudioProcessor (patcher_desc, presets, data);
}

CustomAudioProcessor::CustomAudioProcessor (const nlohmann::json& patcher_desc,
                                             const nlohmann::json& presets,
                                             const RNBO::BinaryData& data)
    : RNBO::JuceAudioProcessor (patcher_desc, presets, data)
{
}

juce::AudioProcessorEditor* CustomAudioProcessor::createEditor()
{
#if defined(RNBO_EDITOR_NATIVE)
    return new CustomAudioEditor (this, this->_rnboObject);
#elif defined(RNBO_EDITOR_WEBVIEW)
    return new WebBrowserAudioEditor (this, this->_rnboObject);
#else
    return RNBO::JuceAudioProcessor::createEditor();
#endif
}

void CustomAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    RNBO::JuceAudioProcessor::prepareToPlay (sampleRate, samplesPerBlock);

    const float sr = static_cast<float> (sampleRate > 0.0 ? sampleRate : 44100.0);

    constexpr float peakFallDbPerSec = 12.0f;
    _peakDecayDbPerBlock = peakFallDbPerSec * static_cast<float> (samplesPerBlock) / sr;

    // Calcola il downsample per ottenere ~200 punti/frame a 60 Hz qualunque sia il sample rate
    constexpr int targetPointsPerFrame = 200;
    constexpr int webViewFps           = 60;
    _scopeDownsample = std::max (1, static_cast<int> (std::round (sr / (targetPointsPerFrame * webViewFps))));

    meterLevels.inPeakL.store (kSilenceDb, std::memory_order_relaxed);
    meterLevels.inPeakR.store (kSilenceDb, std::memory_order_relaxed);
    meterLevels.outPeakL.store (kSilenceDb, std::memory_order_relaxed);
    meterLevels.outPeakR.store (kSilenceDb, std::memory_order_relaxed);
}

void CustomAudioProcessor::measurePeaks (const juce::AudioBuffer<float>& buffer, bool isOutput)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto ch0Db = measureChannelPeakDb (buffer.getReadPointer (0), numSamples);

    if (isOutput)
    {
        updatePeakHold (meterLevels.outPeakL, ch0Db, _peakDecayDbPerBlock);

        if (numChannels > 1)
        {
            const auto ch1Db = measureChannelPeakDb (buffer.getReadPointer (1), numSamples);
            updatePeakHold (meterLevels.outPeakR, ch1Db, _peakDecayDbPerBlock);
        }
        else
        {
            updatePeakHold (meterLevels.outPeakR, ch0Db, _peakDecayDbPerBlock);
        }
    }
    else
    {
        updatePeakHold (meterLevels.inPeakL, ch0Db, _peakDecayDbPerBlock);

        if (numChannels > 1)
        {
            const auto ch1Db = measureChannelPeakDb (buffer.getReadPointer (1), numSamples);
            updatePeakHold (meterLevels.inPeakR, ch1Db, _peakDecayDbPerBlock);
        }
        else
        {
            updatePeakHold (meterLevels.inPeakR, ch0Db, _peakDecayDbPerBlock);
        }
    }
}

void CustomAudioProcessor::fillScopeFifo (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    for (int i = 0; i < numSamples; i += _scopeDownsample)
    {
        int start1, size1, start2, size2;
        _scopeFifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            // Codifica M/S: X = Side (L−R), Y = Mid (L+R)
            _scopeBufX[start1] = (chL[i] - chR[i]) * 0.5f;
            _scopeBufY[start1] = (chL[i] + chR[i]) * 0.5f;
            _scopeFifo.finishedWrite (1);
        }
    }
}

void CustomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (getTotalNumInputChannels() > 0)
        measurePeaks (buffer, false);

    RNBO::JuceAudioProcessor::processBlock (buffer, midiMessages);

    measurePeaks (buffer, true);
    fillScopeFifo (buffer);
}

void CustomAudioProcessor::handleMessageEvent (const RNBO::MessageEvent& event)
{
    static const RNBO::MessageTag setlatency = RNBO::TAG ("setlatency");

    if (event.getTag() == setlatency)
    {
        if (event.getType() == RNBO::MessageEvent::Type::Number)
            setLatencySamples (static_cast<int> (event.getNumValue()));

        return;
    }

    if (event.getType() == RNBO::MessageEvent::Type::Number)
    {
        const auto tag   = event.getTag();
        const auto value = static_cast<float> (event.getNumValue());

        if (tag == tagInRmsL)
            meterLevels.inL.store (value, std::memory_order_relaxed);
        else if (tag == tagInRmsR)
            meterLevels.inR.store (value, std::memory_order_relaxed);
        else if (tag == tagOutRmsL)
            meterLevels.outL.store (value, std::memory_order_relaxed);
        else if (tag == tagOutRmsR)
            meterLevels.outR.store (value, std::memory_order_relaxed);
        else if (tag == tagDelayTime)
            meterLevels.delayTime.store (value, std::memory_order_relaxed);
        else if (tag == tagCorrelationValue)
            meterLevels.correlationValue.store (value, std::memory_order_relaxed);
    }
    
    RNBO::EventHandler::handleMessageEvent (event);
}
