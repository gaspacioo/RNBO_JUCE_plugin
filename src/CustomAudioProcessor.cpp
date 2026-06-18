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

    _specSampleRate = sr;
    rebuildSpecBands (_specBandCount.load (std::memory_order_relaxed));
    _pendingSpecBandCount.store (0, std::memory_order_relaxed);

    _rawRingPos     = 0;
    _decRingPos     = 0;
    _fftHopCount    = 0;
    _fftLowHopCount = 0;
    _decimCounter   = 0;
    std::fill (std::begin (_rawRingL), std::end (_rawRingL), 0.0f);
    std::fill (std::begin (_rawRingR), std::end (_rawRingR), 0.0f);
    std::fill (std::begin (_rawRingMid),  std::end (_rawRingMid),  0.0f);
    std::fill (std::begin (_rawRingSide), std::end (_rawRingSide), 0.0f);
    std::fill (std::begin (_decRingL), std::end (_decRingL), 0.0f);
    std::fill (std::begin (_decRingR), std::end (_decRingR), 0.0f);
    std::fill (std::begin (_decRingMid),  std::end (_decRingMid),  0.0f);
    std::fill (std::begin (_decRingSide), std::end (_decRingSide), 0.0f);

    prepareDecimationFilter (sr);

    std::fill (std::begin (_specMagL), std::end (_specMagL), kSilenceDb);
    std::fill (std::begin (_specMagR), std::end (_specMagR), kSilenceDb);
    std::fill (std::begin (_specMagMid),  std::end (_specMagMid),  kSilenceDb);
    std::fill (std::begin (_specMagSide), std::end (_specMagSide), kSilenceDb);

    prepareLufs (sampleRate);
}

//==============================================================================
// LUFS – ITU-R BS.1770: filtro K (shelf + high-pass), mean-square, gating.
// I coefficienti dei biquad sono ricalcolati analiticamente al sample rate
// corrente (a 48 kHz coincidono con la Tabella 1/2 del BS.1770).
void CustomAudioProcessor::prepareLufs (double sampleRate)
{
    const double fs = sampleRate > 0.0 ? sampleRate : 48000.0;

    // Stadio 1: shelving "testa" (+~4 dB sopra ~1.5 kHz)
    {
        const double f0 = 1681.974450955533;
        const double G  = 3.999843853973347;
        const double Q  = 0.7071752369554196;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        _kStage1.b0 = (Vh + Vb * K / Q + K * K) / a0;
        _kStage1.b1 = 2.0 * (K * K - Vh) / a0;
        _kStage1.b2 = (Vh - Vb * K / Q + K * K) / a0;
        _kStage1.a1 = 2.0 * (K * K - 1.0) / a0;
        _kStage1.a2 = (1.0 - K / Q + K * K) / a0;
    }
    // Stadio 2: high-pass RLB (~38 Hz)
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        _kStage2.b0 = 1.0;
        _kStage2.b1 = -2.0;
        _kStage2.b2 = 1.0;
        _kStage2.a1 = 2.0 * (K * K - 1.0) / a0;
        _kStage2.a2 = (1.0 - K / Q + K * K) / a0;
    }

    for (auto& s : _kS1) s = {};
    for (auto& s : _kS2) s = {};

    _lufsSubLen   = std::max (1, static_cast<int> (std::round (fs * 0.1)));   // 100 ms
    _lufsSubSumL  = _lufsSubSumR = 0.0;
    _lufsSubCount = 0;
    std::fill (std::begin (_lufsBlockL), std::end (_lufsBlockL), 0.0);
    std::fill (std::begin (_lufsBlockR), std::end (_lufsBlockR), 0.0);
    _lufsBlockPos = _lufsBlockFilled = 0;
    std::fill (std::begin (_lufsHist), std::end (_lufsHist), 0);

    meterLevels.lufsMomentary.store  (-100.f, std::memory_order_relaxed);
    meterLevels.lufsShortTerm.store  (-100.f, std::memory_order_relaxed);
    meterLevels.lufsIntegrated.store (-100.f, std::memory_order_relaxed);
}

