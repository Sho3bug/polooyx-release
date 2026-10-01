#pragma once
// POLOOYX RELEASE ENGINE — Mid/Side stereo engine.
//
//  M = (L+R)/2, S = (L-R)/2.
//  LOW-FREQUENCY MONO: the side signal is high-passed with an LR4 filter at MONO BELOW (side content
//  under it is removed; the mid channel is never touched).
//  Per-band width: when LOW / MID / HIGH width differ, the side is split with an LR4 crossover tree
//  (all-pass compensated → flat when the widths are equal); when they are equal no filter is in the
//  path at all, so default settings are bit-transparent.
//  MID / SIDE level trims follow.

#include "DspCommon.h"

namespace prl
{
class StereoEngine
{
public:
    struct Settings { float width = 1, lowWidth = 1, highWidth = 1, xoLow = 250, xoHigh = 5000, midDb = 0, sideDb = 0;
                      float monoBass = 0, monoHz = 80, amount = 1; };

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept
    {
        monoHp.reset(); lpA.reset(); hpA.reset(); lpB.reset(); hpB.reset(); apB.reset();
        amt.snap (0.0f); wR.snap (1.0f); lwR.snap (1.0f); hwR.snap (1.0f); mR.snap (1.0f); sR.snap (1.0f); monoR.snap (0.0f); splitR.snap (0.0f);
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        const bool split = std::abs (s.lowWidth - s.width) > 1.0e-4f || std::abs (s.highWidth - s.width) > 1.0e-4f;
        amt.set (s.amount); wR.set (s.width); lwR.set (s.lowWidth); hwR.set (s.highWidth);
        mR.set (dbToGain (s.midDb)); sR.set (dbToGain (s.sideDb)); monoR.set (s.monoBass); splitR.set (split ? 1.0f : 0.0f);
        const float xb = std::max (s.xoHigh, s.xoLow * 1.5f);
        monoHp.set (s.monoHz, sr); lpA.set (s.xoLow, sr); hpA.set (s.xoLow, sr); lpB.set (xb, sr); hpB.set (xb, sr); apB.set (xb, sr);
        for (int i = 0; i < n; ++i)
        {
            const float M = 0.5f * (L[i] + R[i]);
            const float S = 0.5f * (L[i] - R[i]);
            // filters always run so their state is current when engaged
            const float sHp = monoHp.process (S);
            const float S2 = S + monoR.at (i, n) * (sHp - S);
            const float sLow = apB.process (lpA.process (S2));
            const float rest = hpA.process (S2);
            const float sMid = lpB.process (rest), sHigh = hpB.process (rest);
            const float w = wR.at (i, n);
            const float direct = S2 * w;
            const float banded = sLow * lwR.at (i, n) + sMid * w + sHigh * hwR.at (i, n);
            const float Sw = (direct + splitR.at (i, n) * (banded - direct)) * sR.at (i, n);
            const float a = amt.at (i, n);
            const float Mo = M + a * (M * mR.at (i, n) - M), So = S + a * (Sw - S);
            L[i] = Mo + So; R[i] = Mo - So;
        }
    }

private:
    float sr = 48000;
    LR4HighPass monoHp, hpA, hpB;
    LR4LowPass lpA, lpB;
    LR4AllPass apB;
    Ramp amt, wR, lwR, hwR, mR, sR, monoR, splitR;
};
} // namespace prl
