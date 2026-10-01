#pragma once
// POLOOYX RELEASE ENGINE — audio processor.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Presets.h"
#include "DSP/MasterChain.h"
#include "Analysis/LoudnessAnalyzer.h"
#include "Analysis/PeakAnalyzer.h"
#include "Analysis/StereoAnalyzer.h"
#include "Analysis/SpectrumAnalyzer.h"
#include "Analysis/MixDiagnostics.h"
#include "Intelligence/AutoMasterEngine.h"
#include <mutex>

class PolooyxReleaseProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    PolooyxReleaseProcessor();
    ~PolooyxReleaseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "POLOOYX RELEASE ENGINE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (prl::id::bypass); }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------------- parameters
    juce::UndoManager undo { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<prl::ParamTable> table;
    float param (const juce::String& pid) const { return apvts.getRawParameterValue (pid)->load(); }
    void setParam (const juce::String& pid, float realValue);   // message thread, with host notification

    // ---------------- presets
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    static juce::File userPresetDir();
    juce::Array<juce::File> userPresets() const;
    juce::String currentPresetName { "POLOOYX DEFAULT" };
    // settings A/B (two complete parameter snapshots)
    void switchAB (int slot);
    int abSlot = 0;

    // ---------------- capture / analysis / POLO IT (message thread API)
    enum class Action { None, Analyze, PoloIt, FinalCheck };
    void startListening (Action then);     // arm capture; `then` runs automatically when capture finishes
    void stopListeningAndRun();            // finish the capture now
    void runOnCapture (Action a);          // run an action on the existing capture
    void cancelJob();
    bool hasCapture() const noexcept { return captureLen.load() > (int) (sr * 3.0) && captureState.load() == kCapDone; }
    bool isListening() const noexcept { return captureState.load() == kCapArmed || captureState.load() == kCapRecording; }
    bool isRecording() const noexcept { return captureState.load() == kCapRecording; }
    float listenSeconds() const noexcept { return (float) captureLen.load() / (float) sr; }
    float maxCaptureSeconds() const noexcept { return (float) captureMax / (float) sr; }
    bool isBusy() const noexcept { return jobRunning.load(); }
    float jobProgress() const noexcept { return progress.load(); }
    Action currentJob() const noexcept { return runningAction; }
    Action getPendingAction() const noexcept { return pendingAction; }
    void applyPoloIt();                    // apply the last POLO IT result (one undo step)
    bool poloItPending() const noexcept { return poloPending; }

    struct Results
    {
        int version = 0;
        int lastKind = 0;            // 1 analyse, 2 POLO IT, 3 final check
        bool hasAnalysis = false, hasPolo = false, hasFinal = false;
        prl::AnalysisResult analysis;
        prl::Diagnosis diagnosis;
        prl::PoloItResult polo;
        prl::AnalysisResult finalAnalysis;
        prl::FinalCheck finalCheck;
        juce::String message;
    };
    Results getResults() const { std::lock_guard<std::mutex> l (resMutex); return results; }
    int resultsVersion() const noexcept { return resVersion.load(); }

    // ---------------- meters (audio thread → UI)
    struct Meters
    {
        std::atomic<float> inPeak { -200 }, inTp { -200 }, outPeak { -200 }, outTp { -200 }, outTpMax { -200 }, inTpMax { -200 };
        std::atomic<float> lufsM { -200 }, lufsS { -200 }, lufsI { -200 }, lra { 0 }, rms { -200 }, crest { 0 };
        std::atomic<float> inLufsS { -200 }, inLufsI { -200 };
        std::atomic<float> corr { 1 }, inCorr { 1 }, midDb { -200 }, sideDb { -200 }, width { 0 }, balance { 0 };
        std::atomic<float> glueGr { 0 }, limGr { 0 }, clipDb { 0 }, punchDb { 0 };
        std::array<std::atomic<float>, prl::kDeqBands> deqGr {};
        std::array<std::atomic<float>, prl::kMbBands> mbGr {};
        std::atomic<float> levelMatchDb { 0 }, cpu { 0 };
        std::atomic<int> osFactor { 2 }, clipCount { 0 };
    } meters;
    void resetPeaks();

    prl::SampleFifo specPre { 1 << 16 }, specPost { 1 << 16 };
    prl::SampleFifo scopeL { 1 << 14 }, scopeR { 1 << 14 };
    double getSr() const noexcept { return sr; }
    int getChainLatency() const noexcept { return chain.getLatency(); }

    // UI state stored with the session (not automatable)
    juce::ValueTree uiState { "UI" };

private:
    void handleAsyncUpdate() override;
    void processChunk (float* L, float* R, int n);
    void worker (Action a);
    void publish (std::function<void (Results&)> fn);

    static constexpr int kChunk = prl::MasterChain::kMaxBlock;
    double sr = 48000;
    prl::MasterChain chain;
    prl::Snapshot snapTarget, snapSmooth;
    std::array<std::array<float, kChunk>, 2> fifoIn {}, fifoOut {};
    int fifoPos = 0;
    std::array<prl::DelayLine, 2> origDelay;
    prl::LoudnessMeter loudPost, loudIn;
    prl::TruePeakMeter tpPost, tpIn;
    prl::StereoAnalyzer stPost, stIn;
    prl::Ramp listenRamp, bypassRamp, lmGain;
    std::array<prl::Svf, 4> smallSpk;
    double lmProc = 0, lmOrig = 0, lmCoef = 0.999;
    prl::KWeighting kwP[2], kwO[2];
    double rmsAcc = 0; int rmsN = 0;
    float tpHoldDisp = -200;
    int meterCounter = 0;
    std::atomic<bool> resetPeaksFlag { false };

    // capture
    static constexpr int kCapIdle = 0, kCapArmed = 1, kCapRecording = 2, kCapDone = 3;
    juce::AudioBuffer<float> capture;
    std::atomic<int> captureState { kCapIdle }, captureLen { 0 };
    int captureMax = 0;
    Action pendingAction = Action::None;
    std::atomic<bool> transportPlaying { false };
    int silentRun = 0;

    // background job
    std::unique_ptr<juce::Thread> jobThread;
    std::atomic<bool> jobRunning { false }, cancelFlag { false };
    std::atomic<float> progress { 0 };
    Action runningAction = Action::None;
    mutable std::mutex resMutex;
    Results results;
    std::atomic<int> resVersion { 0 };
    bool poloPending = false;

    int currentProgram = 0;
    std::array<juce::ValueTree, 2> abSlots;
    float cpuAvg = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolooyxReleaseProcessor)
};
