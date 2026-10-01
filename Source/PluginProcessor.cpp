#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace prl;

namespace
{
class FnThread : public juce::Thread
{
public:
    FnThread (std::function<void()> f) : juce::Thread ("POLOOYX worker"), fn (std::move (f)) {}
    void run() override { fn(); }
private:
    std::function<void()> fn;
};

}

PolooyxReleaseProcessor::PolooyxReleaseProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PRL", createLayout())
{
    table = std::make_unique<ParamTable> (apvts);
    table->read (snapTarget);
    snapSmooth = snapTarget;
    uiState.setProperty ("view", "BEGINNER", nullptr);
    uiState.setProperty ("specMode", 2, nullptr);       // 0 PRE, 1 POST, 2 BOTH
    uiState.setProperty ("specRange", 90, nullptr);
    uiState.setProperty ("specSmooth", 1, nullptr);     // 0 low, 1 medium, 2 high
    uiState.setProperty ("peakHold", false, nullptr);
    uiState.setProperty ("tpFactor", 4, nullptr);
    abSlots[0] = apvts.copyState();
}

void PolooyxReleaseProcessor::handleAsyncUpdate()
{
    // message thread: start the action that was waiting for the capture, or apply POLO IT
    if (! jobRunning.load() && captureState.load() == kCapDone && pendingAction != Action::None)
    {
        const auto a = pendingAction;
        pendingAction = Action::None;
        runOnCapture (a);
        return;
    }
    if (! jobRunning.load() && poloPending) applyPoloIt();
}

PolooyxReleaseProcessor::~PolooyxReleaseProcessor()
{
    cancelFlag = true;
    if (jobThread) jobThread->stopThread (20000);
    cancelPendingUpdate();
}

bool PolooyxReleaseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono()) return false;
    return ! (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono());
}

void PolooyxReleaseProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    chain.prepare (sr);
    setLatencySamples (chain.getLatency() + kChunk);
    for (auto& d : origDelay) d.prepare (chain.getLatency() + kChunk + 8);
    loudPost.prepare (sr); loudIn.prepare (sr);
    const int tpf = (int) uiState.getProperty ("tpFactor", 4);
    tpPost.prepare (tpf); tpIn.prepare (tpf);
    stPost.prepare (sr); stIn.prepare (sr);
    smallSpk[0].set (Svf::Mode::HighPass, 180.0f, 0.7f, 0, (float) sr);
    smallSpk[1].set (Svf::Mode::HighPass, 180.0f, 0.7f, 0, (float) sr);
    smallSpk[2].set (Svf::Mode::LowPass, 7000.0f, 0.7f, 0, (float) sr);
    smallSpk[3].set (Svf::Mode::Bell, 1200.0f, 0.9f, 3.0f, (float) sr);
    for (auto& f : smallSpk) f.reset();
    for (int c = 0; c < 2; ++c) { kwP[c].prepare (sr); kwO[c].prepare (sr); }
    lmCoef = std::exp (-1.0 / (3.0 * sr));
    lmProc = lmOrig = 0.0;
    fifoPos = 0;
    for (auto& f : fifoIn) f.fill (0.0f);
    for (auto& f : fifoOut) f.fill (0.0f);
    table->read (snapTarget); snapSmooth = snapTarget;
    listenRamp.snap (param (id::listen) > 0.5f ? 1.0f : 0.0f);
    bypassRamp.snap (param (id::bypass) > 0.5f ? 1.0f : 0.0f);
    // capture buffer (allocated here, never on the audio thread)
    if (! jobRunning.load())
    {
        captureMax = (int) (sr * (sr > 100000.0 ? 30.0 : 60.0));
        capture.setSize (2, captureMax, false, true, true);
        captureLen = 0; captureState = kCapIdle;
    }
}

void PolooyxReleaseProcessor::resetPeaks() { resetPeaksFlag = true; }

void PolooyxReleaseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    bool playing = wrapperType == wrapperType_Standalone;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) playing = playing || pos->getIsPlaying();
    transportPlaying = playing;

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        fifoIn[0][(size_t) fifoPos] = inL ? inL[i] : 0.0f;
        fifoIn[1][(size_t) fifoPos] = inR ? inR[i] : 0.0f;
        const float ol = fifoOut[0][(size_t) fifoPos], orr = fifoOut[1][(size_t) fifoPos];
        if (outR != nullptr) { outL[i] = ol; outR[i] = orr; }
        else outL[i] = 0.5f * (ol + orr);
        if (++fifoPos == kChunk)
        {
            fifoPos = 0;
            fifoOut = fifoIn;
            processChunk (fifoOut[0].data(), fifoOut[1].data(), kChunk);
        }
    }
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (secs / (n / sr));
    cpuAvg = 0.95f * cpuAvg + 0.05f * load;
    meters.cpu.store (cpuAvg, std::memory_order_relaxed);
}

void PolooyxReleaseProcessor::processChunk (float* L, float* R, int n)
{
    if (resetPeaksFlag.exchange (false))
    {
        tpPost.reset(); tpIn.reset(); loudPost.resetIntegrated(); loudIn.resetIntegrated(); tpHoldDisp = -200;
        meters.outTpMax = -200.0f; meters.inTpMax = -200.0f; meters.clipCount = 0;
    }
    table->read (snapTarget);
    table->smooth (snapSmooth, snapTarget, std::exp (-(float) n / (0.03f * (float) sr)));
    const auto& ix = table->ix;
    const bool monoIn = getTotalNumInputChannels() == 1;

    // ---------------- input: metering, capture, aligned original
    float origL[kChunk], origR[kChunk], mid[kChunk];
    const int st = captureState.load (std::memory_order_acquire);
    int len = captureLen.load (std::memory_order_relaxed);
    const bool play = transportPlaying.load();
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];
        tpIn.process (l, r);
        loudIn.process (l, r, monoIn);
        stIn.process (l, r);
        if (st == kCapArmed || st == kCapRecording)
        {
            if (play && (st == kCapRecording || std::abs (l) + std::abs (r) > 2.0e-4f))
            {
                if (len < captureMax) { capture.setSample (0, len, l); capture.setSample (1, len, r); ++len; }
            }
        }
        origDelay[0].push (l); origDelay[1].push (r);
        origL[i] = origDelay[0].readInt (chain.getLatency());
        origR[i] = origDelay[1].readInt (chain.getLatency());
        mid[i] = 0.5f * (origL[i] + origR[i]);
    }
    if (st == kCapArmed && len > 0) captureState.store (kCapRecording, std::memory_order_release);
    if (st == kCapArmed || st == kCapRecording)
    {
        captureLen.store (len, std::memory_order_release);
        if (len >= captureMax || (st == kCapRecording && ! play && len > (int) (sr * 10.0)))
        {
            captureState.store (kCapDone, std::memory_order_release);
            triggerAsyncUpdate();
        }
    }
    specPre.push (mid, n);

    // ---------------- the chain
    const auto cs = resolve (snapSmooth, ix, sr, stIn.hasSignal() ? stIn.correlation() : 1.0f);
    chain.process (L, R, n, cs);

    // ---------------- output metering (processed signal, before listen/monitor)
    float pmid[kChunk];
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];
        loudPost.process (l, r);
        tpPost.process (l, r);
        stPost.process (l, r);
        rmsAcc += 0.5 * ((double) l * l + (double) r * r); ++rmsN;
        pmid[i] = 0.5f * (l + r);
        // level match: slow K-weighted power of processed vs. original
        const double pl = kwP[0].process (l), pr = kwP[1].process (r);
        const double ol = kwO[0].process (origL[i]), orr = kwO[1].process (origR[i]);
        lmProc = pl * pl + pr * pr + lmCoef * (lmProc - (pl * pl + pr * pr));
        lmOrig = ol * ol + orr * orr + lmCoef * (lmOrig - (ol * ol + orr * orr));
    }
    specPost.push (pmid, n);
    {
        float sl[kChunk / 2], sr2[kChunk / 2];
        for (int i = 0; i < n / 2; ++i) { sl[i] = L[2 * i]; sr2[i] = R[2 * i]; }
        scopeL.push (sl, n / 2); scopeR.push (sr2, n / 2);
    }

    // ---------------- listen (ORIGINAL is level-matched) / bypass / monitor
    const bool listenOrig = snapTarget[(size_t) ix.listen] > 0.5f;
    const bool byp = snapTarget[(size_t) ix.bypass] > 0.5f;
    const bool lmOn = snapTarget[(size_t) ix.levelMatch] > 0.5f;
    float lmDb = 0.0f;
    if (lmProc > 1.0e-9 && lmOrig > 1.0e-9) lmDb = clampf ((float) powToDb (lmProc / lmOrig), -24.0f, 24.0f);
    meters.levelMatchDb.store (lmDb, std::memory_order_relaxed);
    listenRamp.set (listenOrig ? 1.0f : 0.0f);
    bypassRamp.set (byp ? 1.0f : 0.0f);
    lmGain.set (lmOn ? dbToGain (lmDb) : 1.0f);
    const int mon = (int) std::lround (snapTarget[(size_t) ix.monitor]);
    for (int i = 0; i < n; ++i)
    {
        const float lr = listenRamp.at (i, n), br = bypassRamp.at (i, n), g = lmGain.at (i, n);
        float l = L[i] + lr * (origL[i] * g - L[i]);
        float r = R[i] + lr * (origR[i] * g - R[i]);
        l = l + br * (origL[i] - l);
        r = r + br * (origR[i] - r);
        if (mon == (int) Monitor::Mono) { const float m = 0.5f * (l + r); l = r = m; }
        else if (mon == (int) Monitor::SmallSpeaker)
        {
            float m = 0.5f * (l + r);
            for (auto& f : smallSpk) m = f.process (m);
            l = r = m;
        }
        L[i] = l; R[i] = r;
    }

    // ---------------- telemetry
    const auto& t = chain.telemetry();
    meters.glueGr.store (t.glueGr, std::memory_order_relaxed);
    meters.limGr.store (t.limGr, std::memory_order_relaxed);
    meters.clipDb.store (t.clipDb, std::memory_order_relaxed);
    meters.punchDb.store (t.punchDb, std::memory_order_relaxed);
    for (int b = 0; b < kDeqBands; ++b) meters.deqGr[(size_t) b].store (t.deqGr[(size_t) b], std::memory_order_relaxed);
    for (int b = 0; b < kMbBands; ++b) meters.mbGr[(size_t) b].store (t.mbGr[(size_t) b], std::memory_order_relaxed);
    meters.osFactor.store (t.osFactor, std::memory_order_relaxed);

    if (++meterCounter * n >= (int) (sr * 0.05))   // every ~50 ms
    {
        meterCounter = 0;
        const float tp = gainToDb (tpPost.takeBlockTp()), sp = gainToDb (tpPost.takeBlockSp());
        meters.outTp.store (tp); meters.outPeak.store (sp);
        meters.outTpMax.store (gainToDb (tpPost.maxTruePeak()));
        meters.inTp.store (gainToDb (tpIn.takeBlockTp())); meters.inPeak.store (gainToDb (tpIn.takeBlockSp()));
        meters.inTpMax.store (gainToDb (tpIn.maxTruePeak()));
        if (sp > -0.05f) meters.clipCount.fetch_add (1);
        meters.lufsM.store ((float) loudPost.getMomentary()); meters.lufsS.store ((float) loudPost.getShortTerm());
        meters.lufsI.store ((float) loudPost.getIntegrated()); meters.lra.store ((float) loudPost.getLoudnessRange());
        meters.inLufsS.store ((float) loudIn.getShortTerm()); meters.inLufsI.store ((float) loudIn.getIntegrated());
        const float rms = (float) powToDb (rmsAcc / std::max (1, rmsN)); rmsAcc = 0; rmsN = 0;
        meters.rms.store (rms); meters.crest.store (sp > -150 && rms > -150 ? sp - rms : 0.0f);
        meters.corr.store (stPost.correlation()); meters.inCorr.store (stIn.correlation());
        meters.midDb.store (stPost.midDb()); meters.sideDb.store (stPost.sideDb());
        meters.width.store (stPost.widthPct()); meters.balance.store (stPost.balanceDb());
    }
}

