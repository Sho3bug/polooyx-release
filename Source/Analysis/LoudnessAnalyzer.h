#pragma once
// POLOOYX RELEASE ENGINE — ITU-R BS.1770-4 / EBU R128 loudness.
//
//  * K-weighting: the two BS.1770 biquads, re-derived for the running sample rate (same closed
//    form libebur128 uses), so 44.1 … 192 kHz all measure correctly.
//  * Momentary  = 400 ms window, Short-term = 3 s window, updated every 100 ms.
//  * Integrated = 400 ms blocks with 75 % overlap, absolute gate -70 LUFS, relative gate -10 LU.
//  * Loudness range (EBU Tech 3342) = 10th..95th percentile of gated short-term values (rel. gate -20 LU).
// Gating uses fixed 0.05 LU histograms, so the meter never allocates on the audio thread and can
// run for hours. The histogram quantisation error on the integrated value is < 0.01 LU.

#include "../DSP/DspCommon.h"

namespace prl
{
class KWeighting
{
public:
    void prepare (double fs)
    {
        {   // stage 1: high-shelf
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPiD * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            s1 = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                   2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {   // stage 2: RLB high-pass
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPiD * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            s2 = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        reset();
    }
    void reset() noexcept { z1a = z2a = z1b = z2b = 0.0; }
    inline double process (double x) noexcept
    {
        double y = s1.b0 * x + z1a;  z1a = s1.b1 * x - s1.a1 * y + z2a;  z2a = s1.b2 * x - s1.a2 * y;
        double w = s2.b0 * y + z1b;  z1b = s2.b1 * y - s2.a1 * w + z2b;  z2b = s2.b2 * y - s2.a2 * w;
        return w;
    }
private:
    struct Bq { double b0, b1, b2, a1, a2; } s1 {}, s2 {};
    double z1a = 0, z2a = 0, z1b = 0, z2b = 0;
};

// Histogram over loudness values (-70 .. +10 LUFS in 0.05 LU steps) holding counts and energies.
class LoudnessHistogram
{
public:
    static constexpr double kMin = -70.0, kStep = 0.05;
    static constexpr int kBins = 1600;
    void clear() noexcept { cnt.fill (0); en.fill (0.0); total = 0; }
    void add (double energy) noexcept
    {
        const double l = -0.691 + powToDb (energy);
        if (l <= kMin) return;               // absolute gate
        int b = (int) ((l - kMin) / kStep);
        b = std::min (kBins - 1, std::max (0, b));
        ++cnt[(size_t) b]; en[(size_t) b] += energy; ++total;
    }
    int64_t count() const noexcept { return total; }

    // gated integrated loudness (relative gate in LU below the abs-gated mean); -inf → returns -200
    double gatedLoudness (double relGateLU) const noexcept
    {
        if (total == 0) return -200.0;
        double s = 0; int64_t n = 0;
        for (int b = 0; b < kBins; ++b) { s += en[(size_t) b]; n += cnt[(size_t) b]; }
        const double rel = -0.691 + powToDb (s / (double) n) + relGateLU;
        double s2 = 0; int64_t n2 = 0;
        for (int b = 0; b < kBins; ++b)
        {
            const double lo = kMin + b * kStep;
            if (lo + kStep * 0.5 > rel) { s2 += en[(size_t) b]; n2 += cnt[(size_t) b]; }
        }
        if (n2 == 0) return -200.0;
        return -0.691 + powToDb (s2 / (double) n2);
    }

    // percentile of the values above the relative gate (for LRA)
    double percentileAboveGate (double relGateLU, double pct) const noexcept
    {
        if (total == 0) return -200.0;
        double s = 0; int64_t n = 0;
        for (int b = 0; b < kBins; ++b) { s += en[(size_t) b]; n += cnt[(size_t) b]; }
        const double rel = -0.691 + powToDb (s / (double) n) + relGateLU;
        int64_t n2 = 0; int first = kBins;
        for (int b = 0; b < kBins; ++b) if (kMin + (b + 0.5) * kStep > rel) { n2 += cnt[(size_t) b]; first = std::min (first, b); }
        if (n2 == 0) return -200.0;
        const int64_t want = (int64_t) std::floor (pct * (double) (n2 - 1));
        int64_t acc = 0;
        for (int b = first; b < kBins; ++b)
        {
            acc += cnt[(size_t) b];
            if (acc > want) return kMin + (b + 0.5) * kStep;
        }
        return kMin + (kBins - 0.5) * kStep;
    }
private:
    std::array<int64_t, kBins> cnt {};
    std::array<double, kBins> en {};
    int64_t total = 0;
};

class LoudnessMeter
{
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& k : kw) k.prepare (fs);
        step = std::max (1, (int) std::lround (fs * 0.1));   // 100 ms
        reset();
    }
    void reset() noexcept
    {
        for (auto& k : kw) k.reset();
        ring.fill (0.0); ringPos = 0; filled = 0; acc = 0.0; accN = 0;
        integrated.clear(); shortHist.clear();
        momentary = shortTerm = -200.0; maxMomentary = maxShort = -200.0;
        updated = false;
    }
    void resetIntegrated() noexcept { integrated.clear(); shortHist.clear(); maxMomentary = maxShort = -200.0; }

