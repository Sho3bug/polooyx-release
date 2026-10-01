#pragma once
// POLOOYX RELEASE ENGINE — offline mix analysis + rule-based diagnosis.
//
// analyzeMix() measures a captured stereo buffer (loudness, peaks, spectrum, low end, stereo,
// dynamics, clipping). diagnose() turns those *measurements* into wording. Nothing here is AI and
// nothing is invented: every statement is tied to a number and a documented threshold
// (see docs/DSP_NOTES.md). Wording uses POSSIBLE / LIKELY / CHECK / DETECTED, because measurements
// do not prove subjective quality.

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace prl
{
constexpr int kAnBands = 9;
inline const char* anBandName (int b)
{
    static const char* n[] = { "20-60 Hz", "60-120 Hz", "120-250 Hz", "250-500 Hz", "500 Hz-1 kHz", "1-2 kHz", "2-5 kHz", "5-10 kHz", "10-20 kHz" };
    return n[b];
}
inline float anBandLo (int b) { static const float f[] = { 20, 60, 120, 250, 500, 1000, 2000, 5000, 10000 }; return f[b]; }
inline float anBandHi (int b) { static const float f[] = { 60, 120, 250, 500, 1000, 2000, 5000, 10000, 20000 }; return f[b]; }

struct AnalysisResult
{
    bool valid = false;
    double seconds = 0, sampleRate = 0;
    // loudness / level
    double lufsI = -200, lufsSMax = -200, lufsMMax = -200, lra = 0;
    double rmsDb = -200, peakDb = -200, truePeakDb = -200, crestDb = 0, plr = 0;
    // spectrum
    std::array<double, kAnBands> bandDb {};      // energy per octave, dB (relative scale)
    std::array<double, kAnBands> bandDev {};     // deviation from the mix's own tilt line (dB)
    std::array<double, kAnBands> bandShare {};   // share of total energy, %
    double tiltDbPerOct = 0, centroidHz = 0;
    double subRelDb = 0, bassRelDb = 0;
    double infraPct = 0;
    double lowMidPeakHz = 250, midPeakHz = 700, presencePeakHz = 3500;
    double promLowMid = 0, promMid = 0, promPresence = 0;   // 1-octave prominence above the local tonal line (dB)
    std::vector<float> thirdHz, thirdRes;         // 1/3-octave residuals vs the local tonal line (LAB display)
    std::vector<float> avgSpectrumDb;             // 1/6-octave smoothed long-term spectrum (for display)
    std::vector<float> avgSpectrumHz;
    // stereo
    double correlation = 1, widthPct = 0, midDb = -200, sideDb = -200, balanceDb = 0, lowSideDb = -200, lowCorrelation = 1;
    // peaks / clipping / dc
    int clipRuns = 0; int64_t nearFullScale = 0; bool intersampleOver = false; double dcOffset = 0;
    // dynamics
    double transientCrestDb = 0, densityPct = 0, stP10 = -200, stP95 = -200;
    std::vector<float> shortTermHistory;          // short-term LUFS every 100 ms (for display)
};

AnalysisResult analyzeMix (const juce::AudioBuffer<float>& buf, int numSamples, double sampleRate);

struct DiagItem
{
    juce::String category, state, detail;
    int level = 0;           // 0 = fine, 1 = check, 2 = likely problem
};

struct Diagnosis
{
    std::vector<DiagItem> items;
    juce::StringArray nextSteps;
    juce::String vocalRegion, masking;
};

Diagnosis diagnose (const AnalysisResult& a, float targetLufs, float ceilingDbTp);

// FINAL CHECK — diagnostic states of a processed render (not a score)
struct FinalCheck
{
    juce::StringArray states;   // e.g. "READY FOR EXPORT" or one/more problem states
    juce::StringArray details;
    bool ready = false;
};
FinalCheck finalCheck (const AnalysisResult& processed, float targetLufs, float ceilingDbTp, bool monitorIsFull, bool bypassed);
} // namespace prl