// ================================================================== parameters / presets
void PolooyxReleaseProcessor::setParam (const juce::String& pid, float v)
{
    if (auto* p = apvts.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

int PolooyxReleaseProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String PolooyxReleaseProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) factoryPresets().size()) ? juce::String (factoryPresets()[(size_t) i].name) : juce::String();
}
void PolooyxReleaseProcessor::setCurrentProgram (int i) { if (juce::isPositiveAndBelow (i, getNumPrograms())) loadFactoryPreset (i); }

void PolooyxReleaseProcessor::loadFactoryPreset (int index)
{
    const auto& p = factoryPresets()[(size_t) index];
    undo.beginNewTransaction ("Preset " + juce::String (p.name));
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            if (! isSessionParam (rp->getParameterID()))
            {
                rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture();
            }
    for (auto& [pid, v] : p.values) setParam (pid, v);
    currentProgram = index;
    currentPresetName = p.name;
}

juce::File PolooyxReleaseProcessor::userPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("POLOOYX").getChildFile ("Release Presets");
}

juce::Array<juce::File> PolooyxReleaseProcessor::userPresets() const
{
    auto files = userPresetDir().findChildFiles (juce::File::findFiles, false, "*.prelease");
    files.sort();
    return files;
}

bool PolooyxReleaseProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto dir = userPresetDir();
    if (! dir.createDirectory()) return false;
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", name);
    xml->setAttribute ("plugin", "POLOOYX RELEASE ENGINE");
    const bool ok = xml->writeTo (dir.getChildFile (name + ".prelease"));
    if (ok) currentPresetName = name;
    return ok;
}

