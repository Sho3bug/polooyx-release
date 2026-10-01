#pragma once
// POLOOYX RELEASE ENGINE — glue (bus) compressor.
// Stereo-linked feed-forward compressor with an RMS-style detector (smoother than peak, so it
// "glues" rather than grabs), log-domain attack/release, soft knee, makeup and parallel mix.

#include "DspCommon.h"

namespace prl
{
class GlueCompressor
{
public:
    struct Settings { float thrDb = 0, ratio = 2, attMs = 30, relMs = 200, kneeDb = 6, makeupDb = 0, mix = 1, amount = 1; };

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept { env.reset(); rms.reset(); amt.snap (0.0f); mix.snap (1.0f); grDb = 0.0f; }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount); mix.set (s.mix);
        if (amt.from <= 0.0f && amt.to <= 0.0f) { if (active) reset(); active = false; grDb = 0.0f; return; }
        active = true;
        env.set (s.attMs, s.relMs, sr);
        rms.set (10.0f, 10.0f, sr);   // true mean-square (symmetric 10 ms) of the louder channel
        const float makeup = dbToGain (s.makeupDb);
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (L[i] * L[i], R[i] * R[i]);
            const float lvl = 10.0f * std::log10 (rms.process (pk) + 1.0e-12f);       // RMS-ish power in dB
            const float e = env.process (lvl);
            const float gr = gainReductionDb (e, s.thrDb, s.ratio, s.kneeDb);
            const float m = mix.at (i, n), a = amt.at (i, n);
            maxGr = std::max (maxGr, gr * m * a);
            const float g = dbToGain (-gr) * makeup;
            const float gg = 1.0f + a * m * (g - 1.0f);
            L[i] *= gg; R[i] *= gg;
        }
        grDb = maxGr;
    }

    float grDb = 0.0f;

private:
    float sr = 48000;
    DbEnvelope env;
    LinEnvelope rms;
    Ramp amt, mix;
    bool active = false;
};
} // namespace prl
