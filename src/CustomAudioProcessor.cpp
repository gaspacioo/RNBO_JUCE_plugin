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

        // findMinAndMax è vettorizzato (SIMD): il picco è il maggiore in valore assoluto
        // tra minimo e massimo del blocco.
        const auto range = juce::FloatVectorOperations::findMinAndMax (samples, numSamples);
        const float peak = juce::jmax (std::abs (range.getStart()), std::abs (range.getEnd()));

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

CustomAudioProcessor::~CustomAudioProcessor()
{
    // Ferma il thread spettro prima che i membri che usa vengano distrutti.
    _specThread.stopThread (1000);
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
    // Ferma il thread spettro mentre azzeriamo band-map, ring e FIFO: così nessuno
    // legge stato dello spettro a metà reset. Viene riavviato in fondo.
    _specThread.stopThread (1000);

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
    std::fill (std::begin (_decRingL), std::end (_decRingL), 0.0f);
    std::fill (std::begin (_decRingR), std::end (_decRingR), 0.0f);

    prepareDecimationFilter (sr);

    std::fill (std::begin (_specMagL), std::end (_specMagL), kSilenceDb);
    std::fill (std::begin (_specMagR), std::end (_specMagR), kSilenceDb);
    std::fill (std::begin (_specMagMid),  std::end (_specMagMid),  kSilenceDb);
    std::fill (std::begin (_specMagSide), std::end (_specMagSide), kSilenceDb);

    prepareLufs (sampleRate);

    // Riparti puliti e fai ripartire il thread spettro a priorità alta, così consuma
    // i frame prontamente (meno latenza) senza però competere col thread audio realtime.
    _specFrameFifo.reset();
    _specThread.startThread (::juce::Thread::Priority::high);
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
    if (numSamples <= 0)
        return;

    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    // Punti decimati prodotti da questo blocco (fase di decimazione per-blocco, come prima).
    const int numPoints = (numSamples + _scopeDownsample - 1) / _scopeDownsample;

    // Una sola prenotazione del FIFO, scrittura in (al più) due segmenti contigui:
    // niente più prepareToWrite/finishedWrite per ogni singolo campione.
    int start1, size1, start2, size2;
    _scopeFifo.prepareToWrite (numPoints, start1, size1, start2, size2);

    int written = 0;
    for (int seg = 0; seg < 2; ++seg)
    {
        const int start = (seg == 0) ? start1 : start2;
        const int size  = (seg == 0) ? size1  : size2;

        for (int j = 0; j < size; ++j)
        {
            const int i = (written + j) * _scopeDownsample;   // < numSamples per costruzione
            // Codifica M/S: X = Side (L−R), Y = Mid (L+R)
            _scopeBufX[start + j] = (chL[i] - chR[i]) * 0.5f;
            _scopeBufY[start + j] = (chL[i] + chR[i]) * 0.5f;
        }

        written += size;
    }

    _scopeFifo.finishedWrite (size1 + size2);
}