bool PolooyxReleaseProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    undo.beginNewTransaction ("Preset " + f.getFileNameWithoutExtension());
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto pid = rp->getParameterID();
            auto child = tree.getChildWithProperty ("id", pid);
            if (child.isValid()) setParam (pid, (float) child.getProperty ("value"));
        }
    currentPresetName = f.getFileNameWithoutExtension();
    return true;
}

void PolooyxReleaseProcessor::switchAB (int slot)
{
    if (slot == abSlot) return;
    abSlots[(size_t) abSlot] = apvts.copyState();
    abSlot = slot;
    if (! abSlots[(size_t) slot].isValid()) { abSlots[(size_t) slot] = apvts.copyState(); return; }
    undo.beginNewTransaction ("A/B");
    const auto& tree = abSlots[(size_t) slot];
    for (auto* ap : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto pid = rp->getParameterID();
            if (isSessionParam (pid)) continue;
            auto child = tree.getChildWithProperty ("id", pid);
            if (child.isValid()) setParam (pid, (float) child.getProperty ("value"));
        }
}

// ================================================================== state
void PolooyxReleaseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("POLOOYX_RELEASE");
    root.setProperty ("version", "1.0.0", nullptr);
    root.setProperty ("preset", currentPresetName, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    std::unique_ptr<juce::XmlElement> xml (root.createXml());
    copyXmlToBinary (*xml, dest);
}

void PolooyxReleaseProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName ("POLOOYX_RELEASE")) return;
    auto root = juce::ValueTree::fromXml (*xml);
    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
    {
        apvts.replaceState (params);
        // set every parameter explicitly so hosts/validators see the restored values immediately
        for (auto* ap : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            {
                auto child = params.getChildWithProperty ("id", rp->getParameterID());
                if (child.isValid()) rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
            }
    }
    auto ui = root.getChildWithName ("UI");
    if (ui.isValid()) uiState.copyPropertiesFrom (ui, nullptr);
    currentPresetName = root.getProperty ("preset", "POLOOYX DEFAULT").toString();
}

// ================================================================== capture / jobs
void PolooyxReleaseProcessor::startListening (Action then)
{
    if (jobRunning.load() || captureMax <= 0) return;
    pendingAction = then;
    captureLen.store (0);
    captureState.store (kCapArmed, std::memory_order_release);
}

void PolooyxReleaseProcessor::stopListeningAndRun()
{
    const int st = captureState.load();
    if (st == kCapArmed || st == kCapRecording)
    {
        captureState.store (kCapDone, std::memory_order_release);
        runOnCapture (pendingAction == Action::None ? Action::Analyze : pendingAction);
        pendingAction = Action::None;
    }
}