void CustomAudioProcessor::resetLufsIntegrated()
{
    // Azzera solo l'istogramma dell'Integrated (M e S restano scorrevoli)
    std::fill (std::begin (_lufsHist), std::end (_lufsHist), 0);
    meterLevels.lufsIntegrated.store (-100.f, std::memory_order_relaxed);
}

void CustomAudioProcessor::feedLufs (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    auto applyBiquad = [] (const Biquad& c, BiquadState& s, double x)
    {
        const double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const double yL = applyBiquad (_kStage2, _kS2[0], applyBiquad (_kStage1, _kS1[0], (double) chL[i]));
        const double yR = applyBiquad (_kStage2, _kS2[1], applyBiquad (_kStage1, _kS1[1], (double) chR[i]));

        _lufsSubSumL += yL * yL;
        _lufsSubSumR += yR * yR;

        if (++_lufsSubCount >= _lufsSubLen)
            pushLufsSubBlock();
    }
}

void CustomAudioProcessor::pushLufsSubBlock()
{
    // Mean-square del sotto-blocco da 100 ms, per canale
    const double msL = _lufsSubSumL / (double) _lufsSubCount;
    const double msR = _lufsSubSumR / (double) _lufsSubCount;
    _lufsSubSumL = _lufsSubSumR = 0.0;
    _lufsSubCount = 0;

    _lufsBlockL[_lufsBlockPos] = msL;
    _lufsBlockR[_lufsBlockPos] = msR;
    _lufsBlockPos = (_lufsBlockPos + 1) % kLufsMaxBlocks;
    if (_lufsBlockFilled < kLufsMaxBlocks) ++_lufsBlockFilled;

    // Media degli ultimi n sotto-blocchi (per canale) → loudness
    auto meanLoudness = [this] (int nBlocks) -> double
    {
        if (_lufsBlockFilled < nBlocks) return -1000.0;
        double sumL = 0.0, sumR = 0.0;
        for (int k = 1; k <= nBlocks; ++k)
        {
            const int idx = (_lufsBlockPos - k + kLufsMaxBlocks) % kLufsMaxBlocks;
            sumL += _lufsBlockL[idx];
            sumR += _lufsBlockR[idx];
        }
        const double z = (sumL + sumR) / (double) nBlocks;   // G_L=G_R=1
        return z > 1e-12 ? -0.691 + 10.0 * std::log10 (z) : -1000.0;
    };

    // Momentary = 400 ms (4 blocchi), Short-term = 3 s (30 blocchi)
    const double m = meanLoudness (4);
    const double s = meanLoudness (30);
    meterLevels.lufsMomentary.store (m <= -1000.0 ? -100.f : (float) m, std::memory_order_relaxed);
    meterLevels.lufsShortTerm.store (s <= -1000.0 ? -100.f : (float) s, std::memory_order_relaxed);

    // Gating block dell'Integrated = 400 ms (4 blocchi), nuovo ogni 100 ms (75% overlap)
    if (_lufsBlockFilled >= 4)
    {
        const double lj = m;   // loudness del gating block da 400 ms
        if (lj > kLufsHistMin)   // gate assoluto -70 LUFS
        {
            int bin = (int) std::floor ((lj - kLufsHistMin) / kLufsHistStep);
            bin = juce::jlimit (0, kLufsHistBins - 1, bin);
            ++_lufsHist[bin];
            updateLufsIntegrated();
        }
    }
}

