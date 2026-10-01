#pragma once
// POLOOYX RELEASE ENGINE — shared DSP primitives. Everything is allocation-free after prepare().

#include <cmath>
#include <complex>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>

namespace prl
{
constexpr float kPi = 3.14159265358979f;
constexpr double kPiD = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-10f)); }
inline double powToDb (double p) noexcept { return 10.0 * std::log10 (std::max (p, 1.0e-20)); }
inline float clamp01 (float x) noexcept   { return std::min (1.0f, std::max (0.0f, x)); }
inline float clampf (float x, float lo, float hi) noexcept { return std::min (hi, std::max (lo, x)); }
inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }

// ============================================================ TPT state-variable filter (Simper)
// Stable under fast modulation, exact bilinear response — used for every EQ / crossover in the engine.
class Svf
{
public:
    enum class Mode { LowPass, HighPass, BandPass, Bell, LowShelf, HighShelf, AllPass };

    void set (Mode mode, float freq, float q, float gainDb, float sr) noexcept
    {
        freq = clampf (freq, 5.0f, 0.49f * sr);
        q = std::max (0.05f, q);
        const float w = std::tan (kPi * freq / sr);
        float A = 1.0f, kk = 1.0f / q, gg = w;
        switch (mode)
        {
            case Mode::LowPass:   m0 = 0; m1 = 0;  m2 = 1; break;
            case Mode::HighPass:  m0 = 1; m1 = -kk; m2 = -1; break;
            case Mode::BandPass:  m0 = 0; m1 = kk; m2 = 0; break;      // unity peak gain
            case Mode::AllPass:   m0 = 1; m1 = -2.0f * kk; m2 = 0; break;
            case Mode::Bell:      A = std::pow (10.0f, gainDb / 40.0f); kk = 1.0f / (q * A); m0 = 1; m1 = kk * (A * A - 1.0f); m2 = 0; break;
            case Mode::LowShelf:  A = std::pow (10.0f, gainDb / 40.0f); gg = w / std::sqrt (A); m0 = 1; m1 = kk * (A - 1.0f); m2 = A * A - 1.0f; break;
            case Mode::HighShelf: A = std::pow (10.0f, gainDb / 40.0f); gg = w * std::sqrt (A); m0 = A * A; m1 = kk * (1.0f - A) * A; m2 = 1.0f - A * A; break;
        }
        g = gg; k = kk;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        fs = sr; wc = w;
    }