void PolooyxReleaseProcessor::cancelJob() { cancelFlag = true; }

void PolooyxReleaseProcessor::runOnCapture (Action a)
{
    if (jobRunning.load()) return;
    if (captureState.load() != kCapDone || captureLen.load() < (int) (sr * 3.0))
    {
        publish ([] (Results& r) { r.message = "Not enough audio captured yet - play at least 3 seconds of the mix."; });
        captureState.store (kCapIdle);
        return;
    }
    if (jobThread) jobThread->stopThread (1000);
    cancelFlag = false; progress = 0.0f; runningAction = a; jobRunning = true;
    jobThread = std::make_unique<FnThread> ([this, a] { worker (a); jobRunning = false; triggerAsyncUpdate(); });
    jobThread->startThread();
}

void PolooyxReleaseProcessor::publish (std::function<void (Results&)> fn)
{
    std::lock_guard<std::mutex> l (resMutex);
    fn (results);
    results.version = ++resVersion;
}

void PolooyxReleaseProcessor::worker (Action a)
{
    const int n = captureLen.load();
    Snapshot snap; table->read (snap);
    const auto& ix = table->ix;
    const Target tk = (Target) (int) std::lround (snap[(size_t) ix.target]);
    const float target = targetLufs (tk, snap[(size_t) ix.customTarget]);
    const float ceiling = snap[(size_t) ix.limCeiling];

    if (a == Action::Analyze)
    {
        progress = 0.3f;
        auto an = analyzeMix (capture, n, sr);
        auto dg = diagnose (an, target, ceiling);
        progress = 1.0f;
        publish ([&] (Results& r) { r.analysis = std::move (an); r.diagnosis = std::move (dg); r.hasAnalysis = true; r.message = {}; r.lastKind = 1; });
    }
    else if (a == Action::PoloIt)
    {
        AutoMasterEngine::Job job;
        job.audio = &capture; job.numSamples = n; job.sampleRate = sr; job.snapshot = snap; job.table = table.get();
        job.mode = (PoloMode) (int) std::lround (snap[(size_t) ix.poloMode]);
        job.targetLufs = target; job.targetKind = tk;
        job.monitorFull = std::lround (snap[(size_t) ix.monitor]) == 0;
        auto res = AutoMasterEngine::poloIt (job, progress, cancelFlag);
        const bool ok = res.ok;
        publish ([&] (Results& r)
        {
            if (ok)
            {
                r.analysis = res.before; r.diagnosis = res.diagnosisBefore; r.hasAnalysis = true;
                r.finalAnalysis = res.after; r.finalCheck = res.check; r.hasFinal = true;
            }
            r.message = ok ? juce::String() : res.error;
            r.polo = std::move (res); r.hasPolo = ok; r.lastKind = 2;
        });
        poloPending = ok;
    }
    else if (a == Action::FinalCheck)
    {
        std::atomic<bool>& c = cancelFlag;
        auto pre = analyzeMix (capture, n, sr);
        progress = 0.2f;
        auto after = AutoMasterEngine::renderAndAnalyse (capture, n, sr, snap, *table, (float) pre.correlation, c);
        progress = 1.0f;
        const bool bypassed = snap[(size_t) ix.bypass] > 0.5f || snap[(size_t) ix.listen] > 0.5f;
        auto fc = finalCheck (after, target, ceiling, std::lround (snap[(size_t) ix.monitor]) == 0, bypassed);
        publish ([&] (Results& r) { r.finalAnalysis = std::move (after); r.finalCheck = std::move (fc); r.hasFinal = true; r.message = {}; r.lastKind = 3; });
    }
}

void PolooyxReleaseProcessor::applyPoloIt()
{
    if (! poloPending) return;
    poloPending = false;
    const auto res = getResults();
    if (! res.hasPolo) return;
    undo.beginNewTransaction ("POLO IT");
    for (const auto& c : res.polo.changes) setParam (c.paramId, c.value);
    undo.beginNewTransaction();
}

juce::AudioProcessorEditor* PolooyxReleaseProcessor::createEditor() { return new PolooyxReleaseEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PolooyxReleaseProcessor(); }
