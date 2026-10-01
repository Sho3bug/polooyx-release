#pragma once
// POLOOYX RELEASE ENGINE — POLOOYX CHARACTER (subtle, master-safe).
//   AURA  – air: harmonics generated only from content above ~7 kHz, plus a gentle 12 kHz shelf
//   GRIT  – harmonic density from the low-mids (parallel, only the *added* harmonics are mixed in)
//   GLASS – smooths aggressive 3–5 kHz peaks (dynamic, ≤ 2 dB) and adds a little clean top sheen
//   DEPTH – very short, filtered side-only early reflections (front/back depth, mono-safe)
//   SPACE – a quiet side-only diffuse ambience (all-pass diffusion + damped delays, mono-safe)
// Everything at 0 → the stage is bypassed exactly. Maximum settings stay inside master territory.

#include "DspCommon.h"

namespace prl
{
class Character
{
public:
    struct Settings { float aura = 0, grit = 0, glass = 0, depth = 0, space = 0, amount = 1; };

    void prepare (double fs)
    {
        sr = (float) fs;
        for (auto& d : er) d.prepare ((int) (0.03f * sr) + 8);
        for (auto& d : amb) d.prepare ((int) (0.12f * sr) + 8);
        for (auto& a : ap) a.prepare ((int) (0.02f * sr) + 8);
        reset();
    }
    void reset() noexcept
    {
        for (auto& f : airHp) f.reset();
        for (auto& f : airShelf) f.reset();
        for (auto& f : gritBp) f.reset();
        glassBp.reset();
        for (auto& f : glassBell) f.reset();
        for (auto& f : glassShelf) f.reset();
        for (auto& f : erLp) f.reset();
        for (auto& f : ambLp) f.reset();
        for (auto& d : er) d.clear();
        for (auto& d : amb) d.clear();
        for (auto& a : ap) a.clear();
        glassEnv.reset();
        apState.fill (0.0f);
        aR.snap (0); gR.snap (0); glR.snap (0); dR.snap (0); sR.snap (0);
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        aR.set (s.aura * s.amount); gR.set (s.grit * s.amount); glR.set (s.glass * s.amount);
        dR.set (s.depth * s.amount); sR.set (s.space * s.amount);
        const bool any = aR.from + aR.to + gR.from + gR.to + glR.from + glR.to + dR.from + dR.to + sR.from + sR.to > 0.0f;
        if (! any) { if (active) reset(); active = false; return; }
        active = true;

        for (int c = 0; c < 2; ++c)
        {
            airHp[(size_t) c].set (Svf::Mode::HighPass, 7000.0f, 0.7f, 0, sr);
            airShelf[(size_t) c].set (Svf::Mode::HighShelf, 12000.0f, 0.6f, 1.5f * aR.to, sr);
            gritBp[(size_t) c].set (Svf::Mode::BandPass, 450.0f, 0.6f, 0, sr);
            glassShelf[(size_t) c].set (Svf::Mode::HighShelf, 10000.0f, 0.6f, 0.8f * glR.to, sr);
            erLp[(size_t) c].set (Svf::Mode::LowPass, 6000.0f, 0.7f, 0, sr);
            ambLp[(size_t) c].set (Svf::Mode::LowPass, 4500.0f, 0.6f, 0, sr);
        }
        glassBp.set (Svf::Mode::BandPass, 3800.0f, 1.0f, 0, sr);
        glassEnv.set (2.0f, 80.0f, sr);

        // glass: one dynamic cut per block from the 3–5 kHz sidechain
        float glassCut = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = gainToDb (std::abs (glassBp.process (0.5f * (L[i] + R[i]))) + 1e-9f);
            glassCut = std::min (2.0f, gainReductionDb (glassEnv.process (lvl), -30.0f, 2.5f, 6.0f)) * glR.to;
        }
        for (int c = 0; c < 2; ++c) glassBell[(size_t) c].set (Svf::Mode::Bell, 3800.0f, 1.0f, -glassCut, sr);

        static const float erT[4] = { 0.0071f, 0.0113f, 0.0167f, 0.0229f };
        static const float erG[4] = { 0.55f, -0.42f, 0.33f, -0.25f };
        static const float ambT[4] = { 0.0371f, 0.0533f, 0.0727f, 0.0951f };
        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float au = aR.at (i, n), gr = gR.at (i, n), gl = glR.at (i, n), de = dR.at (i, n), sp = sR.at (i, n);
            for (int c = 0; c < 2; ++c)
            {
                float x = ch[c][i];
                // AURA
                const float h = airHp[(size_t) c].process (x);
                x += au * 0.35f * (fastTanh (3.0f * h) / 3.0f - h * 0.85f);
                x = x + au * (airShelf[(size_t) c].process (x) - x);
                // GRIT
                const float b = gritBp[(size_t) c].process (x);
                x += gr * 0.6f * (fastTanh (4.0f * b) / 4.0f - b);
                // GLASS
                const float gx = glassShelf[(size_t) c].process (glassBell[(size_t) c].process (x));
                x = x + (gl > 0.0f ? 1.0f : 0.0f) * (gx - x);
                ch[c][i] = x;
            }
            // DEPTH + SPACE: side-only (mono-compatible: they cancel in L+R)
            const float mid = 0.5f * (ch[0][i] + ch[1][i]);
            float sideAdd = 0.0f;
            if (de > 0.0f || sp > 0.0f)
            {
                er[0].push (mid);
                float e = 0.0f;
                for (int k = 0; k < 4; ++k) e += erG[k] * er[0].readInt ((int) (erT[k] * sr));
                sideAdd += de * 0.10f * erLp[0].process (e);

                // two all-pass diffusers then damped taps
                float v = mid;
                for (int k = 0; k < 2; ++k)
                {
                    const int dl = (int) ((k == 0 ? 0.0047f : 0.0113f) * sr);
                    const float dly = ap[(size_t) k].readInt (dl);
                    const float y = -0.6f * v + dly;
                    ap[(size_t) k].push (v + 0.6f * y);
                    v = y;
                }
                amb[0].push (v);
                float a = 0.0f;
                for (int k = 0; k < 4; ++k) a += (k % 2 ? -0.5f : 0.5f) * amb[0].readInt ((int) (ambT[k] * sr));
                sideAdd += sp * 0.08f * ambLp[0].process (a);
            }
            ch[0][i] += sideAdd;
            ch[1][i] -= sideAdd;
        }
    }

private:
    float sr = 48000;
    std::array<Svf, 2> airHp, airShelf, gritBp, glassBell, glassShelf, erLp, ambLp;
    Svf glassBp;
    DbEnvelope glassEnv;
    std::array<DelayLine, 2> er, amb, ap;
    std::array<float, 2> apState {};
    Ramp aR, gR, glR, dR, sR;
    bool active = false;
};
} // namespace prl