    // channel weights: L/R = 1.0 (BS.1770). Mono input: pass the same value as r with mono=true.
    inline void process (float l, float r, bool mono = false) noexcept
    {
        const double a = kw[0].process (l);
        double e = a * a;
        if (! mono) { const double b = kw[1].process (r); e += b * b; }
        acc += e;
        if (++accN >= step)
        {
            ring[(size_t) ringPos] = acc / (double) accN;   // mean square of this 100 ms step
            ringPos = (ringPos + 1) % kRing;
            filled = std::min (filled + 1, kRing);
            acc = 0.0; accN = 0;
            const double m = meanOf (4), s = meanOf (30);
            momentary = filled >= 4 ? -0.691 + powToDb (m) : -200.0;
            shortTerm = filled >= 30 ? -0.691 + powToDb (s) : -200.0;
            if (filled >= 4)  { integrated.add (m); maxMomentary = std::max (maxMomentary, momentary); }
            if (filled >= 30) { shortHist.add (s); maxShort = std::max (maxShort, shortTerm); }
            updated = true;
        }
    }

    double getMomentary() const noexcept  { return momentary; }
    double getShortTerm() const noexcept  { return shortTerm; }
    double getIntegrated() const noexcept { return integrated.gatedLoudness (-10.0); }
    double getLoudnessRange() const noexcept
    {
        if (shortHist.count() < 10) return 0.0;
        return std::max (0.0, shortHist.percentileAboveGate (-20.0, 0.95) - shortHist.percentileAboveGate (-20.0, 0.10));
    }
    double getMaxMomentary() const noexcept { return maxMomentary; }
    double getMaxShortTerm() const noexcept { return maxShort; }
    int64_t integratedBlocks() const noexcept { return integrated.count(); }
    bool takeUpdated() noexcept { const bool u = updated; updated = false; return u; }

private:
    static constexpr int kRing = 30;
    double meanOf (int steps) const noexcept
    {
        const int n = std::min (steps, filled);
        if (n == 0) return 0.0;
        double s = 0;
        for (int i = 1; i <= n; ++i) s += ring[(size_t) ((ringPos - i + kRing) % kRing)];
        return s / (double) steps;   // BS.1770: window mean (zero-padded until full)
    }

    double sr = 48000;
    std::array<KWeighting, 2> kw;
    std::array<double, kRing> ring {};
    int ringPos = 0, filled = 0, step = 4800, accN = 0;
    double acc = 0;
    LoudnessHistogram integrated, shortHist;
    double momentary = -200, shortTerm = -200, maxMomentary = -200, maxShort = -200;
    bool updated = false;
};
} // namespace prl