void CustomAudioProcessor::updateLufsIntegrated()
{
    auto binLoudness = [] (int bin) { return kLufsHistMin + (bin + 0.5) * kLufsHistStep; };
    auto binZ        = [] (double l) { return std::pow (10.0, (l + 0.691) / 10.0); };

    // Passo 1: media (gated solo dal -70 assoluto) → soglia relativa = media - 10 LU
    double sumZ = 0.0; long long n = 0;
    for (int b = 0; b < kLufsHistBins; ++b)
        if (_lufsHist[b] > 0) { sumZ += _lufsHist[b] * binZ (binLoudness (b)); n += _lufsHist[b]; }

    if (n == 0) { meterLevels.lufsIntegrated.store (-100.f, std::memory_order_relaxed); return; }

    const double relGate = -0.691 + 10.0 * std::log10 (sumZ / (double) n) - 10.0;

    // Passo 2: media dei blocchi sopra la soglia relativa
    double sumZ2 = 0.0; long long n2 = 0;
    for (int b = 0; b < kLufsHistBins; ++b)
        if (_lufsHist[b] > 0 && binLoudness (b) > relGate)
        { sumZ2 += _lufsHist[b] * binZ (binLoudness (b)); n2 += _lufsHist[b]; }

    const double integrated = n2 > 0 ? -0.691 + 10.0 * std::log10 (sumZ2 / (double) n2) : -100.0;
    meterLevels.lufsIntegrated.store ((float) integrated, std::memory_order_relaxed);
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

    // Cambio del numero di bande richiesto dalla UI: applicato qui (thread audio)
    // così la band-map e i mag non sono mai modificati mentre vengono letti altrove.
    const int pending = _pendingSpecBandCount.exchange (0, std::memory_order_relaxed);
    if (pending != 0 && pending != _specBandCount.load (std::memory_order_relaxed))
        rebuildSpecBands (pending);

    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    constexpr float msScale = 0.70710678f;   // 1/sqrt(2): codifica M/S a energia costante

    // Biquad Direct Form I (in double): un campione attraverso una sezione.
    auto biquad = [] (const Biquad& c, BiquadState& s, double x) -> double
    {
        const double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const float l    = chL[i];
        const float r    = chR[i];
        const float mid  = (l + r) * msScale;
        const float side = (l - r) * msScale;

        // --- Path medie/alte: ring a full rate ---
        _rawRingL[_rawRingPos]    = l;
        _rawRingR[_rawRingPos]    = r;
        _rawRingMid[_rawRingPos]  = mid;
        _rawRingSide[_rawRingPos] = side;
        _rawRingPos = (_rawRingPos + 1) & (kFftSize - 1);

        if (++_fftHopCount >= kFftHop)
        {
            _fftHopCount = 0;
            computeSpectrumFrame (false);
        }

        // --- Path basse: filtro anti-alias (cascata di biquad) poi decimazione ---
        double fL = l, fR = r, fM = mid, fS = side;
        for (int s = 0; s < kAaStages; ++s)
        {
            fL = biquad (_aaCoef[s], _aaState[0][s], fL);
            fR = biquad (_aaCoef[s], _aaState[1][s], fR);
            fM = biquad (_aaCoef[s], _aaState[2][s], fM);
            fS = biquad (_aaCoef[s], _aaState[3][s], fS);
        }

        if (++_decimCounter >= kSpecDecim)
        {
            _decimCounter = 0;

            _decRingL[_decRingPos]    = (float) fL;
            _decRingR[_decRingPos]    = (float) fR;
            _decRingMid[_decRingPos]  = (float) fM;
            _decRingSide[_decRingPos] = (float) fS;
            _decRingPos = (_decRingPos + 1) & (kFftLowSize - 1);

            if (++_fftLowHopCount >= kFftLowHop)
            {
                _fftLowHopCount = 0;
                computeSpectrumFrame (true);
            }
        }
    }
}

void CustomAudioProcessor::setSpecBandCount (int n)
{
    // Richiesta dalla UI (message thread): applicata sul thread audio al prossimo blocco.
    _pendingSpecBandCount.store (juce::jlimit (32, kSpecBandsMax, n), std::memory_order_relaxed);
}

