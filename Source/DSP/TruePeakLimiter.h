#pragma once
// POLOOYX RELEASE ENGINE — true-peak lookahead limiter.
//
// Gain computer: every input sample's *true-peak* estimate (polyphase interpolation, BS.1770 style)
// becomes the gain that would put it exactly at the ceiling. A sliding-window minimum over the
// lookahead window guarantees the gain is already down when the peak arrives; a box-car average
// turns the attack into a smooth ramp (no clicks). Release depends on STYLE:
//   TRANSPARENT – program-dependent: release slows down when limiting is sustained
//   PUNCH       – attack ramp half as long + quicker release (keeps more of the transient shape)
//   DENSE       – fast release + 0.5 dB pre-knee (denser / louder)
//   MODERN      – adaptive: fast release on isolated peaks, slow on dense passages
// Latency is constant (max lookahead + interpolator delay) whatever LOOKAHEAD is set to, so changing
// it never shifts audio in the DAW. A final sample-level clamp at the ceiling is the last safety net.
//
// Timing (u = input index): the requirement computed at u refers to audio sample s = u - tpLat.
// With window `la` and box-car `ramp ≤ la`, the gain at index u0 + la - 1 (u0 = s + tpLat) is
// guaranteed ≤ that requirement, so sample s is multiplied by the gain from index s + tpLat + la - 1.

#include "DspCommon.h"
#include "../Params.h"
#include "../Analysis/PeakAnalyzer.h"

namespace prl
{
class TruePeakLimiter
{
public:
    struct Settings { LimStyle style = LimStyle::Transparent; float threshDb = 0, ceilingDb = -1, releaseMs = 120, lookaheadMs = 5, mix = 1, amount = 1; };

    static constexpr float kMaxLookaheadMs = 10.0f;

    void prepare (double fs, int tpFactor)
    {
        sr = (float) fs;
        for (auto& d : tp) d.prepare (tpFactor);
        maxLa = std::max (4, (int) std::ceil (kMaxLookaheadMs * 0.001f * sr));
        tpLat = tp[0].latency();
        D = maxLa + tpLat;
        int n = 1; while (n < D * 2 + 16) n <<= 1;
        mask = n - 1;
        for (auto& b : delay) b.assign ((size_t) n, 0.0f);
        for (auto& b : delayIn) b.assign ((size_t) n, 0.0f);
        req.assign ((size_t) n, 1.0f);
        dq.assign ((size_t) n, 0);
        box.assign ((size_t) n, 1.0f);
        gainHist.assign ((size_t) n, 1.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& d : tp) d.reset();
        for (auto& b : delay) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : delayIn) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (req.begin(), req.end(), 1.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        std::fill (gainHist.begin(), gainHist.end(), 1.0f);
        head = tail = 0; t = 0; env = 1.0f; boxLen = 1; boxSum = 1.0; curLa = -1; pkPrev = 0.0f;
        amt.snap (0.0f); drive.snap (1.0f); mixR.snap (1.0f);
        grDb = 0.0f; sustained = 0.0f;
    }

    int getLatency() const noexcept { return D; }

