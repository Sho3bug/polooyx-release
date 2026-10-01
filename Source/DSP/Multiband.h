#pragma once
// POLOOYX RELEASE ENGINE — 5-band dynamics.
//
// Band split: a Linkwitz-Riley (LR4) crossover tree with all-pass compensation, so the five bands
// always sum to a flat magnitude response (an all-pass), and turning one band down never makes a
// neighbouring band louder. Because any crossover rotates phase, the module is *bit-transparent when
// idle*: until a band has a threshold below 0 dB, makeup, or AUTO GAIN, the dry signal passes untouched
// (the filters keep running in the background so engaging is seamless).
// Each band: stereo-linked downward compressor (threshold, ratio, attack, release, makeup, mix).
// AUTO GAIN (off by default) slowly restores each band's average gain reduction.

#include "DspCommon.h"
#include "../Params.h"

namespace prl
{
class Multiband
{
public:
    struct Band { float thrDb = 0, ratio = 2, attMs = 20, relMs = 150, makeupDb = 0, mix = 1; };
    struct Settings { std::array<float, kMbBands - 1> xo { { 90, 300, 2500, 8000 } }; std::array<Band, kMbBands> bands; bool autoGain = false; float amount = 1; };

    static bool engaged (const Settings& s) noexcept
    {
        if (s.autoGain) return true;
        for (auto& b : s.bands) if (b.thrDb < -0.05f || std::abs (b.makeupDb) > 0.05f) return true;
        return false;
    }

    void prepare (double fs) { sr = (float) fs; reset(); }
    void reset() noexcept
    {
        for (auto& ch : xs) for (auto& x : ch) { x.lp.reset(); x.hp.reset(); }
        for (auto& ch : ap) for (auto& a : ch) a.reset();
        for (auto& e : env) e.reset();
        autoAvg.fill (0.0f); amt.snap (0.0f); grOut.fill (0.0f); lastGr.fill (0.0f);
    }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kMbBands - 1; ++i)
            {
                xs[(size_t) c][(size_t) i].lp.set (s.xo[(size_t) i], sr);
                xs[(size_t) c][(size_t) i].hp.set (s.xo[(size_t) i], sr);
            }
        // compensation all-passes: band 0 needs AP(x2,x3,x4), band 1 AP(x3,x4), band 2 AP(x4)
        for (int c = 0; c < 2; ++c)
        {
            int k = 0;
            for (int b = 0; b < kMbBands - 2; ++b)
                for (int x = b + 1; x < kMbBands - 1; ++x) ap[(size_t) c][(size_t) k++].set (s.xo[(size_t) x], sr);
        }
        for (int b = 0; b < kMbBands; ++b) env[(size_t) b].set (s.bands[(size_t) b].attMs, s.bands[(size_t) b].relMs, sr);
        const float autoC = std::exp (-(float) n / (2.0f * sr));
        const bool idle = amt.from <= 0.0f && amt.to <= 0.0f;
        float* ch[2] = { L, R };
        std::array<float, kMbBands> grMax {};

        for (int i = 0; i < n; ++i)
        {
            float band[kMbBands][2];
            for (int c = 0; c < 2; ++c)
            {
                float r = ch[c][i];
                for (int b = 0; b < kMbBands - 1; ++b)
                {
                    auto& x = xs[(size_t) c][(size_t) b];
                    band[b][c] = x.lp.process (r);
                    r = x.hp.process (r);
                }
                band[kMbBands - 1][c] = r;
                int k = 0;
                for (int b = 0; b < kMbBands - 2; ++b)
                    for (int x = b + 1; x < kMbBands - 1; ++x) band[b][c] = ap[(size_t) c][(size_t) k++].process (band[b][c]);
            }
            if (idle) continue;   // filters stay current; audio untouched
            const float a = amt.at (i, n);
            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < kMbBands; ++b)
            {
                const auto& p = s.bands[(size_t) b];
                const float pk = std::max (std::abs (band[b][0]), std::abs (band[b][1]));
                const float e = env[(size_t) b].process (gainToDb (pk + 1.0e-9f));
                const float grDb = gainReductionDb (e, p.thrDb, p.ratio, 4.0f);
                grMax[(size_t) b] = std::max (grMax[(size_t) b], grDb * p.mix);
                const float makeup = p.makeupDb + (s.autoGain ? autoAvg[(size_t) b] : 0.0f);
                const float g = 1.0f + p.mix * (dbToGain (makeup - grDb) - 1.0f);
                outL += band[b][0] * g; outR += band[b][1] * g;
                lastGr[(size_t) b] = grDb * p.mix;
            }
            ch[0][i] += a * (outL - ch[0][i]);
            ch[1][i] += a * (outR - ch[1][i]);
        }
        for (int b = 0; b < kMbBands; ++b)
        {
            grOut[(size_t) b] = grMax[(size_t) b];
            autoAvg[(size_t) b] = std::min (6.0f, lastGr[(size_t) b] + autoC * (autoAvg[(size_t) b] - lastGr[(size_t) b]));
        }
    }

    std::array<float, kMbBands> grOut {};

private:
    struct Xover { LR4LowPass lp; LR4HighPass hp; };
    float sr = 48000;
    std::array<std::array<Xover, kMbBands - 1>, 2> xs;
    std::array<std::array<LR4AllPass, 6>, 2> ap;     // 3 + 2 + 1 compensation stages
    std::array<DbEnvelope, kMbBands> env;
    std::array<float, kMbBands> autoAvg {}, lastGr {};
    Ramp amt;
};
} // namespace prl
