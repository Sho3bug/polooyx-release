#pragma once
// POLOOYX RELEASE ENGINE — the mastering chain.
//
//  INPUT GAIN → DC → SUBSONIC → TONAL EQ → DYNAMIC EQ → MULTIBAND → GLUE → PUNCH
//  → [oversampled] HARMONIC COLOR → STEREO / M-S → POLOOYX CHARACTER → [oversampled] SOFT CLIPPER
//  → TRUE-PEAK LIMITER → OUTPUT GAIN → DRY/WET (latency-compensated dry)
//
// The chain is used both live (inside the plugin) and offline (POLO IT / FINAL CHECK renders of the
// captured mix) — same code, same settings → the verification is of the real processing.
// Latency is constant: every oversampling factor is padded to the 8x latency, the limiter always
// delays by its maximum lookahead.

#include "ChainSettings.h"
#include <juce_dsp/juce_dsp.h>

namespace prl
{
class MasterChain
{
public:
    static constexpr int kMaxBlock = 32;

    void prepare (double sampleRate);
    void reset();
    int getLatency() const noexcept { return latency; }
    double getSampleRate() const noexcept { return sr; }

    // n ≤ kMaxBlock; L/R processed in place
    void process (float* L, float* R, int n, const ChainSettings& s) noexcept;

    struct Telemetry
    {
        float glueGr = 0, clipDb = 0, limGr = 0, punchDb = 0;
        std::array<float, kDeqBands> deqGr {};
        std::array<float, kMbBands> mbGr {};
        double dcOffset = 0;
        int osFactor = 1;
    };
    const Telemetry& telemetry() const noexcept { return tel; }

private:
    struct OsSection
    {
        std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> os;   // 2x, 4x, 8x
        std::array<int, 3> lat {};
        int maxLat = 0;
        std::array<DelayLine, 2> pad, bypass;
        int curFactor = 0;
        bool wasActive = false;
        void prepare (int maxBlock);
        void reset();
    };
    template <typename Fn> void runOversampled (OsSection& sec, float* L, float* R, int n, int factor, bool active, Fn&& fn) noexcept;

    double sr = 48000;
    int latency = 0;
    GainStage gainStage;
    MasterEQ eq;
    DynamicEQ deq;
    Multiband mb;
    GlueCompressor glue;
    Punch punch;
    Saturation sat;
    StereoEngine stereo;
    Character character;
    Clipper clipper;
    TruePeakLimiter limiter;
    OsSection osColor, osClip;
    int satFactor = 0;
    std::array<DelayLine, 2> dryDelay;
    Ramp outGain, dryWet;
    Telemetry tel;
};
} // namespace prl