    void process (float* L, float* R, int n, const Settings& s) noexcept
    {
        amt.set (s.amount); drive.set (dbToGain (-s.threshDb)); mixR.set (s.mix);
        const float ceil = dbToGain (s.ceilingDb);
        const int la = std::max (2, std::min (maxLa, (int) std::lround (s.lookaheadMs * 0.001f * sr)));
        int ramp = la;
        float relMs = s.releaseMs, preKnee = 1.0f;
        switch (s.style)
        {
            case LimStyle::Transparent: break;
            case LimStyle::Punch:  ramp = std::max (1, la / 2); relMs *= 0.6f; break;
            case LimStyle::Dense:  relMs *= 0.35f; preKnee = dbToGain (-0.5f); break;
            case LimStyle::Modern: break;
        }
        if (la != curLa || ramp != boxLen) resetWindow (la, ramp);
        const float rC  = std::exp (-1.0f / (std::max (1.0f, relMs) * 0.001f * sr));
        const float rCs = std::exp (-1.0f / (std::max (1.0f, relMs * 4.0f) * 0.001f * sr));
        const float susC = std::exp (-1.0f / (0.3f * sr));
        const int gainOffset = D - tpLat - la + 1;   // ≥ 1
        const float thr = ceil * preKnee;
        float minG = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            const float a = amt.at (i, n), g = 1.0f + a * (drive.at (i, n) - 1.0f), m = mixR.at (i, n);
            const float xl = L[i] * g, xr = R[i] * g;
            const float pkNow = std::max (tp[0].process (xl), tp[1].process (xr));
            const float pk = std::max (pkNow, pkPrev);            // covers both neighbours of an inter-sample peak
            pkPrev = pkNow;
            const float want = pk > thr ? thr / pk : 1.0f;

            // sliding minimum over the last `la` requirements (monotonic deque)
            const int idx = (int) (t & mask);
            req[(size_t) idx] = want;
            while (head != tail && req[(size_t) (dq[(size_t) ((tail - 1) & mask)] & mask)] >= want) tail = (tail - 1) & mask;
            dq[(size_t) tail] = t; tail = (tail + 1) & mask;
            while (dq[(size_t) head] <= t - la) head = (head + 1) & mask;
            const float wmin = req[(size_t) (dq[(size_t) head] & mask)];

            // release ballistics
            if (wmin < env) env = wmin;
            else
            {
                float c = rC;
                if (s.style == LimStyle::Transparent) c = rC + (rCs - rC) * sustained;
                else if (s.style == LimStyle::Modern) c = sustained > 0.6f ? rCs : rC;
                env = wmin + c * (env - wmin);
            }
            const float limiting = env < 0.97f ? 1.0f : 0.0f;
            sustained = limiting + susC * (sustained - limiting);

            // box-car attack ramp (never above the current envelope)
            const size_t bi = (size_t) (t % boxLen);
            boxSum += (double) env - (double) box[bi];
            box[bi] = env;
            const float gg = std::min (env, (float) (boxSum / (double) boxLen));
            gainHist[(size_t) idx] = gg;
            const float gApplied = gainHist[(size_t) ((t - gainOffset) & mask)];
            minG = std::min (minG, gApplied);

            // audio path
            delay[0][(size_t) idx] = xl; delay[1][(size_t) idx] = xr;
            delayIn[0][(size_t) idx] = L[i]; delayIn[1][(size_t) idx] = R[i];
            const int rd = (int) ((t - D) & mask);
            const float yl = delay[0][(size_t) rd], yr = delay[1][(size_t) rd];
            const float limL = std::max (-ceil, std::min (ceil, yl * gApplied));
            const float limR = std::max (-ceil, std::min (ceil, yr * gApplied));
            const float wl = yl + m * (limL - yl), wr = yr + m * (limR - yr);      // parallel mix (dry = driven, unlimited)
            const float dl = delayIn[0][(size_t) rd], dr = delayIn[1][(size_t) rd];
            L[i] = dl + a * (wl - dl);                                               // module off → delayed original
            R[i] = dr + a * (wr - dr);
            ++t;
        }
        grDb = -gainToDb (minG) * std::max (amt.from, amt.to);
    }

    float grDb = 0.0f;

private:
    void resetWindow (int la, int ramp) noexcept
    {
        curLa = la; boxLen = std::max (1, ramp);
        std::fill (box.begin(), box.begin() + boxLen, env);
        boxSum = (double) env * boxLen;
        // rebuild the deque from the stored requirements so no pending peak is forgotten
        head = tail = 0;
        for (int64_t u = std::max<int64_t> (0, t - la); u < t; ++u)
        {
            const float w = req[(size_t) (u & mask)];
            while (head != tail && req[(size_t) (dq[(size_t) ((tail - 1) & mask)] & mask)] >= w) tail = (tail - 1) & mask;
            dq[(size_t) tail] = u; tail = (tail + 1) & mask;
        }
    }

    float sr = 48000;
    std::array<TruePeakDetector, 2> tp;
    int maxLa = 480, tpLat = 6, D = 486, mask = 0, head = 0, tail = 0, curLa = -1, boxLen = 1;
    int64_t t = 0;
    std::array<std::vector<float>, 2> delay, delayIn;
    std::vector<float> req, box, gainHist;
    std::vector<int64_t> dq;
    float env = 1, sustained = 0, pkPrev = 0;
    double boxSum = 1;
    Ramp amt, drive, mixR;
};
} // namespace prl
