#pragma once
// POLOOYX RELEASE ENGINE — real-time stereo measurements (correlation, mid/side, width, balance).
// Exponentially-weighted (~300 ms) so the readings settle like a hardware correlation meter.

#include "../DSP/DspCommon.h"

namespace prl
{
class StereoAnalyzer
{
public:
    void prepare (double fs, float timeS = 0.3f) { c = std::exp (-1.0 / (timeS * fs)); reset(); }
    void reset() noexcept { ll = rr = lr = mm = ss = 0.0; }
    inline void process (float l, float r) noexcept
    {
        const double k = 1.0 - c;
        ll += k * ((double) l * l - ll); rr += k * ((double) r * r - rr); lr += k * ((double) l * r - lr);
        const double m = 0.5 * (l + r), s = 0.5 * (l - r);
        mm += k * (m * m - mm); ss += k * (s * s - ss);
    }
    float correlation() const noexcept { const double d = std::sqrt (ll * rr); return d > 1.0e-12 ? (float) (lr / d) : 1.0f; }
    float midDb() const noexcept   { return (float) powToDb (mm); }
    float sideDb() const noexcept  { return (float) powToDb (ss); }
    float widthPct() const noexcept { return mm > 1.0e-12 ? (float) (100.0 * std::sqrt (ss / mm)) : 0.0f; }
    float balanceDb() const noexcept { return (float) (powToDb (ll) - powToDb (rr)); }
    bool hasSignal() const noexcept { return ll + rr > 1.0e-9; }
private:
    double c = 0.999, ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
};
} // namespace prl
