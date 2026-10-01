#include "MixDiagnostics.h"
#include "LoudnessAnalyzer.h"
#include "PeakAnalyzer.h"
#include <juce_dsp/juce_dsp.h>

namespace prl
{
AnalysisResult analyzeMix (const juce::AudioBuffer<float>& buf, int N, double sr)
{
    AnalysisResult a;
    N = std::min (N, buf.getNumSamples());
    if (N < (int) (sr * 1.0) || buf.getNumChannels() < 1) return a;
    const float* L = buf.getReadPointer (0);
    const float* R = buf.getReadPointer (std::min (1, buf.getNumChannels() - 1));
    a.sampleRate = sr; a.seconds = N / sr;

    // ---------------- loudness, peaks, rms, dc, clipping, correlation (one pass)
    LoudnessMeter lm; lm.prepare (sr);
    TruePeakMeter tp; tp.prepare (4);
    double sumL2 = 0, sumR2 = 0, sumLR = 0, sumM2 = 0, sumS2 = 0, dcL = 0, dcR = 0;
    int run = 0; float last = 0;
    const int stStep = (int) std::lround (sr * 0.1);
    for (int i = 0; i < N; ++i)
    {
        const float l = L[i], r = R[i];
        lm.process (l, r);
        tp.process (l, r);
        sumL2 += (double) l * l; sumR2 += (double) r * r; sumLR += (double) l * r;
        const double m = 0.5 * (l + r), s = 0.5 * (l - r);
        sumM2 += m * m; sumS2 += s * s;
        dcL += l; dcR += r;
        const float al = std::max (std::abs (l), std::abs (r));
        if (al >= 0.98855f) ++a.nearFullScale;                               // ≥ -0.1 dBFS
        // clipping: ≥ 3 consecutive (near-)identical samples at ≥ -0.1 dBFS (flat-topped waveform)
        if (al >= 0.98855f && std::abs (al - last) < 1.0e-4f) { if (++run == 2) ++a.clipRuns; }
        else run = 0;
        last = al;
        if (i % stStep == stStep - 1 && lm.getShortTerm() > -150.0) a.shortTermHistory.push_back ((float) lm.getShortTerm());
    }
    a.lufsI = lm.getIntegrated();
    a.lufsMMax = lm.getMaxMomentary(); a.lufsSMax = lm.getMaxShortTerm();
    a.lra = lm.getLoudnessRange();
    a.peakDb = gainToDb (tp.maxSamplePeak());
    a.truePeakDb = gainToDb (tp.maxTruePeak());
    a.intersampleOver = a.truePeakDb > 0.0 && a.peakDb <= 0.0;
    const double ms = 0.5 * (sumL2 + sumR2) / N;
    a.rmsDb = powToDb (ms);
    a.crestDb = a.peakDb - a.rmsDb;
    a.plr = a.lufsI > -150 ? a.truePeakDb - a.lufsI : 0.0;
    a.correlation = (sumL2 > 0 && sumR2 > 0) ? sumLR / std::sqrt (sumL2 * sumR2) : 1.0;
    a.midDb = powToDb (sumM2 / N); a.sideDb = powToDb (sumS2 / N);
    a.widthPct = sumM2 > 0 ? 100.0 * std::sqrt (sumS2 / sumM2) : 0.0;
    a.balanceDb = powToDb (sumL2 / N) - powToDb (sumR2 / N);
    {   // DC offset = a *constant* shift: per-second means must agree in sign and size across the song
        // (an asymmetric kick waveform has a non-zero short-term mean but is not an offset)
        const int sec = (int) sr; std::vector<double> means;
        for (int s0 = 0; s0 + sec <= N; s0 += sec)
        { double m = 0; for (int i = s0; i < s0 + sec; ++i) m += 0.5 * ((double) L[i] + R[i]); means.push_back (m / sec); }
        double med = 0;
        if (! means.empty()) { auto t = means; std::nth_element (t.begin(), t.begin() + (long) t.size() / 2, t.end()); med = t[t.size() / 2]; }
        int agree = 0; for (auto m : means) if (m * med > 0 && std::abs (m) > 0.5 * std::abs (med)) ++agree;
        a.dcOffset = (! means.empty() && agree >= (int) (0.9 * means.size())) ? std::abs (med) : 0.0;
        juce::ignoreUnused (dcL, dcR);
    }

    // short-term distribution (for density)
    if (! a.shortTermHistory.empty())
    {
        auto st = a.shortTermHistory; std::sort (st.begin(), st.end());
        a.stP10 = st[(size_t) (0.10 * (st.size() - 1))]; a.stP95 = st[(size_t) (0.95 * (st.size() - 1))];
        const float top = st.back();
        size_t near = 0; for (auto v : st) if (v > top - 2.0f) ++near;
        a.densityPct = 100.0 * (double) near / (double) st.size();
    }

    // transient crest: median peak/RMS over 50 ms windows that carry signal
    {
        const int w = std::max (64, (int) (0.05 * sr));
        std::vector<float> crest;
        for (int s0 = 0; s0 + w <= N; s0 += w)
        {
            double e = 0; float pk = 0;
            for (int i = s0; i < s0 + w; ++i) { const float m = 0.5f * (L[i] + R[i]); e += (double) m * m; pk = std::max (pk, std::abs (m)); }
            const double rms = std::sqrt (e / w);
            if (rms > 1.0e-3) crest.push_back ((float) (gainToDb (pk) - gainToDb ((float) rms)));
        }
        if (! crest.empty()) { std::nth_element (crest.begin(), crest.begin() + (long) crest.size() / 2, crest.end()); a.transientCrestDb = crest[crest.size() / 2]; }
    }

    // ---------------- long-term spectrum (Welch, Hann, 50 % overlap) of mid and side
    int order = 13; while ((1 << order) < sr / 6.0 && order < 16) ++order;   // ≤ 6 Hz resolution
    const int F = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> win ((size_t) F), work ((size_t) F * 2);
    for (int i = 0; i < F; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) F);
    std::vector<double> pM ((size_t) F / 2 + 1, 0.0), pS ((size_t) F / 2 + 1, 0.0);
    int frames = 0;
    for (int off = 0; off + F <= N; off += F / 2)
    {
        for (int pass = 0; pass < 2; ++pass)
        {
            std::fill (work.begin(), work.end(), 0.0f);
            for (int i = 0; i < F; ++i)
                work[(size_t) i] = (pass == 0 ? 0.5f * (L[off + i] + R[off + i]) : 0.5f * (L[off + i] - R[off + i])) * win[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (work.data(), true);
            auto& p = pass == 0 ? pM : pS;
            for (int k = 0; k <= F / 2; ++k) p[(size_t) k] += (double) work[(size_t) k] * work[(size_t) k];
        }
        ++frames;
    }
    if (frames > 0)
    {
        const double binHz = sr / F;
        double total = 0, cNum = 0, infra = 0;
        std::array<double, kAnBands> e {};
        double lowM = 0, lowS = 0;
        for (int k = 1; k <= F / 2; ++k)
        {
            const double f = k * binHz, pw = pM[(size_t) k] + pS[(size_t) k];
            if (f < 20.0) { infra += pM[(size_t) k] + pS[(size_t) k]; continue; }
            if (f > 20000.0) continue;
            total += pw; cNum += f * pw;
            for (int b = 0; b < kAnBands; ++b) if (f >= anBandLo (b) && f < anBandHi (b)) { e[(size_t) b] += pw; break; }
            if (f < 120.0) { lowM += pM[(size_t) k]; lowS += pS[(size_t) k]; }
        }
        a.centroidHz = total > 0 ? cNum / total : 0;
        a.infraPct = total > 0 ? 100.0 * infra / (total + infra) : 0;
        a.lowSideDb = lowM > 0 ? powToDb (lowS / lowM) : -200;
        for (int b = 0; b < kAnBands; ++b)
        {
            const double hi = std::min ((double) anBandHi (b), sr * 0.5);
            const double oct = std::max (0.1, std::log2 (hi / anBandLo (b)));
            a.bandDb[(size_t) b] = powToDb (e[(size_t) b] / oct / std::max (total, 1e-20));
            a.bandShare[(size_t) b] = total > 0 ? 100.0 * e[(size_t) b] / total : 0;
        }
        // ---- robust tonal line: Theil-Sen fit over 1/3-octave levels (power per octave, 100 Hz - 8 kHz).
        // The median of pairwise slopes ignores a few bumped bands, so a buildup cannot hide by
        // dragging the reference line toward itself.
        std::vector<double> tx, ty;                       // all 1/3-octave bands 20 Hz .. 20 kHz
        for (double fc = 20.0 * std::pow (2.0, 1.0 / 6.0); fc < 20000.0; fc *= std::pow (2.0, 1.0 / 3.0))
        {
            const double lo = fc * std::pow (2.0, -1.0 / 6.0), hi = std::min (fc * std::pow (2.0, 1.0 / 6.0), sr * 0.5);
            double pw = 0; for (int k = std::max (1, (int) std::ceil (lo / binHz)); k <= std::min (F / 2, (int) std::floor (hi / binHz)); ++k) pw += pM[(size_t) k] + pS[(size_t) k];
            if (hi <= lo) break;
            tx.push_back (std::log2 (fc)); ty.push_back (powToDb (pw / std::log2 (hi / lo) / std::max (total, 1e-20) + 1e-30));
        }
        // tilt = least-squares slope over 100 Hz - 8 kHz
        auto fitLine = [&] (double exclCentre, double exclHalfWidth, double& sl, double& ic)
        {
            double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
            for (size_t i = 0; i < tx.size(); ++i)
            {
                if (tx[i] < std::log2 (100.0) || tx[i] > std::log2 (8000.0) || std::abs (tx[i] - exclCentre) <= exclHalfWidth) continue;
                sx += tx[i]; sy += ty[i]; sxx += tx[i] * tx[i]; sxy += tx[i] * ty[i]; ++n;
            }
            sl = n > 1 ? (n * sxy - sx * sy) / std::max (1e-9, n * sxx - sx * sx) : 0.0;
            ic = n > 0 ? (sy - sl * sx) / n : 0.0;
        };
        double slope = 0, icpt = 0;
        fitLine (0.0, -1.0, slope, icpt);
        a.tiltDbPerOct = slope;
        // residual of each 1/3-octave band against a line fitted WITHOUT the surrounding +-0.7 octave,
        // so a bump never drags its own reference line along; then a 3-band (1-octave) average gives
        // the "prominence" used by the rules
        std::vector<double> res (tx.size(), 0.0);
        for (size_t i = 0; i < tx.size(); ++i) { double sl, ic; fitLine (tx[i], 0.7, sl, ic); res[i] = ty[i] - (ic + sl * tx[i]); }
        std::vector<double> sm (tx.size(), 0.0);
        for (size_t i = 0; i < tx.size(); ++i)
        {
            double s3 = 0; int c = 0;
            for (int d = -1; d <= 1; ++d) { const long j = (long) i + d; if (j >= 0 && j < (long) tx.size()) { s3 += res[(size_t) j]; ++c; } }
            sm[i] = s3 / c;
        }
        for (size_t i = 0; i < tx.size(); ++i) { a.thirdHz.push_back ((float) std::pow (2.0, tx[i])); a.thirdRes.push_back ((float) res[i]); }
        auto prominence = [&] (double lo, double hi, double& atHz)
        {
            double best = -1e9; atHz = std::sqrt (lo * hi);
            for (size_t i = 0; i < tx.size(); ++i) { const double f = std::pow (2.0, tx[i]); if (f >= lo && f < hi && sm[i] > best) { best = sm[i]; atHz = f; } }
            return best > -1e8 ? best : 0.0;
        };
        a.promLowMid = prominence (120.0, 500.0, a.lowMidPeakHz);
        a.promMid = prominence (500.0, 1000.0, a.midPeakHz);
        a.promPresence = prominence (2000.0, 5000.0, a.presencePeakHz);
        for (int b = 0; b < kAnBands; ++b)
        {
            double s2 = 0; int c = 0;
            for (size_t i = 0; i < tx.size(); ++i) { const double f = std::pow (2.0, tx[i]); if (f >= anBandLo (b) && f < anBandHi (b)) { s2 += res[i]; ++c; } }
            a.bandDev[(size_t) b] = c > 0 ? s2 / c : 0.0;
        }
        const double ref = 0.5 * (a.bandDb[2] + a.bandDb[3]);
        a.subRelDb = a.bandDb[0] - ref;
        a.bassRelDb = a.bandDb[1] - ref;

        // low-frequency correlation from the spectra: (M² - S²)/(M² + S²) below 120 Hz
        a.lowCorrelation = (lowM + lowS) > 0 ? (lowM - lowS) / (lowM + lowS) : 1.0;

        // 1/6-octave display spectrum (mid + side power, dB re. max)
        double mx = 1e-30;
        for (double f = 20.0; f <= 20000.0; f *= std::pow (2.0, 1.0 / 6.0))
        {
            const double lo = f * std::pow (2.0, -1.0 / 12.0), hi = f * std::pow (2.0, 1.0 / 12.0);
            double s = 0; int c = 0;
            for (int k = std::max (1, (int) (lo / binHz)); k <= std::min (F / 2, (int) (hi / binHz) + 1); ++k) { s += pM[(size_t) k] + pS[(size_t) k]; ++c; }
            const double v = c > 0 ? s / c : 0;
            a.avgSpectrumHz.push_back ((float) f); a.avgSpectrumDb.push_back ((float) v);
            mx = std::max (mx, v);
        }
        for (auto& v : a.avgSpectrumDb) v = (float) powToDb (v / mx);
    }
    a.valid = a.lufsI > -150.0;
    return a;
}

// ------------------------------------------------------------------------------------------------
static juce::String dbs (double v, int dp = 1) { return (v > 0.05 ? "+" : "") + juce::String (v, dp) + " dB"; }

Diagnosis diagnose (const AnalysisResult& a, float target, float ceiling)
{
    Diagnosis d;
    auto add = [&] (const char* cat, juce::String st, juce::String det, int lvl) { d.items.push_back ({ cat, st, det, lvl }); };
    if (! a.valid) { add ("ANALYSIS", "NO SIGNAL", "Play the mix so POLO can listen to it.", 1); return d; }

    // LOUDNESS
    const double dl = a.lufsI - target;
    add ("LOUDNESS", dl < -1.5 ? "BELOW TARGET" : dl > 1.5 ? "ABOVE TARGET" : "ON TARGET",
         juce::String (a.lufsI, 1) + " LUFS-I  (target " + juce::String (target, 1) + ")   LRA " + juce::String (a.lra, 1) + " LU", 0);

    // TRUE PEAK / HEADROOM
    {
        juce::String st = "SAFE"; int lvl = 0;
        if (a.truePeakDb > 0.0) { st = a.intersampleOver ? "OVER 0 dBTP (INTER-SAMPLE)" : "OVER 0 dBTP"; lvl = 2; }
        else if (a.truePeakDb > ceiling) { st = "HIGH"; lvl = 1; }
        add ("TRUE PEAK", st, juce::String (a.truePeakDb, 2) + " dBTP (4x est.)   sample peak " + juce::String (a.peakDb, 2) + " dBFS   headroom " + juce::String (-a.truePeakDb, 1) + " dB", lvl);
    }
    // CLIPPING
    add ("CLIPPING", a.clipRuns > 0 ? "DETECTED" : a.nearFullScale > 0 ? "NEAR FULL SCALE" : "NONE",
         a.clipRuns > 0 ? juce::String (a.clipRuns) + " flat-topped runs at full scale (clipped before this plugin)"
                        : a.nearFullScale > 0 ? juce::String ((juce::int64) a.nearFullScale) + " samples at or above -0.1 dBFS" : "no flat-topped peaks found",
         a.clipRuns > 0 ? 2 : a.nearFullScale > 0 ? 1 : 0);

    // DYNAMICS
    {
        juce::String st = "BALANCED"; int lvl = 0;
        if (a.plr < 6.5 && a.lufsI > -10.0) { st = "LIKELY OVER-LIMITED"; lvl = 2; }
        else if (a.plr < 8.5 || a.lra < 2.5) { st = "DENSE"; lvl = 1; }
        else if (a.plr > 15.0) { st = "VERY DYNAMIC"; lvl = 0; }
        else if (a.plr > 12.0) { st = "DYNAMIC"; lvl = 0; }
        add ("DYNAMICS", st, "PLR " + juce::String (a.plr, 1) + " dB   crest " + juce::String (a.crestDb, 1) + " dB   transient crest " + juce::String (a.transientCrestDb, 1) + " dB", lvl);
    }

    // LOW END
    {
        juce::String st = "CONTROLLED"; int lvl = 0;
        if (a.subRelDb > 14.0 && a.subRelDb > a.bassRelDb + 6.0) { st = "POSSIBLE EXCESS SUB"; lvl = 1; }
        else if (a.bassRelDb > 12.0) { st = "POSSIBLY HEAVY"; lvl = 1; }
        else if (a.subRelDb < -6.0 && a.bassRelDb < -3.0) { st = "LIGHT"; lvl = 1; }
        else if (a.subRelDb > 6.0 || a.bassRelDb > 5.0) st = "STRONG";
        if (a.infraPct > 2.0) { st << " / INFRASONIC ENERGY"; lvl = std::max (lvl, 1); }
        add ("LOW END", st, "sub " + dbs (a.subRelDb) + "  bass " + dbs (a.bassRelDb) + " vs 120-500 Hz", lvl);
    }
    // LOW-MID
    {
        const double lm = a.promLowMid;
        juce::String st = lm > 2.0 ? "POSSIBLE BUILDUP" : (a.bandDev[2] + a.bandDev[3]) * 0.5 < -3.0 ? "LIGHT" : "BALANCED";
        add ("LOW-MID", st, dbs (lm) + " around " + juce::String ((int) a.lowMidPeakHz) + " Hz above the mix's own tonal line", lm > 2.0 ? 1 : 0);
    }
    // PRESENCE / HIGH-MID
    {
        const double p = a.promPresence, avg = a.bandDev[6];
        add ("PRESENCE", p > 2.0 ? "POSSIBLY HARSH" : avg < -3.0 ? "POSSIBLY RECESSED" : "BALANCED", dbs (p) + " around " + juce::String ((int) a.presencePeakHz) + " Hz vs tonal line", (p > 2.0 || avg < -3.0) ? 1 : 0);
    }
    // HIGH END
    {
        const double h = a.bandDev[8], t = a.tiltDbPerOct;
        juce::String st = "BALANCED"; int lvl = 0;
        if (h < -12.0 || t < -6.0) { st = "POSSIBLY DULL"; lvl = 1; }
        else if (h > 0.5 || t > 0.0) { st = "POSSIBLY BRIGHT"; lvl = 1; }
        add ("HIGH END", st, "tilt " + juce::String (t, 1) + " dB/oct (100 Hz-8 kHz)   10-20 kHz " + dbs (h) + "   centroid " + juce::String ((int) a.centroidHz) + " Hz", lvl);
    }
    // STEREO
    {
        juce::String st; int lvl = 0;
        if (a.correlation < 0.0) { st = "PHASE ISSUE DETECTED"; lvl = 2; }
        else if (a.correlation < 0.25) { st = "WIDE / CHECK MONO"; lvl = 1; }
        else if (a.widthPct < 6.0) { st = "NEAR MONO"; lvl = 0; }
        else if (a.widthPct > 55.0) st = "WIDE / COMPATIBLE";
        else st = "COMPATIBLE";
        if (std::abs (a.balanceDb) > 1.5) { st << " / OFF-CENTRE"; lvl = std::max (lvl, 1); }
        add ("STEREO", st, "correlation " + juce::String (a.correlation, 2) + "   width " + juce::String (a.widthPct, 0) + "%   L/R " + dbs (a.balanceDb), lvl);
    }
    // LOW-FREQUENCY STEREO
    add ("LOW STEREO", a.lowSideDb > -12.0 ? "WIDE LOW END / CHECK MONO" : "MONO-COMPATIBLE",
         "side vs mid below 120 Hz: " + dbs (a.lowSideDb), a.lowSideDb > -12.0 ? 1 : 0);
    // DC
    if (a.dcOffset > 0.001) add ("DC OFFSET", "DETECTED", juce::String (gainToDb ((float) a.dcOffset), 1) + " dBFS mean offset (removed by the DC filter)", 1);

    // VOCAL REGION (estimate — not source separation)
    {
        const double v = 0.5 * (a.bandDev[5] + a.bandDev[6]);
        d.vocalRegion = v < -2.5 ? "POSSIBLY RECESSED" : v > 2.5 ? "DENSE" : "BALANCED";
        add ("VOCAL REGION", d.vocalRegion + " (ESTIMATE)", "1-5 kHz " + dbs (v) + " vs tonal line - broad frequency estimate, not vocal isolation", std::abs (v) > 2.5 ? 1 : 0);
    }
    // MASKING (frequency-domain only)
    {
        juce::StringArray dense;
        auto region = [] (double hz) { return juce::String ((int) (hz * 0.8)) + "-" + juce::String ((int) (hz * 1.25)) + " Hz"; };
        if (a.promLowMid > 2.0) dense.add (region (a.lowMidPeakHz));
        if (a.promMid > 2.5) dense.add (region (a.midPeakHz));
        if (a.promPresence > 2.0) dense.add (region (a.presencePeakHz));
        d.masking = dense.isEmpty() ? "NO DENSE REGION FOUND" : "POSSIBLE MASKING " + dense.joinIntoString (", ");
        if (! dense.isEmpty()) add ("MASKING", "POSSIBLE", dense.joinIntoString (", ") + " carry more energy than the surrounding spectrum (frequency-domain estimate)", 1);
    }

    // ---------------- NEXT STEPS (what to *check*, most important first)
    auto& n = d.nextSteps;
    if (a.clipRuns > 0) n.add ("Clipping is already in the mix (" + juce::String (a.clipRuns) + " flat-topped runs). Lower the mix bus or find the clipping track before mastering.");
    if (a.correlation < 0.0) n.add ("Correlation is negative: check for a polarity-flipped or phase-shifted element. Listen in MONO.");
    if (a.lowSideDb > -12.0) n.add ("There is stereo content below 120 Hz. Check the 808/kick in MONO; LOW FREQUENCY MONO keeps it centred.");
    if (a.promLowMid > 2.0) n.add ("Check around " + juce::String ((int) a.lowMidPeakHz) + " Hz for possible low-mid buildup (often bass, keys or vocal body overlapping).");
    if (a.promPresence > 2.0) n.add ("Around " + juce::String ((int) a.presencePeakHz) + " Hz is forward. Check hi-hats, snares and vocal sibilance for harshness at high volume.");
    if (0.5 * (a.bandDev[5] + a.bandDev[6]) < -2.5) n.add ("The 1-5 kHz region is low. Compare vocal presence against the instrumental - it may be sitting behind the beat.");
    if (a.infraPct > 2.0) n.add (juce::String (a.infraPct, 1) + "% of the energy is below 20 Hz (rumble or DC drift). The SUBSONIC filter removes it.");
    if (a.subRelDb > 14.0 && a.subRelDb > a.bassRelDb + 6.0) n.add ("Sub energy is high relative to the bass. Check the 808 level on small speakers and in the car.");
    if (a.plr < 8.5) n.add ("The mix is already dense (PLR " + juce::String (a.plr, 1) + " dB). Check whether bus limiting is reducing transient impact before adding more.");
    if (a.truePeakDb > ceiling) n.add ("True peak is " + juce::String (a.truePeakDb, 1) + " dBTP. Recheck true peak after final limiting.");
    if (a.dcOffset > 0.001) n.add ("DC offset detected. Check the source tracks (the DC filter removes it here).");
    if (std::abs (a.balanceDb) > 1.5) n.add ("The mix leans " + juce::String (a.balanceDb > 0 ? "left" : "right") + " by " + juce::String (std::abs (a.balanceDb), 1) + " dB. Check panning of the main elements.");
    if (n.isEmpty()) n.add ("No technical problem stood out. Trust your ears: compare against a reference at matched loudness.");
    n.add ("Verify mono compatibility with MONO, and check translation with SMALL SPEAKER.");
    return d;
}

FinalCheck finalCheck (const AnalysisResult& p, float target, float ceiling, bool monitorFull, bool bypassed)
{
    FinalCheck f;
    if (! p.valid) { f.states.add ("CHECK MIX"); f.details.add ("No processed audio to check."); return f; }
    if (p.truePeakDb > ceiling + 0.3 || p.truePeakDb > -0.1) { f.states.add ("PEAKS TOO HIGH"); f.details.add ("True peak " + juce::String (p.truePeakDb, 2) + " dBTP vs ceiling " + juce::String (ceiling, 1) + " dBTP."); }
    if (p.lowSideDb > -10.0 || p.correlation < 0.0) { f.states.add ("LOW-END COMPATIBILITY ISSUE"); f.details.add ("Side energy below 120 Hz " + juce::String (p.lowSideDb, 1) + " dB, correlation " + juce::String (p.correlation, 2) + "."); }
    if (p.plr < 6.0 || (p.lra < 1.5 && p.lufsI > -10.0)) { f.states.add ("EXCESSIVE DENSITY"); f.details.add ("PLR " + juce::String (p.plr, 1) + " dB, LRA " + juce::String (p.lra, 1) + " LU."); }
    if (p.clipRuns > 0) { f.states.add ("CHECK MIX"); f.details.add (juce::String (p.clipRuns) + " flat-topped runs in the output."); }
    if (std::abs (p.lufsI - target) > 2.0) f.details.add ("Integrated loudness " + juce::String (p.lufsI, 1) + " LUFS (target " + juce::String (target, 1) + ") - informational.");
    if (! monitorFull) f.details.add ("MONITOR is not FULL - switch back to FULL before exporting.");
    if (bypassed) f.details.add ("The plugin is bypassed / listening to ORIGINAL - switch back before exporting.");
    f.ready = f.states.isEmpty();
    if (f.ready) { f.states.add ("READY FOR EXPORT"); f.details.insert (0, "True peak " + juce::String (p.truePeakDb, 2) + " dBTP, " + juce::String (p.lufsI, 1) + " LUFS-I, PLR " + juce::String (p.plr, 1) + " dB, correlation " + juce::String (p.correlation, 2) + "."); }
    return f;
}
} // namespace prl
