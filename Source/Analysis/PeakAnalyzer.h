#pragma once
// POLOOYX RELEASE ENGINE — sample peak + inter-sample ("true") peak estimation.
//
// True peak is estimated the way ITU-R BS.1770-4 Annex 2 describes: the signal is upsampled with a
// polyphase windowed-sinc interpolator (12 taps per phase) and the largest absolute value of all
// phases is taken. 4x is the BS.1770 reference; 2x and 8x are offered as cheaper / finer variants.
// This is an *estimate*: a finite interpolator cannot reconstruct every inter-sample peak exactly
// (BS.1770 allows +0.2/-0.4 dB at 48 kHz). The UI labels the reading "dBTP (4x est.)" accordingly.

#include "../DSP/DspCommon.h"

namespace prl
{
class TruePeakDetector
{
public:
    static constexpr int kTapsPerPhase = 12;

    void prepare (int factor)
    {
        F = std::max (1, factor);
        const int L = kTapsPerPhase * F;
        coef.assign ((size_t) L, 0.0f);
        if (F > 1)
        {
            const double mid = 0.5 * (L - 1);
            const double beta = 6.0;   // Kaiser window
            auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
            for (int n = 0; n < L; ++n)
            {
                const double t = (n - mid) / (double) F;
                const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (kPiD * t) / (kPiD * t);
                const double r = (n - mid) / mid;
                const double w = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
                coef[(size_t) n] = (float) (sinc * w);
            }
            // normalise every phase to unity DC gain
            for (int p = 0; p < F; ++p)
            {
                double s = 0; for (int j = 0; j < kTapsPerPhase; ++j) s += coef[(size_t) (p + F * j)];
                for (int j = 0; j < kTapsPerPhase; ++j) coef[(size_t) (p + F * j)] = (float) (coef[(size_t) (p + F * j)] / s);
            }
        }
        hist.fill (0.0f);
        pos = 0;
    }

    void reset() noexcept { hist.fill (0.0f); pos = 0; }

    // group delay of the interpolator in input samples (the estimate for sample t refers to ≈ t - latency)
    int latency() const noexcept { return F > 1 ? kTapsPerPhase / 2 : 0; }

    // feed one sample, return the largest |value| among the F interpolated points for this step
    inline float process (float x) noexcept
    {
        pos = (pos + 1) % kTapsPerPhase;
        hist[(size_t) pos] = x;
        if (F == 1) return std::abs (x);
        float m = 0.0f;
        for (int p = 0; p < F; ++p)
        {
            float y = 0.0f;
            int idx = pos;
            for (int j = 0; j < kTapsPerPhase; ++j)
            {
                y += coef[(size_t) (p + F * j)] * hist[(size_t) idx];
                idx = idx == 0 ? kTapsPerPhase - 1 : idx - 1;
            }
            m = std::max (m, std::abs (y));
        }
        return m;
    }

    int factor() const noexcept { return F; }

private:
    int F = 4, pos = 0;
    std::vector<float> coef;
    std::array<float, kTapsPerPhase> hist {};
};

// stereo true-peak meter with hold (audio thread writes, UI reads atomically via the processor)
class TruePeakMeter
{
public:
    void prepare (int factor) { for (auto& d : det) d.prepare (factor); reset(); }
    void reset() noexcept { for (auto& d : det) d.reset(); maxTp = maxSp = 0.0f; blockTp = blockSp = 0.0f; }
    inline void process (float l, float r) noexcept
    {
        const float tp = std::max (det[0].process (l), det[1].process (r));
        const float sp = std::max (std::abs (l), std::abs (r));
        blockTp = std::max (blockTp, std::max (tp, sp)); blockSp = std::max (blockSp, sp);
        maxTp = std::max (maxTp, std::max (tp, sp));     maxSp = std::max (maxSp, sp);
    }
    // block values (reset after reading)
    float takeBlockTp() noexcept { const float v = blockTp; blockTp = 0.0f; return v; }
    float takeBlockSp() noexcept { const float v = blockSp; blockSp = 0.0f; return v; }
    float maxTruePeak() const noexcept { return maxTp; }
    float maxSamplePeak() const noexcept { return maxSp; }
private:
    std::array<TruePeakDetector, 2> det;
    float maxTp = 0, maxSp = 0, blockTp = 0, blockSp = 0;
};
} // namespace prl
