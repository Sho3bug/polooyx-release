#pragma once
// POLOOYX RELEASE ENGINE — 8-band mastering EQ + TONE tilt.
// Each band: BELL / LOW SHELF / HIGH SHELF / HIGH-PASS / LOW-PASS, frequency, gain, Q.
// TONE is a see-saw tilt around ~650 Hz (low shelf down while high shelf goes up, and vice versa):
// it never boosts both ends at once.

#include "DspCommon.h"
#include "../Params.h"

namespace prl
{
class MasterEQ
{
public:
    struct Band { EqType type = EqType::Bell; float freq = 1000, gainDb = 0, q = 1; };
    struct Settings { std::array<Band, kEqBands> bands; float tiltDb = 0; float amount = 1; };

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept
    {
        for (auto& ch : f) for (auto& b : ch) b.reset();
        for (auto& ch : tilt) for (auto& b : ch) b.reset();
        amt.snap (0.0f);
    }

    static bool bandActive (const Band& b) noexcept
    {
        return b.type == EqType::HighPass || b.type == EqType::LowPass || std::abs (b.gainDb) > 0.005f;
    }
    static Svf::Mode modeOf (EqType t) noexcept
    {
        switch (t)
        {
            case EqType::Bell: return Svf::Mode::Bell;
            case EqType::LowShelf: return Svf::Mode::LowShelf;
            case EqType::HighShelf: return Svf::Mode::HighShelf;
            case EqType::HighPass: return Svf::Mode::HighPass;
            case EqType::LowPass: return Svf::Mode::LowPass;
        }
        return Svf::Mode::Bell;
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount);
        if (amt.from <= 0.0f && amt.to <= 0.0f) { if (wasActive) reset(); wasActive = false; return; }
        wasActive = true;
        std::array<bool, kEqBands> on {};
        for (int b = 0; b < kEqBands; ++b)
        {
            on[(size_t) b] = bandActive (s.bands[(size_t) b]);
            for (int c = 0; c < 2; ++c)
            {
                if (on[(size_t) b]) f[(size_t) c][(size_t) b].set (modeOf (s.bands[(size_t) b].type), s.bands[(size_t) b].freq, s.bands[(size_t) b].q, s.bands[(size_t) b].gainDb, sr);
                else f[(size_t) c][(size_t) b].reset();
            }
        }
        const bool tiltOn = std::abs (s.tiltDb) > 0.005f;
        for (int c = 0; c < 2; ++c)
        {
            tilt[(size_t) c][0].set (Svf::Mode::LowShelf, 250.0f, 0.5f, -s.tiltDb, sr);
            tilt[(size_t) c][1].set (Svf::Mode::HighShelf, 1700.0f, 0.5f, s.tiltDb, sr);
        }
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float a = amt.at (i, n);
            for (int c = 0; c < 2; ++c)
            {
                const float x = ch[c][i];
                float y = x;
                for (int b = 0; b < kEqBands; ++b) if (on[(size_t) b]) y = f[(size_t) c][(size_t) b].process (y);
                if (tiltOn) y = tilt[(size_t) c][1].process (tilt[(size_t) c][0].process (y));
                ch[c][i] = x + a * (y - x);
            }
        }
        if (! tiltOn) for (auto& ch2 : tilt) for (auto& t : ch2) t.reset();
    }

    // magnitude response (dB) of a settings object at frequency hz — for the EQ curve display
    static float responseDb (const Settings& s, double hz, float fs)
    {
        std::complex<double> h (1.0, 0.0);
        for (const auto& b : s.bands)
            if (bandActive (b)) { Svf f; f.set (modeOf (b.type), b.freq, b.q, b.gainDb, fs); h *= f.response (hz); }
        if (std::abs (s.tiltDb) > 0.005f)
        {
            Svf a, b; a.set (Svf::Mode::LowShelf, 250.0f, 0.5f, -s.tiltDb, fs); b.set (Svf::Mode::HighShelf, 1700.0f, 0.5f, s.tiltDb, fs);
            h *= a.response (hz) * b.response (hz);
        }
        return (float) (20.0 * std::log10 (std::max (1e-9, std::abs (h))));
    }

private:
    float sr = 48000;
    std::array<std::array<Svf, kEqBands>, 2> f;
    std::array<std::array<Svf, 2>, 2> tilt;
    Ramp amt;
    bool wasActive = false;
};
} // namespace prl
