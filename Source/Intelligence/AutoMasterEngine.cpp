#include "AutoMasterEngine.h"
#include "../DSP/MasterChain.h"

namespace prl
{
static void setSnap (Snapshot& s, const ParamTable& t, const juce::String& pid, float v)
{
    const int i = t.ids.indexOf (pid);
    if (i >= 0) s[(size_t) i] = v;
}
static float getSnap (const Snapshot& s, const ParamTable& t, const juce::String& pid)
{
    const int i = t.ids.indexOf (pid);
    return i >= 0 ? s[(size_t) i] : 0.0f;
}

AnalysisResult AutoMasterEngine::renderAndAnalyse (const juce::AudioBuffer<float>& in, int n, double sr, const Snapshot& snap,
                                                   const ParamTable& table, float correlation, const std::atomic<bool>& cancel,
                                                   juce::AudioBuffer<float>* keep)
{
    auto chain = std::make_unique<MasterChain>();
    chain->prepare (sr);
    const auto cs = resolve (snap, table.ix, sr, correlation);
    const int lat = chain->getLatency();
    juce::AudioBuffer<float> out (2, n);
    out.clear();
    const float* inL = in.getReadPointer (0);
    const float* inR = in.getReadPointer (std::min (1, in.getNumChannels() - 1));
    float bl[MasterChain::kMaxBlock], br[MasterChain::kMaxBlock];
    const int total = n + lat;
    for (int pos = 0; pos < total; pos += MasterChain::kMaxBlock)
    {
        if ((pos & 0xFFFF) == 0 && cancel.load()) return {};
        const int m = std::min (MasterChain::kMaxBlock, total - pos);
        for (int i = 0; i < m; ++i)
        {
            const int s = pos + i;
            bl[i] = s < n ? inL[s] : 0.0f; br[i] = s < n ? inR[s] : 0.0f;
        }
        chain->process (bl, br, m, cs);
        for (int i = 0; i < m; ++i)
        {
            const int o = pos + i - lat;
            if (o >= 0 && o < n) { out.setSample (0, o, bl[i]); out.setSample (1, o, br[i]); }
        }
    }
    auto a = analyzeMix (out, n, sr);
    if (keep != nullptr) *keep = std::move (out);
    return a;
}

float AutoMasterEngine::presenceP90 (const juce::AudioBuffer<float>& in, int n, double sr, float inGainDb)
{
    Svf bp; bp.set (Svf::Mode::BandPass, 3500.0f, 1.2f, 0, (float) sr);
    DbEnvelope env; env.set (5.0f, 120.0f, (float) sr);
    const float g = dbToGain (inGainDb);
    const float* L = in.getReadPointer (0);
    const float* R = in.getReadPointer (std::min (1, in.getNumChannels() - 1));
    std::vector<float> vals;
    const int step = std::max (1, (int) (0.01 * sr));
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]) * g;
        const float e = env.process (gainToDb (std::abs (bp.process (m)) + 1.0e-9f));
        if (i % step == 0 && e > -70.0f) vals.push_back (e);
    }
    if (vals.size() < 20) return -200.0f;
    std::sort (vals.begin(), vals.end());
    return vals[(size_t) (0.9 * (vals.size() - 1))];
}

