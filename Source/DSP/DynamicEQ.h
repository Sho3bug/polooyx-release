#pragma once
// POLOOYX RELEASE ENGINE — 4-band dynamic EQ (downward / cut-only).
// Each band listens to its own band-passed sidechain (stereo-linked, mid signal) and, only while
// that band exceeds the threshold, pulls a bell filter down by up to RANGE dB. Meant for
// intermittent problems (harsh peaks, low-mid buildup on certain notes, resonances).

#include "DspCommon.h"
#include "../Params.h"

namespace prl
{
class DynamicEQ
{
public:
    struct Band { float freq = 1000, q = 1.2f, thrDb = 0, ratio = 2, attMs = 5, relMs = 120, rangeDb = 3, mix = 1; };
    struct Settings { std::array<Band, kDeqBands> bands; float amount = 1; };

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept
    {
        for (auto& b : sc) b.reset();
        for (auto& ch : bell) for (auto& b : ch) b.reset();
        for (auto& e : env) e.reset();
        gr.fill (0.0f); amt.snap (0.0f);
        for (auto& g : grOut) g = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount);
        if (amt.from <= 0.0f && amt.to <= 0.0f) { if (active) reset(); active = false; return; }
        active = true;
        // sidechain pass: envelope per band over this block
        for (int b = 0; b < kDeqBands; ++b)
        {
            const auto& p = s.bands[(size_t) b];
            sc[(size_t) b].set (Svf::Mode::BandPass, p.freq, p.q, 0, sr);
            env[(size_t) b].set (p.attMs, p.relMs, sr);
        }
        std::array<float, kDeqBands> grMax {};
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            for (int b = 0; b < kDeqBands; ++b)
            {
                const auto& p = s.bands[(size_t) b];
                const float lvl = gainToDb (std::abs (sc[(size_t) b].process (m)) + 1.0e-9f);
                const float e = env[(size_t) b].process (lvl);
                const float g = std::min (p.rangeDb, gainReductionDb (e, p.thrDb, p.ratio, 3.0f)) * p.mix;
                grMax[(size_t) b] = std::max (grMax[(size_t) b], g);
                gr[(size_t) b] = g;
            }
        }
        // apply: bell cut at the block's current reduction (envelopes are already smooth)
        float* ch[2] = { L, R };
        for (int b = 0; b < kDeqBands; ++b)
        {
            const auto& p = s.bands[(size_t) b];
            const float cut = -gr[(size_t) b];
            grOut[(size_t) b] = grMax[(size_t) b];
            if (cut > -0.01f && lastCut[(size_t) b] > -0.01f) { for (auto& c : bell) c[(size_t) b].reset(); lastCut[(size_t) b] = 0; continue; }
            lastCut[(size_t) b] = cut;
            for (int c = 0; c < 2; ++c) bell[(size_t) c][(size_t) b].set (Svf::Mode::Bell, p.freq, p.q, cut, sr);
            for (int i = 0; i < n; ++i)
            {
                const float a = amt.at (i, n);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = ch[c][i];
                    ch[c][i] = x + a * (bell[(size_t) c][(size_t) b].process (x) - x);
                }
            }
        }
    }

    std::array<float, kDeqBands> grOut {};   // telemetry: max reduction per band in the last block (dB)

private:
    float sr = 48000;
    std::array<Svf, kDeqBands> sc;
    std::array<std::array<Svf, kDeqBands>, 2> bell;
    std::array<DbEnvelope, kDeqBands> env;
    std::array<float, kDeqBands> gr {}, lastCut {};
    Ramp amt;
    bool active = false;
};
} // namespace prl
