#pragma once
// POLOOYX RELEASE ENGINE — POLO IT / FINAL CHECK engine (runs on a background thread).
//
// POLO IT:
//   1. analyse the captured input mix (BEFORE)
//   2. recommendations → corrections (RecommendationEngine)
//   3. safety validation (SafetyEngine)
//   4. loudness: render the captured mix through the real chain with the corrections and solve the
//      limiter drive iteratively until the integrated loudness meets the target — never below the
//      mode's peak-to-loudness floor, never above +12 dB drive
//   5. render the final settings, analyse (AFTER), run FINAL CHECK, report exactly what changed
// Nothing is applied to the plugin here; the processor applies the result on the message thread as
// one undoable step.

#include "CorrectionModel.h"
#include "SafetyEngine.h"
#include "RecommendationEngine.h"
#include "../DSP/ChainSettings.h"
#include <atomic>

namespace prl
{
struct PoloItResult
{
    bool ok = false;
    juce::String error;
    AnalysisResult before, after;
    Diagnosis diagnosisBefore;
    std::vector<Correction> changes;
    juce::StringArray notes;
    FinalCheck check;
    float limDriveDb = 0, clipDb = 0;
    bool nothingToDo = false;
};

class AutoMasterEngine
{
public:
    struct Job
    {
        const juce::AudioBuffer<float>* audio = nullptr;
        int numSamples = 0;
        double sampleRate = 48000;
        Snapshot snapshot;                 // current parameter values
        const ParamTable* table = nullptr;
        PoloMode mode = PoloMode::Balanced;
        float targetLufs = -14;
        Target targetKind = Target::Streaming;
        bool monitorFull = true;
    };

    // full POLO IT
    static PoloItResult poloIt (const Job& job, std::atomic<float>& progress, const std::atomic<bool>& cancel);

    // render the captured audio through `snapshot` and analyse it (FINAL CHECK / AFTER)
    static AnalysisResult renderAndAnalyse (const juce::AudioBuffer<float>& in, int n, double sr, const Snapshot& snap,
                                            const ParamTable& table, float correlation, const std::atomic<bool>& cancel,
                                            juce::AudioBuffer<float>* keepOutput = nullptr);

    // 90th-percentile level of the presence sidechain (same filter as the dynamic EQ band)
    static float presenceP90 (const juce::AudioBuffer<float>& in, int n, double sr, float inputGainDb);
};
} // namespace prl
