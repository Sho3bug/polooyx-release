#pragma once
// POLOOYX RELEASE ENGINE — harmonic color (runs inside the oversampled section).
//
// Final-mix coloration, deliberately far gentler than vocal distortion. Each waveshaper is
// normalised to unity small-signal gain (y = f(d·x)/d), so DRIVE changes harmonic content rather
// than level; MIX blends it in parallel. Modes:
//   CLEAN   – no waveshaping at all (TONE / OUTPUT only)
//   TAPE    – symmetric soft saturation with a slight HF softening of the harmonics (odd, smooth)
//   TUBE    – soft odd curve plus an even (1 - sech) bend → 2nd harmonic dominant, DC removed afterwards
//   CONSOLE – gentle cubic-style odd harmonics that only grow on loud material
//   DIGITAL – crisp sine-fold-free "hard-soft" knee (brighter odd harmonics), still band-limited by OS

#include "DspCommon.h"
#include "../Params.h"

namespace prl
{
class Saturation
{
public:
    struct Settings { ColorMode mode = ColorMode::Tape; float drive = 0.3f, tone = 0, mix = 0, outDb = 0, amount = 1; };

    void prepare (double osRate) { sr = (float) osRate; reset(); }
    void reset() noexcept
    {
        for (auto& f : harmLp) f.reset();
        for (auto& t : toneLo) t.reset();
        for (auto& t : toneHi) t.reset();
        dcX.fill (0.0f); dcY.fill (0.0f);
        mixR.snap (0.0f); outR.snap (1.0f);
    }

    static inline float shape (ColorMode m, float x, float d) noexcept
    {
        const float u = d * x;
        switch (m)
        {
            case ColorMode::Clean:   return x;
            case ColorMode::Tape:    return fastTanh (u) / d;
            case ColorMode::Tube:    return (2.0f * fastTanh (0.5f * u) + 0.8f * (1.0f - 1.0f / std::cosh (std::min (std::abs (u), 20.0f)))) / d;   // odd soft part + even (sech) bend → 2nd harmonic dominant
            case ColorMode::Console: { const float c = clampf (u, -1.5f, 1.5f); return (c - c * c * c / 6.75f) / (d * 1.0f); }
            case ColorMode::Digital: { const float a = std::abs (u); const float y = a < 0.7f ? a : 0.7f + 0.3f * fastTanh ((a - 0.7f) / 0.3f); return (u < 0 ? -y : y) / d; }
        }
        return x;
    }

    void process (float* const* ch, int n, const Settings& s) noexcept
    {
        mixR.set (s.mix * s.amount); outR.set (dbToGain (s.outDb * s.amount));
        if (mixR.from <= 0.0f && mixR.to <= 0.0f && outR.from == 1.0f && outR.to == 1.0f)
        { if (active) reset(); active = false; return; }
        active = true;
        const float d = 1.0f + 7.0f * std::pow (clamp01 (s.drive), 1.5f);
        const float toneDb = 3.0f * s.tone;
        const float R1 = 1.0f - (2.0f * kPi * 6.0f / sr);
        for (int c = 0; c < 2; ++c)
        {
            harmLp[(size_t) c].set (Svf::Mode::LowPass, 16000.0f, 0.6f, 0, sr);
            toneLo[(size_t) c].set (Svf::Mode::LowShelf, 300.0f, 0.6f, -toneDb, sr);
            toneHi[(size_t) c].set (Svf::Mode::HighShelf, 3000.0f, 0.6f, toneDb, sr);
        }
        const bool toneOn = std::abs (toneDb) > 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const float m = mixR.at (i, n), o = outR.at (i, n);
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                float w = shape (s.mode, x, d);
                if (s.mode == ColorMode::Tube)   // remove the DC the asymmetry creates
                {
                    const float y = w - dcX[(size_t) c] + R1 * dcY[(size_t) c];
                    dcX[(size_t) c] = w; dcY[(size_t) c] = y; w = y;
                }
                if (s.mode == ColorMode::Tape) w = x + harmLp[(size_t) c].process (w - x);   // soften only the added harmonics
                if (toneOn) w = toneHi[(size_t) c].process (toneLo[(size_t) c].process (w));
                ch[c][i] = (x + m * (w - x)) * o;
            }
        }
    }

private:
    float sr = 96000;
    std::array<Svf, 2> harmLp, toneLo, toneHi;
    std::array<float, 2> dcX {}, dcY {};
    Ramp mixR, outR;
    bool active = false;
};
} // namespace prl
