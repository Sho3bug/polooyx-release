#pragma once
// POLOOYX RELEASE ENGINE — PUNCH (transient / sustain shaping).
//
// Transient detection compares a fast and a slow envelope of the mid signal (difference in dB is
// large at attacks, ~0 during sustain). TRANSIENT scales attacks up/down, BODY scales the sustain,
// IMPACT adds attack emphasis only to the low band (< 150 Hz), detected on that band itself, so
// kick hits gain impact while long 808 tails are left alone. Gains are bounded (±5 dB transient,
// ±3 dB body, +4 dB impact) and applied stereo-linked, which avoids image shift and pumping.

#include "DspCommon.h"

namespace prl
{
class Punch
{
public:
    struct Settings { float transient = 0, body = 0, impact = 0, mix = 1, amount = 1; };   // transient/body -1..1, impact/mix 0..1

    void prepare (double fs)
    {
        sr = (float) fs;
        fast.set (0.8f, 40.0f, sr);  slow.set (25.0f, 220.0f, sr);
        fastLo.set (2.0f, 60.0f, sr); slowLo.set (40.0f, 260.0f, sr);
        for (auto& f : lo) f.set (150.0f, sr);
        gSmooth.set (0.3f, 15.0f, sr); gSmoothLo.set (0.5f, 20.0f, sr);
        reset();
    }
    void reset() noexcept
    {
        fast.reset(); slow.reset(); fastLo.reset(); slowLo.reset();
        for (auto& f : lo) f.reset();
        gSmooth.reset (0.0f); gSmoothLo.reset (0.0f);
        amt.snap (0.0f);
        lastGainDb = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount);
        if (amt.from <= 0.0f && amt.to <= 0.0f) { if (active) reset(); active = false; return; }
        active = true;
        const bool any = std::abs (s.transient) > 0.001f || std::abs (s.body) > 0.001f || s.impact > 0.001f;
        float peakGain = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lowL = lo[0].process (L[i]), lowR = lo[1].process (R[i]);
            if (! any && amt.from == amt.to) continue;   // keep crossover state current, leave audio untouched
            const float m = 0.5f * std::abs (L[i] + R[i]);
            const float td = std::max (0.0f, gainToDb (fast.process (m) + 1e-7f) - gainToDb (slow.process (m) + 1e-7f));
            const float ml = 0.5f * std::abs (lowL + lowR);
            const float tdl = std::max (0.0f, gainToDb (fastLo.process (ml) + 1e-7f) - gainToDb (slowLo.process (ml) + 1e-7f));

            const float tNorm = std::min (td, 9.0f) / 9.0f;                   // 0 sustain .. 1 sharp attack
            float gDb = s.transient * 5.0f * tNorm + s.body * 3.0f * (1.0f - std::min (1.0f, td / 4.0f));
            float gLoDb = s.impact * 4.0f * std::min (tdl, 9.0f) / 9.0f;
            gDb   = gSmooth.process (gDb);
            gLoDb = gSmoothLo.process (gLoDb);
            const float a = amt.at (i, n) * s.mix;
            const float g = 1.0f + a * (dbToGain (gDb) - 1.0f);
            const float gl = a * (dbToGain (gLoDb) - 1.0f);                   // extra gain on the low band only
            L[i] = L[i] * g + lowL * gl * g;
            R[i] = R[i] * g + lowR * gl * g;
            peakGain = std::max (peakGain, std::abs (gDb) + gLoDb);
        }
        lastGainDb = peakGain;
    }

    float lastGainDb = 0.0f;   // telemetry: largest gain change in the last block (dB)

private:
    float sr = 48000;
    LinEnvelope fast, slow, fastLo, slowLo;
    std::array<LR4LowPass, 2> lo;
    DbEnvelope gSmooth, gSmoothLo;
    Ramp amt;
    bool active = false;
};
} // namespace prl