    inline float process (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    // exact magnitude of the digital filter at frequency f (for drawing EQ curves)
    std::complex<double> response (double f) const noexcept
    {
        const double wf = std::tan (kPiD * std::min (f, 0.4999 * fs) / fs) / (double) g;
        const std::complex<double> s (0.0, wf);
        const auto D = s * s + (double) k * s + 1.0;
        return ((double) m0 * D + (double) m1 * s + (double) m2) / D;
    }

private:
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0, ic1 = 0, ic2 = 0, fs = 48000, wc = 0;
};

// 4th-order Linkwitz-Riley low-pass (two cascaded Butterworth SVF stages)
class LR4LowPass
{
public:
    void set (float f, float sr) noexcept { a.set (Svf::Mode::LowPass, f, 0.70710678f, 0, sr); b.set (Svf::Mode::LowPass, f, 0.70710678f, 0, sr); }
    inline float process (float x) noexcept { return b.process (a.process (x)); }
    void reset() noexcept { a.reset(); b.reset(); }
private:
    Svf a, b;
};

// 4th-order Linkwitz-Riley high-pass; LR4LowPass + LR4HighPass = 2nd-order all-pass (flat magnitude)
class LR4HighPass
{
public:
    void set (float f, float sr) noexcept { a.set (Svf::Mode::HighPass, f, 0.70710678f, 0, sr); b.set (Svf::Mode::HighPass, f, 0.70710678f, 0, sr); }
    inline float process (float x) noexcept { return b.process (a.process (x)); }
    void reset() noexcept { a.reset(); b.reset(); }
private:
    Svf a, b;
};

// the all-pass an LR4 crossover pair sums to — used to phase-align bands that skip a crossover
class LR4AllPass
{
public:
    void set (float f, float sr) noexcept { a.set (Svf::Mode::AllPass, f, 0.70710678f, 0, sr); }
    inline float process (float x) noexcept { return a.process (x); }
    void reset() noexcept { a.reset(); }
private:
    Svf a;
};

// ============================================================ delay line (integer + fractional read)
class DelayLine
{
public:
    void prepare (int maxDelay) { int n = 1; while (n < maxDelay + 4) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float readInt (int d) const noexcept { return buf[(size_t) ((w - 1 - d) & mask)]; }   // d = 0 → newest
    inline float read (float d) const noexcept
    {
        const int i = (int) d; const float f = d - (float) i;
        return readInt (i) + f * (readInt (i + 1) - readInt (i));
    }
private:
    std::vector<float> buf { 0.0f };
    int mask = 0, w = 0;
};

// ============================================================ smoothing
class Smoother
{
public:
    void reset (float v) noexcept { cur = v; }
    // exponential smoothing per block of n samples with time constant tau (s)
    float next (float target, int n, float sr, float tau = 0.03f) noexcept
    {
        const float a = std::exp (-(float) n / (tau * sr));
        cur = target + a * (cur - target);
        if (std::abs (cur - target) < 1.0e-6f * (1.0f + std::abs (target))) cur = target;
        return cur;
    }
    float value() const noexcept { return cur; }
private:
    float cur = 0.0f;
};

// linear per-sample ramp across one processing block
struct Ramp
{
    float from = 0, to = 0;
    void set (float target) noexcept { from = to; to = target; }
    void snap (float v) noexcept { from = to = v; }
    inline float at (int i, int n) const noexcept { return from + (to - from) * ((float) (i + 1) / (float) n); }
};

// envelope follower in the dB domain (attack / release ballistics)
class DbEnvelope
{
public:
    void set (float attackMs, float releaseMs, float sr) noexcept
    {
        aC = std::exp (-1.0f / (std::max (0.01f, attackMs) * 0.001f * sr));
        rC = std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * sr));
    }
    inline float process (float inDb) noexcept
    {
        env = inDb > env ? inDb + aC * (env - inDb) : inDb + rC * (env - inDb);
        return env;
    }
    void reset (float v = -120.0f) noexcept { env = v; }
    float value() const noexcept { return env; }
private:
    float aC = 0, rC = 0, env = -120.0f;
};

// linear-domain peak/RMS follower
class LinEnvelope
{
public:
    void set (float attackMs, float releaseMs, float sr) noexcept
    {
        aC = std::exp (-1.0f / (std::max (0.01f, attackMs) * 0.001f * sr));
        rC = std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * sr));
    }
    inline float process (float x) noexcept { env = x > env ? x + aC * (env - x) : x + rC * (env - x); return env; }
    void reset() noexcept { env = 0.0f; }
private:
    float aC = 0, rC = 0, env = 0;
};

// soft-knee downward gain computer: returns gain reduction in dB (>= 0)
inline float gainReductionDb (float levelDb, float threshDb, float ratio, float kneeDb) noexcept
{
    const float slope = 1.0f - 1.0f / std::max (1.0f, ratio);
    const float over = levelDb - threshDb;
    if (kneeDb > 0.0f)
    {
        if (2.0f * over < -kneeDb) return 0.0f;
        if (2.0f * std::abs (over) <= kneeDb) { const float t = over + 0.5f * kneeDb; return slope * t * t / (2.0f * kneeDb); }
    }
    return over > 0.0f ? slope * over : 0.0f;
}

inline float fastTanh (float x) noexcept
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

// deterministic random (no globals → reproducible renders)
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    inline float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; }
};
} // namespace prl