void CustomAudioProcessor::feedSpectrum (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto* chL = buffer.getReadPointer (0);
    const auto* chR = numChannels > 1 ? buffer.getReadPointer (1) : chL;

    // Biquad Direct Form I (in double): un campione attraverso una sezione.
    auto biquad = [] (const Biquad& c, BiquadState& s, double x) -> double
    {
        const double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    };

    // Mid/Side non vengono né accumulati né filtrati qui: la FFT è lineare, quindi gli
    // spettri M/S si ricavano esattamente da quelli di L/R in computeSpectrumFrame
    // (M = (L+R)/√2, S = (L−R)/√2). Così bastano 2 ring e 2 catene anti-alias invece di 4.
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = chL[i];
        const float r = chR[i];

        // --- Path medie/alte: ring a full rate ---
        _rawRingL[_rawRingPos] = l;
        _rawRingR[_rawRingPos] = r;
        _rawRingPos = (_rawRingPos + 1) & (kFftSize - 1);

        if (++_fftHopCount >= kFftHop)
        {
            _fftHopCount = 0;
            enqueueSpecFrame (false);   // copia il frame; la FFT avviene sul thread dedicato
        }

        // --- Path basse: filtro anti-alias (cascata di biquad) poi decimazione ---
        double fL = l, fR = r;
        for (int s = 0; s < kAaStages; ++s)
        {
            fL = biquad (_aaCoef[s], _aaState[0][s], fL);
            fR = biquad (_aaCoef[s], _aaState[1][s], fR);
        }

        if (++_decimCounter >= kSpecDecim)
        {
            _decimCounter = 0;

            _decRingL[_decRingPos] = (float) fL;
            _decRingR[_decRingPos] = (float) fR;
            _decRingPos = (_decRingPos + 1) & (kFftLowSize - 1);

            if (++_fftLowHopCount >= kFftLowHop)
            {
                _fftLowHopCount = 0;
                enqueueSpecFrame (true);   // copia il frame; la FFT avviene sul thread dedicato
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

    const int   halfNLow  = kFftLowSize / 2;
    const int   halfNHigh = kFftSize / 2;
    const float nyqLow    = nyquist / (float) kSpecDecim;   // Nyquist del segnale decimato
    const float frameComp = (float) kFftHop / (float) (kFftSize / 2);

    for (int b = 0; b < count; ++b)
    {
        // Bordi di banda log-spaced: banda b copre [edge(b), edge(b+1))
        const float fLo = fMin * std::pow (fMax / fMin, (float) b       / (float) count);
        const float fHi = fMin * std::pow (fMax / fMin, (float) (b + 1) / (float) count);
        const float fCenter = std::sqrt (fLo * fHi);

        // Peso di crossfade: 0 = solo path basso (fine), 1 = solo path alto (vivace).
        // smoothstep sul log della frequenza → transizione morbida, niente linea netta.
        float blend;
        if      (fCenter <= kSpecXfadeLoHz) blend = 0.0f;
        else if (fCenter >= kSpecXfadeHiHz) blend = 1.0f;
        else
        {
            const float t = std::log (fCenter / kSpecXfadeLoHz)
                          / std::log (kSpecXfadeHiHz / kSpecXfadeLoHz);
            blend = t * t * (3.0f - 2.0f * t);
        }
        _bandBlend[b] = blend;

        // Bin sul path basso (decimato): usati quando blend < 1.
        if (blend < 1.0f)
        {
            int lo = (int) std::floor (fLo / nyqLow * (float) halfNLow);
            int hi = (int) std::ceil  (fHi / nyqLow * (float) halfNLow) - 1;
            lo = juce::jlimit (1, halfNLow, lo);
            hi = juce::jlimit (lo, halfNLow, hi);
            _bandLoLow[b] = lo;
            _bandHiLow[b] = hi;
        }
        else { _bandLoLow[b] = 1; _bandHiLow[b] = 0; }   // range vuoto: non verrà letto

        // Bin sul path alto (full rate): usati quando blend > 0.
        if (blend > 0.0f)
        {
            int lo = (int) std::floor (fLo / nyquist * (float) halfNHigh);
            int hi = (int) std::ceil  (fHi / nyquist * (float) halfNHigh) - 1;
            lo = juce::jlimit (1, halfNHigh, lo);
            hi = juce::jlimit (lo, halfNHigh, hi);
            _bandLoHigh[b] = lo;
            _bandHiHigh[b] = hi;
        }
        else { _bandLoHigh[b] = 1; _bandHiHigh[b] = 0; }

        // Smoothing temporale per banda.
        if (blend <= 0.0f)
        {
            // Path basso puro: la finestra lunga (~372 ms) domina già la lentezza,
            // EMA leggera per non aggiungere altro ritardo.
            _bandAtk[b] = 0.30f;
            _bandDcy[b] = 0.78f;
        }
        else
        {
            // Graduato per frequenza: calmo nella zona di transizione (così si fonde col
            // path fine senza essere nervoso), reattivo verso l'acuto. Compensazione per
            // frame rate (a' = a^(hop/(N/2))) così la costante di tempo non dipende da kFftHop.
            const float u = juce::jlimit (0.0f, 1.0f,
                std::log (fCenter / kSpecXfadeLoHz) / std::log (fMax / kSpecXfadeLoHz));
            _bandAtk[b] = std::pow (juce::jmap (u, 0.90f, 0.45f), frameComp);
            _bandDcy[b] = std::pow (juce::jmap (u, 0.94f, 0.82f), frameComp);
        }
    }

    // Riparti da silenzio sulle bande attive: la mappatura bin→banda è cambiata.
    // Sotto lock perché ora rebuild gira sul thread spettro, mentre l'editor legge
    // _specMag*/_specBandCount: count e magnitudini devono cambiare in modo atomico.
    {
        const juce::ScopedLock lock (_specLock);
        std::fill (_specMagL,    _specMagL    + count, kSilenceDb);
        std::fill (_specMagR,    _specMagR    + count, kSilenceDb);
        std::fill (_specMagMid,  _specMagMid  + count, kSilenceDb);
        std::fill (_specMagSide, _specMagSide + count, kSilenceDb);
        std::fill (_specRawLowL,    _specRawLowL    + count, kSilenceDb);
        std::fill (_specRawLowR,    _specRawLowR    + count, kSilenceDb);
        std::fill (_specRawLowMid,  _specRawLowMid  + count, kSilenceDb);
        std::fill (_specRawLowSide, _specRawLowSide + count, kSilenceDb);
        std::fill (_specRawHighL,    _specRawHighL    + count, kSilenceDb);
        std::fill (_specRawHighR,    _specRawHighR    + count, kSilenceDb);
        std::fill (_specRawHighMid,  _specRawHighMid  + count, kSilenceDb);
        std::fill (_specRawHighSide, _specRawHighSide + count, kSilenceDb);
        _specBandCount.store (count, std::memory_order_release);
    }
}

void CustomAudioProcessor::prepareDecimationFilter (double sampleRate)
{
    const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Nyquist dopo la decimazione = fs / (2 · kSpecDecim) ≈ 1378 Hz a 44100 Hz.
    // Il taglio deve coprire tutta la zona di crossfade (la banda passante piatta deve
    // arrivare fin sopra kSpecXfadeHiHz), ma restare ben sotto la Nyquist decimata. Le
    // frequenze che potrebbero ripiegarsi nella zona mostrata cadono molto sotto con un
    // Butterworth di 6° ordine.
    const double decNyquist = fs / (2.0 * (double) kSpecDecim);
    const double fc         = std::min ((double) kSpecXfadeHiHz * 1.1, decNyquist * 0.7);

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

void CustomAudioProcessor::enqueueSpecFrame (bool lowBands)
{
    // Thread audio: cattura una copia coerente del ring nel prossimo slot libero e
    // sveglia il thread spettro. Niente FFT qui → il callback audio resta leggero.
    int s1, sz1, s2, sz2;
    _specFrameFifo.prepareToWrite (1, s1, sz1, s2, sz2);
    if (sz1 <= 0)
        return;   // coda piena: il consumer è in ritardo, salta questo frame

    SpecFrame& f = _specFrames[s1];
    const int fftLen  = lowBands ? kFftLowSize : kFftSize;
    const int ringPos = lowBands ? _decRingPos : _rawRingPos;
    const int mask    = fftLen - 1;
    const float* ringL = lowBands ? _decRingL : _rawRingL;
    const float* ringR = lowBands ? _decRingR : _rawRingR;

    f.lowBands = lowBands;
    f.len      = fftLen;
    for (int k = 0; k < fftLen; ++k)
    {
        const int idx = (ringPos + k) & mask;   // ordine cronologico crescente
        f.L[k] = ringL[idx];
        f.R[k] = ringR[idx];
    }

    _specFrameFifo.finishedWrite (1);
    _specThread.notify();
}

void CustomAudioProcessor::specThreadRun()
{
    // Thread dedicato: consuma i frame catturati dal thread audio e fa la FFT qui,
    // fuori dal callback real-time. Anche il cambio del numero di bande avviene su
    // questo thread, così band-map e magnitudini non sono mai mutate mentre l'audio scrive.
    while (! _specThread.threadShouldExit())
    {
        const int pending = _pendingSpecBandCount.exchange (0, std::memory_order_relaxed);
        if (pending != 0 && pending != _specBandCount.load (std::memory_order_relaxed))
            rebuildSpecBands (pending);

        while (_specFrameFifo.getNumReady() > 0 && ! _specThread.threadShouldExit())
        {
            int s1, sz1, s2, sz2;
            _specFrameFifo.prepareToRead (1, s1, sz1, s2, sz2);
            const int slot = sz1 > 0 ? s1 : s2;
            if (sz1 + sz2 <= 0)
                break;

            computeSpectrumFrame (_specFrames[slot]);
            _specFrameFifo.finishedRead (1);
        }

        _specThread.wait (5);   // svegliato da notify(); il timeout è solo una rete di sicurezza
    }
}

void CustomAudioProcessor::computeSpectrumFrame (const SpecFrame& frame)
{
    const bool  lowBands = frame.lowBands;
    const int   fftLen   = frame.len;
    const int   numBands = _specBandCount.load (std::memory_order_relaxed);

    float newMagL[kSpecBandsMax];
    float newMagR[kSpecBandsMax];
    float newMagMid[kSpecBandsMax];
    float newMagSide[kSpecBandsMax];

    auto&       fft     = lowBands ? _fftLow        : _fft;
    const auto* window  = lowBands ? _hannWindowLow : _hannWindow;

    // Normalizzazione ampiezza: 2 / (guadagno coerente finestra · N).
    // Blackman-Harris ha guadagno coerente a0 = 0.35875 (Hann era 0.5).
    const float magScale = 2.0f / (0.35875f * (float) fftLen);

    // Il frame è già linearizzato in ordine cronologico dal thread audio.
    // Solo 2 FFT (L e R): gli spettri Mid/Side si ricavano per linearità della DFT
    // direttamente dai bin di L e R (vedi sotto). Costo dimezzato rispetto a 4 FFT.
    for (int k = 0; k < fftLen; ++k)
        _fftWorkBuf[k] = frame.L[k] * window[k];
    std::fill (_fftWorkBuf + fftLen, _fftWorkBuf + fftLen * 2, 0.0f);
    fft.performRealOnlyForwardTransform (_fftWorkBuf, true);

    for (int k = 0; k < fftLen; ++k)
        _fftWorkBufR[k] = frame.R[k] * window[k];
    std::fill (_fftWorkBufR + fftLen, _fftWorkBufR + fftLen * 2, 0.0f);
    fft.performRealOnlyForwardTransform (_fftWorkBufR, true);

    constexpr float msScale = 0.70710678f;   // 1/√2: codifica M/S a energia costante

    // Questo path contribuisce a una banda se è il path basso (blend < 1) o il path alto
    // (blend > 0). I bin da usare sono quelli calcolati per QUESTO path in rebuildSpecBands.
    const int* bandLo = lowBands ? _bandLoLow : _bandLoHigh;
    const int* bandHi = lowBands ? _bandHiLow : _bandHiHigh;

    auto toDb = [magScale] (float maxMagSq)
    {
        const float mag = std::sqrt (maxMagSq) * magScale;
        return mag > 1e-6f ? 20.0f * std::log10 (mag) : kSilenceDb;
    };

    for (int b = 0; b < numBands; ++b)
    {
        const float blend  = _bandBlend[b];
        const bool  active = lowBands ? (blend < 1.0f) : (blend > 0.0f);
        if (! active)
            continue;

        float maxL = 0.0f, maxR = 0.0f, maxM = 0.0f, maxS = 0.0f;

        for (int bin = bandLo[b]; bin <= bandHi[b]; ++bin)
        {
            const float reL = _fftWorkBuf[bin * 2];
            const float imL = _fftWorkBuf[bin * 2 + 1];
            const float reR = _fftWorkBufR[bin * 2];
            const float imR = _fftWorkBufR[bin * 2 + 1];

            // M = (L+R)/√2, S = (L−R)/√2 nel dominio della frequenza: identico
            // a calcolare la FFT del Mid/Side nel dominio del tempo (DFT lineare).
            const float reM = (reL + reR) * msScale;
            const float imM = (imL + imR) * msScale;
            const float reS = (reL - reR) * msScale;
            const float imS = (imL - imR) * msScale;

            maxL = std::max (maxL, reL * reL + imL * imL);
            maxR = std::max (maxR, reR * reR + imR * imR);
            maxM = std::max (maxM, reM * reM + imM * imM);
            maxS = std::max (maxS, reS * reS + imS * imS);
        }

        newMagL[b]    = toDb (maxL);
        newMagR[b]    = toDb (maxR);
        newMagMid[b]  = toDb (maxM);
        newMagSide[b] = toDb (maxS);
    }

    // Lock bloccante: siamo sul thread spettro (non real-time), l'editor tiene il lock
    // solo per la breve copia delle bande. Aggiorniamo il raw di questo path, fondiamo
    // col raw dell'altro path (crossfade) e smussiamo nel tempo verso il risultato.
    {
        const juce::ScopedLock lock (_specLock);

        // Raw per-path da aggiornare con i valori appena calcolati.
        float* rawL    = lowBands ? _specRawLowL    : _specRawHighL;
        float* rawR    = lowBands ? _specRawLowR    : _specRawHighR;
        float* rawMid  = lowBands ? _specRawLowMid  : _specRawHighMid;
        float* rawSide = lowBands ? _specRawLowSide : _specRawHighSide;

        for (int b = 0; b < numBands; ++b)
        {
            const float blend  = _bandBlend[b];
            const bool  active = lowBands ? (blend < 1.0f) : (blend > 0.0f);
            if (! active)
                continue;

            rawL[b]    = newMagL[b];
            rawR[b]    = newMagR[b];
            rawMid[b]  = newMagMid[b];
            rawSide[b] = newMagSide[b];

            // Crossfade: l'altro path usa il suo ultimo valore memorizzato (il path basso
            // cambia lentamente, quindi il suo ultimo valore va sempre bene per la fusione).
            const float bL    = (1.0f - blend) * _specRawLowL[b]    + blend * _specRawHighL[b];
            const float bR    = (1.0f - blend) * _specRawLowR[b]    + blend * _specRawHighR[b];
            const float bMid  = (1.0f - blend) * _specRawLowMid[b]  + blend * _specRawHighMid[b];
            const float bSide = (1.0f - blend) * _specRawLowSide[b] + blend * _specRawHighSide[b];

            const float aAtk = _bandAtk[b];
            const float aDcy = _bandDcy[b];
            auto smooth = [aAtk, aDcy] (float cur, float inc)
            {
                const float a = (inc > cur) ? aAtk : aDcy;
                return a * cur + (1.0f - a) * inc;
            };

            _specMagL[b]    = smooth (_specMagL[b],    bL);
            _specMagR[b]    = smooth (_specMagR[b],    bR);
            _specMagMid[b]  = smooth (_specMagMid[b],  bMid);
            _specMagSide[b] = smooth (_specMagSide[b], bSide);
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