PoloItResult AutoMasterEngine::poloIt (const Job& job, std::atomic<float>& progress, const std::atomic<bool>& cancel)
{
    PoloItResult r;
    const auto& T = *job.table;
    const auto& in = *job.audio;
    const int n = job.numSamples;
    const double sr = job.sampleRate;
    progress = 0.02f;

    // 1. BEFORE
    r.before = analyzeMix (in, n, sr);
    if (! r.before.valid) { r.error = "Not enough signal captured - play at least a few seconds of the mix."; return r; }
    const float ceiling = getSnap (job.snapshot, T, id::limCeiling);
    r.diagnosisBefore = diagnose (r.before, job.targetLufs, ceiling);
    progress = 0.10f;
    if (cancel) return r;

    // 2. recommendations
    RecommendationInputs ri;
    ri.a = &r.before; ri.mode = job.mode;
    ri.inputGainDb = getSnap (job.snapshot, T, id::inGain);
    ri.presenceP90Db = presenceP90 (in, n, sr, ri.inputGainDb);
    ri.current = [&] (const juce::String& pid) { return getSnap (job.snapshot, T, pid); };
    auto cs = recommend (ri);

    // 3. safety
    r.notes.addArray (SafetyEngine::validate (cs));

    Snapshot s = job.snapshot;
    for (auto& c : cs) setSnap (s, T, c.paramId, c.value);
    const auto lim = limitsFor (job.mode);
    const float corr = (float) r.before.correlation;

    // 4. loudness — clipper first (only when the material has sharp peaks and the mode allows it)
    setSnap (s, T, id::limThresh, 0.0f);
    float clip = 0.0f;
    progress = 0.18f;
    auto base = renderAndAnalyse (in, n, sr, s, T, corr, cancel);
    if (cancel || ! base.valid) { r.error = cancel ? "Cancelled." : "Render failed."; return r; }
    const float target = job.targetLufs;
    const float need = target - (float) base.lufsI;

    float G = 0.0f;
    if (job.targetKind == Target::Premaster)
        r.notes.add ("MIX / PREMASTER target: POLO IT does not push loudness; the limiter only guards the ceiling.");
    else if (need < 0.5f)
        r.notes.add ("The mix is already at or above the target (" + juce::String (base.lufsI, 1) + " LUFS) - POLO IT does not turn it down; streaming platforms normalise loud masters.");
    else
    {
        if (lim.clipMaxDb > 0.0f && need > 4.0f && r.before.plr > 11.0)
        {
            clip = std::min (lim.clipMaxDb, 0.25f * (float) (r.before.plr - 9.0));
            setSnap (s, T, id::clipDrive, clip);
            setSnap (s, T, id::clipOn, 1.0f);
        }
        if (job.mode == PoloMode::Loud) setSnap (s, T, id::limStyle, (float) LimStyle::Modern);
        G = std::min (HardLimits::limDriveMaxDb, need);
        AnalysisResult cur;
        for (int it = 0; it < 6; ++it)
        {
            setSnap (s, T, id::limThresh, -G);
            cur = renderAndAnalyse (in, n, sr, s, T, corr, cancel);
            progress = 0.25f + 0.08f * (float) it;
            if (cancel || ! cur.valid) { r.error = "Cancelled."; return r; }
            const float err = target - (float) cur.lufsI;
            if (std::abs (err) < 0.25f) break;
            G = juce::jlimit (0.0f, HardLimits::limDriveMaxDb, G + err * 1.15f);
            if (G >= HardLimits::limDriveMaxDb && err > 0) { r.notes.add ("Limiter drive capped at +12 dB."); break; }
        }
        // respect the mode's peak-to-loudness floor (no over-limiting)
        if (cur.plr < lim.plrFloor)
        {
            float lo = 0.0f, hi = G;
            for (int k = 0; k < 5; ++k)
            {
                const float mid = 0.5f * (lo + hi);
                setSnap (s, T, id::limThresh, -mid);
                auto t2 = renderAndAnalyse (in, n, sr, s, T, corr, cancel);
                if (cancel) { r.error = "Cancelled."; return r; }
                if (t2.plr >= lim.plrFloor) lo = mid; else hi = mid;
                progress = 0.75f + 0.03f * (float) k;
            }
            r.notes.add ("Target not fully reached: stopped at the point where peak-to-loudness ratio stays >= " + juce::String (lim.plrFloor, 1)
                         + " dB (" + poloModeNames()[(int) job.mode] + " mode). Pushing further would over-limit this mix.");
            G = lo;
        }
        setSnap (s, T, id::limThresh, -G);
    }
    r.limDriveDb = G; r.clipDb = clip;
    progress = 0.9f;

    // 5. AFTER + final check
    r.after = renderAndAnalyse (in, n, sr, s, T, corr, cancel);
    if (cancel) { r.error = "Cancelled."; return r; }
    if (r.after.valid && r.after.truePeakDb > ceiling + 0.3)
        r.notes.add ("Verification: true peak " + juce::String (r.after.truePeakDb, 2) + " dBTP is above the ceiling - check the limiter mix / ceiling.");
    r.check = finalCheck (r.after, target, ceiling, job.monitorFull, false);

    // collect every parameter that differs from the starting snapshot
    for (int i = 0; i < T.size(); ++i)
        if (std::abs (s[(size_t) i] - job.snapshot[(size_t) i]) > 1.0e-4f)
        {
            Correction c; c.paramId = T.ids[i]; c.before = job.snapshot[(size_t) i]; c.value = s[(size_t) i];
            for (auto& rc : cs) if (rc.paramId == c.paramId) { c.group = rc.group; c.reason = rc.reason; }
            if (c.paramId == id::limThresh || c.paramId == id::limStyle) { c.group = "LIMITER"; c.reason = "loudness toward " + juce::String (target, 1) + " LUFS"; }
            if (c.paramId == id::clipDrive || c.paramId == id::clipOn) { c.group = "CLIP"; c.reason = "peak shaping before the limiter"; }
            r.changes.push_back (c);
        }
    r.nothingToDo = r.changes.empty();
    r.ok = true;
    progress = 1.0f;
    return r;
}
} // namespace prl