void CustomAudioProcessor::rebuildSpecBands (int count)
{
    count = juce::jlimit (32, kSpecBandsMax, count);

    const float sr      = (float) (_specSampleRate > 0.0 ? _specSampleRate : 48000.0);
    const float nyquist = sr * 0.5f;
    const float fMin    = 20.0f;
    const float fMax    = std::min (20000.0f, nyquist);

    for (int b = 0; b < count; ++b)
    {
        // Bordi di banda log-spaced: banda b copre [edge(b), edge(b+1))
        const float fLo = fMin * std::pow (fMax / fMin, (float) b       / (float) count);
        const float fHi = fMin * std::pow (fMax / fMin, (float) (b + 1) / (float) count);

        // Sotto il crossover la banda usa la FFT decimata (più bin per ottava). In quel
        // caso la Nyquist di riferimento è quella del segnale decimato (sr / 2·kSpecDecim).
        const bool  useLow      = fHi < kSpecLowCrossHz;
        const int   halfN       = (useLow ? kFftLowSize : kFftSize) / 2;
        const float bandNyquist = useLow ? nyquist / (float) kSpecDecim : nyquist;

        int lo = (int) std::floor (fLo / bandNyquist * (float) halfN);
        int hi = (int) std::ceil  (fHi / bandNyquist * (float) halfN) - 1;

        lo = juce::jlimit (1, halfN, lo);
        hi = juce::jlimit (lo, halfN, hi);

        _bandLo[b]     = lo;
        _bandHi[b]     = hi;
        _bandUseLow[b] = useLow;

        // Smoothing graduato per frequenza (centro geometrico della banda).
        const float fCenter = std::sqrt (fLo * fHi);

        if (useLow)
        {
            // Path decimato: la finestra lunga (~372 ms) domina già la lentezza, quindi
            // lo smoothing resta leggero per non aggiungere altro ritardo.
            _bandAtk[b] = 0.30f;
            _bandDcy[b] = 0.78f;
        }
        else
        {
            // Path corto: subito sopra il crossover emuliamo la lentezza della finestra
            // lunga (alpha alto → stessa risposta delle basse, niente scalino), poi
            // rilassiamo verso l'acuto dove la reattività è desiderata.
            const float u = juce::jlimit (0.0f, 1.0f,
                std::log (fCenter / kSpecLowCrossHz) / std::log (fMax / kSpecLowCrossHz));
            _bandAtk[b] = juce::jmap (u, 0.90f, 0.15f);
            _bandDcy[b] = juce::jmap (u, 0.90f, 0.62f);
        }
    }

    // Riparti da silenzio sulle bande attive: la mappatura bin→banda è cambiata
    std::fill (_specMagL,    _specMagL    + count, kSilenceDb);
    std::fill (_specMagR,    _specMagR    + count, kSilenceDb);
    std::fill (_specMagMid,  _specMagMid  + count, kSilenceDb);
    std::fill (_specMagSide, _specMagSide + count, kSilenceDb);

    _specBandCount.store (count, std::memory_order_release);
}

void CustomAudioProcessor::prepareDecimationFilter (double sampleRate)
{
    const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Nyquist dopo la decimazione = fs / (2 · kSpecDecim) ≈ 1378 Hz a 44100 Hz.
    // Il taglio va sopra il crossover (così il crossover resta nella banda passante piatta)
    // ma ben sotto la Nyquist decimata. Le frequenze che potrebbero ripiegarsi nella zona
    // mostrata (≥ ~2,2 kHz) cadono > 50 dB sotto con un Butterworth di 6° ordine.
    const double decNyquist = fs / (2.0 * (double) kSpecDecim);
    const double fc         = std::min ((double) kSpecLowCrossHz * 1.6, decNyquist * 0.7);

    // Q delle 3 sezioni biquad di un Butterworth di 6° ordine.
    const double q[kAaStages] = { 0.51763809, 0.70710678, 1.93185165 };

    for (int s = 0; s < kAaStages; ++s)
    {
        const double w0    = 2.0 * juce::MathConstants<double>::pi * fc / fs;
        const double cosw0 = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q[s]);
        const double a0    = 1.0 + alpha;

        // Lowpass RBJ normalizzato per a0.
        _aaCoef[s].b0 = (1.0 - cosw0) * 0.5 / a0;
        _aaCoef[s].b1 = (1.0 - cosw0)       / a0;
        _aaCoef[s].b2 = (1.0 - cosw0) * 0.5 / a0;
        _aaCoef[s].a1 = (-2.0 * cosw0)      / a0;
        _aaCoef[s].a2 = (1.0 - alpha)       / a0;
    }

    for (auto& ch : _aaState)
        for (auto& st : ch)
            st = {};
}

