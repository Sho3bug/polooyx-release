#pragma once
// POLOOYX RELEASE ENGINE — input gain, DC-offset removal, subsonic filter.

#include "DspCommon.h"

namespace prl
{
class GainStage
{
public:
    struct Settings { float gainDb = 0; float dcAmt = 1; float subAmt = 1; float subHz = 18; };

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept
    {
        for (auto& f : sub) for (auto& s : f) s.reset();
        dcX.fill (0.0f); dcY.fill (0.0f);
        gain.snap (1.0f); dc.snap (0.0f); subMix.snap (0.0f);
        dcMeanL = dcMeanR = 0.0;
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        gain.set (dbToGain (s.gainDb)); dc.set (s.dcAmt); subMix.set (s.subAmt);
        for (int c = 0; c < 2; ++c)
        {
            sub[(size_t) c][0].set (Svf::Mode::HighPass, s.subHz, 0.5412f, 0, sr);   // 4th-order Butterworth
            sub[(size_t) c][1].set (Svf::Mode::HighPass, s.subHz, 1.3066f, 0, sr);
        }
        const float R1 = 1.0f - (2.0f * kPi * 4.0f / sr);   // DC blocker pole (~4 Hz)
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.at (i, n), d = dc.at (i, n), sm = subMix.at (i, n);
            for (int c = 0; c < 2; ++c)
            {
                float x = ch[c][i] * g;
                // DC offset measurement (slow mean, for diagnostics) + removal
                (c == 0 ? dcMeanL : dcMeanR) += 0.00002 * ((double) x - (c == 0 ? dcMeanL : dcMeanR));
                const float y = x - dcX[(size_t) c] + R1 * dcY[(size_t) c];
                dcX[(size_t) c] = x; dcY[(size_t) c] = y;
                x = x + d * (y - x);
                const float h = sub[(size_t) c][1].process (sub[(size_t) c][0].process (x));
                ch[c][i] = x + sm * (h - x);
            }
        }
    }

    double dcOffset() const noexcept { return 0.5 * (std::abs (dcMeanL) + std::abs (dcMeanR)); }

private:
    float sr = 48000;
    std::array<std::array<Svf, 2>, 2> sub;
    std::array<float, 2> dcX {}, dcY {};
    Ramp gain, dc, subMix;
    double dcMeanL = 0, dcMeanR = 0;
};
} // namespace prl
