#pragma once
// POLOOYX RELEASE ENGINE — soft clipper (runs inside the oversampled section).
// Peak shaping before the limiter: shaving the sharpest peaks here lets the limiter work less.
//   SOFT – linear up to 2 dB below the ceiling, then a smooth knee that reaches the ceiling
//   TAPE – tanh-style curve scaled to the ceiling (rounder, slightly denser)
//   HARD – flat clamp at the ceiling (most transparent for very short peaks when oversampled)

#include "DspCommon.h"
#include "../Params.h"

namespace prl
{
class Clipper
{
public:
    struct Settings { ClipMode mode = ClipMode::Soft; float driveDb = 0, ceilingDb = -0.3f, mix = 1, amount = 1; };

    void reset() noexcept { amt.snap (0.0f); drive.snap (1.0f); clipDb = 0.0f; }

    static inline float shape (ClipMode m, float x, float c) noexcept
    {
        const float a = std::abs (x);
        float y = a;
        switch (m)
        {
            case ClipMode::Soft:
            {
                const float k = c * 0.794f;                   // knee starts 2 dB below the ceiling
                if (a > k) y = k + (c - k) * fastTanh ((a - k) / (c - k));
                break;
            }
            case ClipMode::Tape: y = c * fastTanh (a / c); break;
            case ClipMode::Hard: y = std::min (a, c); break;
        }
        return x < 0 ? -y : y;
    }

    void process (float* const* ch, int n, const Settings& s) noexcept
    {
        amt.set (s.amount * s.mix); drive.set (dbToGain (s.driveDb));
        if (amt.from <= 0.0f && amt.to <= 0.0f) { clipDb = 0.0f; return; }
        const float c = dbToGain (s.ceilingDb);
        float maxRed = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float a = amt.at (i, n), g = drive.at (i, n);
            for (int k = 0; k < 2; ++k)
            {
                const float x = ch[k][i];
                const float xd = x * g;
                const float y = shape (s.mode, xd, c);
                if (std::abs (xd) > 1.0e-6f) maxRed = std::max (maxRed, gainToDb (std::abs (xd)) - gainToDb (std::abs (y)));
                // dry path keeps its level (drive only feeds the clipper) → mix blends shaped vs. unshaped
                ch[k][i] = x + a * (y - x);
            }
        }
        clipDb = maxRed;
    }

    float clipDb = 0.0f;   // telemetry: largest reduction applied by the clipper in the last block

private:
    Ramp amt, drive;
};
} // namespace prl