void CustomAudioProcessor::computeSpectrumFrame (bool lowBands)
{
    const int numBands = _specBandCount.load (std::memory_order_relaxed);

    float newMagL[kSpecBandsMax];
    float newMagR[kSpecBandsMax];
    float newMagMid[kSpecBandsMax];
    float newMagSide[kSpecBandsMax];

    auto&       fft     = lowBands ? _fftLow        : _fft;
    const auto* window  = lowBands ? _hannWindowLow : _hannWindow;
    const int   fftLen  = lowBands ? kFftLowSize    : kFftSize;

    // Normalizzazione ampiezza: 2 / (guadagno coerente finestra · N).
    // Blackman-Harris ha guadagno coerente a0 = 0.35875 (Hann era 0.5).
    const float magScale = 2.0f / (0.35875f * (float) fftLen);

    // Ogni ring ha dimensione esattamente pari alla propria finestra: l'elemento più
    // vecchio è in ringPos, leggiamo in ordine cronologico crescente.
    const int   ringPos  = lowBands ? _decRingPos : _rawRingPos;
    const int   ringMask = fftLen - 1;

    const float* ringsLow[4]  = { _decRingL, _decRingR, _decRingMid, _decRingSide };
    const float* ringsHigh[4] = { _rawRingL, _rawRingR, _rawRingMid, _rawRingSide };
    const float* const* rings = lowBands ? ringsLow : ringsHigh;
    float*       results[4]   = { newMagL,   newMagR,   newMagMid,   newMagSide   };

    for (int ch = 0; ch < 4; ++ch)
    {
        for (int k = 0; k < fftLen; ++k)
            _fftWorkBuf[k] = rings[ch][(ringPos + k) & ringMask] * window[k];

        std::fill (_fftWorkBuf + fftLen, _fftWorkBuf + fftLen * 2, 0.0f);
        fft.performRealOnlyForwardTransform (_fftWorkBuf, true);

        for (int b = 0; b < numBands; ++b)
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
        // Smoothing asimmetrico: attacco veloce (alpha basso → il valore nuovo pesa
        // di più, il picco sale subito), decadimento lento (alpha alto → scende piano).
        // Come un analizzatore hardware: transienti immediati, coda morbida.
        // Smoothing asimmetrico (attacco veloce, decadimento lento) con coefficienti
        // graduati per banda: vedi rebuildSpecBands. La risposta varia con continuità
        // dallo spettro basso (lento) all'alto (reattivo), senza cucitura a 500 Hz.
        for (int b = 0; b < numBands; ++b)
        {
            if (_bandUseLow[b] != lowBands)
                continue;

            const float aAtk = _bandAtk[b];
            const float aDcy = _bandDcy[b];

            auto smooth = [aAtk, aDcy] (float cur, float inc)
            {
                const float a = (inc > cur) ? aAtk : aDcy;
                return a * cur + (1.0f - a) * inc;
            };

            _specMagL[b]    = smooth (_specMagL[b],    newMagL[b]);
            _specMagR[b]    = smooth (_specMagR[b],    newMagR[b]);
            _specMagMid[b]  = smooth (_specMagMid[b],  newMagMid[b]);
            _specMagSide[b] = smooth (_specMagSide[b], newMagSide[b]);
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
    feedLufs (buffer);
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
