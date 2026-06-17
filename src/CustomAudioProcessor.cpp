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

    // --- Spettro: finestre Blackman-Harris (4 termini) + mappatura bande log ---
    // Lobi laterali ~-92 dB: molto meno leakage spettrale rispetto a Hann (-31 dB),
    // così un tono puro non "sbava" sulle frequenze vicine.
    auto blackmanHarris = [] (int i, int n)
    {
        constexpr float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
        const float t = 2.0f * juce::MathConstants<float>::pi * (float) i / (float) (n - 1);
        return a0 - a1 * std::cos (t) + a2 * std::cos (2.0f * t) - a3 * std::cos (3.0f * t);
    };

    for (int i = 0; i < kFftSize; ++i)
        _hannWindow[i] = blackmanHarris (i, kFftSize);

    for (int i = 0; i < kFftLowSize; ++i)
        _hannWindowLow[i] = blackmanHarris (i, kFftLowSize);

    const float nyquist = sr * 0.5f;
    const float fMin    = 20.0f;
    const float fMax    = std::min (20000.0f, nyquist);

    for (int b = 0; b < kSpecBands; ++b)
    {
        // Bordi di banda log-spaced: banda b copre [edge(b), edge(b+1))
        const float fLo = fMin * std::pow (fMax / fMin, (float) b       / (float) kSpecBands);
        const float fHi = fMin * std::pow (fMax / fMin, (float) (b + 1) / (float) kSpecBands);

        // Sotto il crossover la banda usa la FFT lunga (più bin per ottava)
        const bool useLow = fHi < kSpecLowCrossHz;
        const int  halfN  = (useLow ? kFftLowSize : kFftSize) / 2;

        int lo = (int) std::floor (fLo / nyquist * (float) halfN);
        int hi = (int) std::ceil  (fHi / nyquist * (float) halfN) - 1;

        lo = juce::jlimit (1, halfN, lo);
        hi = juce::jlimit (lo, halfN, hi);

        _bandLo[b]     = lo;
        _bandHi[b]     = hi;
        _bandUseLow[b] = useLow;
    }

    _fftRingPos     = 0;
    _fftHopCount    = 0;
    _fftLowHopCount = 0;
    std::fill (std::begin (_fftRingL), std::end (_fftRingL), 0.0f);
    std::fill (std::begin (_fftRingR), std::end (_fftRingR), 0.0f);
    std::fill (std::begin (_fftRingMid),  std::end (_fftRingMid),  0.0f);
    std::fill (std::begin (_fftRingSide), std::end (_fftRingSide), 0.0f);
    std::fill (std::begin (_specMagL), std::end (_specMagL), kSilenceDb);
    std::fill (std::begin (_specMagR), std::end (_specMagR), kSilenceDb);
    std::fill (std::begin (_specMagMid),  std::end (_specMagMid),  kSilenceDb);
    std::fill (std::begin (_specMagSide), std::end (_specMagSide), kSilenceDb);
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

void CustomAudioProcessor::feedSpectrum (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    constexpr float msScale = 0.70710678f;   // 1/sqrt(2): codifica M/S a energia costante

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = chL[i];
        const float r = chR[i];
        _fftRingL[_fftRingPos]    = l;
        _fftRingR[_fftRingPos]    = r;
        _fftRingMid[_fftRingPos]  = (l + r) * msScale;
        _fftRingSide[_fftRingPos] = (l - r) * msScale;
        _fftRingPos = (_fftRingPos + 1) & (kFftLowSize - 1);

        if (++_fftHopCount >= kFftHop)
        {
            _fftHopCount = 0;
            computeSpectrumFrame (false);
        }

        if (++_fftLowHopCount >= kFftLowHop)
        {
            _fftLowHopCount = 0;
            computeSpectrumFrame (true);
        }
    }
}

void CustomAudioProcessor::computeSpectrumFrame (bool lowBands)
{
    float newMagL[kSpecBands];
    float newMagR[kSpecBands];
    float newMagMid[kSpecBands];
    float newMagSide[kSpecBands];

    auto&       fft     = lowBands ? _fftLow        : _fft;
    const auto* window  = lowBands ? _hannWindowLow : _hannWindow;
    const int   fftLen  = lowBands ? kFftLowSize    : kFftSize;

    // Normalizzazione ampiezza: 2 / (guadagno coerente finestra · N).
    // Blackman-Harris ha guadagno coerente a0 = 0.35875 (Hann era 0.5).
    const float magScale = 2.0f / (0.35875f * (float) fftLen);

    const int ringOffset = _fftRingPos + kFftLowSize - fftLen;

    const float* rings[4]   = { _fftRingL, _fftRingR, _fftRingMid, _fftRingSide };
    float*       results[4] = { newMagL,   newMagR,   newMagMid,   newMagSide   };

    for (int ch = 0; ch < 4; ++ch)
    {
        for (int k = 0; k < fftLen; ++k)
            _fftWorkBuf[k] = rings[ch][(ringOffset + k) & (kFftLowSize - 1)] * window[k];

        std::fill (_fftWorkBuf + fftLen, _fftWorkBuf + fftLen * 2, 0.0f);
        fft.performRealOnlyForwardTransform (_fftWorkBuf, true);

        for (int b = 0; b < kSpecBands; ++b)
        {
            if (_bandUseLow[b] != lowBands)
                continue;

            float maxMagSq = 0.0f;

            for (int bin = _bandLo[b]; bin <= _bandHi[b]; ++bin)
            {
                const float re = _fftWorkBuf[bin * 2];
                const float im = _fftWorkBuf[bin * 2 + 1];
                maxMagSq = std::max (maxMagSq, re * re + im * im);
            }

            const float mag = std::sqrt (maxMagSq) * magScale;
            results[ch][b]  = mag > 1e-6f ? 20.0f * std::log10 (mag) : kSilenceDb;
        }
    }

    const juce::GenericScopedTryLock<juce::CriticalSection> lock (_specLock);

    if (lock.isLocked())
    {
        const float alpha = lowBands ? 0.6f : 0.7f;

        for (int b = 0; b < kSpecBands; ++b)
        {
            if (_bandUseLow[b] != lowBands)
                continue;

            _specMagL[b] = alpha * _specMagL[b] + (1.0f - alpha) * newMagL[b];
            _specMagR[b] = alpha * _specMagR[b] + (1.0f - alpha) * newMagR[b];
            _specMagMid[b]  = alpha * _specMagMid[b]  + (1.0f - alpha) * newMagMid[b];
            _specMagSide[b] = alpha * _specMagSide[b] + (1.0f - alpha) * newMagSide[b];
        }

        _specNewData.store (true, std::memory_order_release);
    }
}

void CustomAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (getTotalNumInputChannels() > 0)
        measurePeaks (buffer, false);

    RNBO::JuceAudioProcessor::processBlock (buffer, midiMessages);

    measurePeaks (buffer, true);
    fillScopeFifo (buffer);
    feedSpectrum (buffer);
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
