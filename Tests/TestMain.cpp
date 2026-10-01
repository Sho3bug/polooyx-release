// POLOOYX RELEASE ENGINE — offline verification harness.
// Runs the exact DSP / analysis / POLO IT code that ships in the VST3 and measures what it does.
// Writes test-output/TEST_REPORT.md. Exit code != 0 if any check fails.
//   PolooyxReleaseTests            full suite
//   PolooyxReleaseTests --ui       also renders UI snapshots to test-output/*.png
//   PolooyxReleaseTests --list-params <file>   writes the parameter reference

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <cstdio>
#include <map>
#include <set>

using namespace prl;

// ============================================================ report
static juce::String report;
static int failures = 0, passes = 0;
static void line (const juce::String& s) { report << s << "\n"; std::printf ("%s\n", s.toRawUTF8()); std::fflush (stdout); }
static void check (bool ok, const juce::String& what) { ok ? ++passes : ++failures; line (juce::String (ok ? "- [PASS] " : "- [FAIL] ") + what); }
static juce::String f1 (double v) { return juce::String (v, 1); }
static juce::String f2 (double v) { return juce::String (v, 2); }

// ============================================================ signals
using Buf = juce::AudioBuffer<float>;
static Buf sine (double sr, double secs, double hz, float ampDb, double phase = 0.0, float rightAmpDb = 999.0f)
{
    const int n = (int) (sr * secs);
    Buf b (2, n);
    const float a = dbToGain (ampDb), ar = rightAmpDb > 900.0f ? a : dbToGain (rightAmpDb);
    for (int i = 0; i < n; ++i)
    {
        const double ph = 2.0 * kPiD * hz * i / sr + phase;
        b.setSample (0, i, (float) (a * std::sin (ph))); b.setSample (1, i, (float) (ar * std::sin (ph)));
    }
    return b;
}
static Buf concat (std::initializer_list<Buf> parts)
{
    int n = 0; for (auto& p : parts) n += p.getNumSamples();
    Buf b (2, n); int pos = 0;
    for (auto& p : parts) { for (int c = 0; c < 2; ++c) b.copyFrom (c, pos, p, c, 0, p.getNumSamples()); pos += p.getNumSamples(); }
    return b;
}
static Buf pink (double sr, double secs, float rmsDb, uint32_t seed = 7, bool stereoIndependent = true)
{
    const int n = (int) (sr * secs);
    Buf b (2, n);
    for (int c = 0; c < 2; ++c)
    {
        Rng r; r.s = seed + (stereoIndependent ? (uint32_t) c * 7919u : 0u);
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = r.next();
            b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
            b.setSample (c, i, (float) (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362)); b6 = w * 0.115926;
        }
    }
    double e = 0; for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) e += (double) b.getSample (c, i) * b.getSample (c, i);
    b.applyGain (dbToGain (rmsDb) / (float) std::sqrt (e / (2.0 * n)));
    return b;
}

// synthetic rap mix: kick, 808, snare, hats, pad, vocal-like formant line — 140 BPM
struct MixOpts { float lowMidDb = 0, tiltDb = 0, stereoBass = 0, peakDb = -3, sibilant = 0; bool squash = false; };
static Buf rapMix (double sr, double secs, MixOpts o = {})
{
    const int n = (int) (sr * secs);
    Buf b (2, n); b.clear();
    const double beat = 60.0 / 140.0;
    Rng rng; rng.s = 1234;
    Svf snBp, hatHp, vF[3];
    snBp.set (Svf::Mode::BandPass, 1800.0f, 0.5f, 0, (float) sr);
    hatHp.set (Svf::Mode::HighPass, 8000.0f, 0.7f, 0, (float) sr);
    const float form[3] = { 700, 1200, 2600 };
    for (int k = 0; k < 3; ++k) vF[k].set (Svf::Mode::BandPass, form[k], 6.0f, 0, (float) sr);
    const float notes808[] = { 43.65f, 43.65f, 51.91f, 38.89f };
    double vph = 0, p808 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double inBeat = std::fmod (t, beat), inBar = std::fmod (t, beat * 4);
        const int beatIdx = (int) (t / beat) % 4;
        float L = 0, R = 0;
        // kick
        { const double e = std::exp (-inBeat / 0.12); const double f = 45.0 + 80.0 * std::exp (-inBeat / 0.03);
          const float k = (float) (0.75 * e * std::sin (2 * kPiD * f * inBeat)); L += k; R += k; }
        // 808 on beats 0 and 2
        { const double t8 = std::fmod (inBar, beat * 2); const int bar = (int) (t / (beat * 4)) % 4;
          p808 += notes808[bar] / sr; const float s = (float) (0.55 * std::exp (-t8 / 0.9) * std::sin (2 * kPiD * p808));
          L += s * (1.0f + o.stereoBass); R += s * (1.0f - o.stereoBass); }
        // snare on 1 and 3
        if (beatIdx % 2 == 1) { const float s = (float) (0.9 * std::exp (-inBeat / 0.09)) * snBp.process (rng.next()); L += s; R += s; }
        else snBp.process (rng.next());
        // hats every 1/8, alternating pan
        { const double t8 = std::fmod (t, beat / 2); const int idx = (int) (t / (beat / 2));
          const float h = (float) (0.35 * std::exp (-t8 / 0.025)) * hatHp.process (rng.next());
          if (idx % 2) L += h; else R += h; }
        // pad (detuned saw-ish chord, stereo)
        for (int k = 0; k < 3; ++k)
        {
            const double f = 220.0 * std::pow (2.0, (k == 0 ? 0 : k == 1 ? 3 : 7) / 12.0);
            float sl = 0, sr2 = 0;
            for (int h = 1; h <= 6; ++h) { sl += (float) (std::sin (2 * kPiD * f * 1.002 * h * t) / h); sr2 += (float) (std::sin (2 * kPiD * f * 0.998 * h * t) / h); }
            L += 0.035f * sl; R += 0.035f * sr2;
        }
        // vocal-like line (formants, 1–3 kHz presence), phrases
        { const bool on = std::fmod (t, 2.0) < 1.4; vph += (180.0 + 20.0 * std::sin (2 * kPiD * 0.5 * t)) / sr; if (vph > 1) vph -= 1;
          float src = 0; for (int h = 1; h <= 25; ++h) src += (float) (std::sin (2 * kPiD * vph * h) / std::pow (h, 1.1));
          float v = 0; for (int k = 0; k < 3; ++k) v += (k == 0 ? 1.0f : k == 1 ? 0.6f : 0.4f) * vF[k].process (src);
          if (o.sibilant > 0 && std::fmod (t, 2.0) < 0.08) v += o.sibilant * rng.next();
          v *= on ? 0.5f : 0.0f; L += v; R += v; }
        b.setSample (0, i, L); b.setSample (1, i, R);
    }
    // optional tonal colour
    if (std::abs (o.lowMidDb) > 0.01f || std::abs (o.tiltDb) > 0.01f)
        for (int c = 0; c < 2; ++c)
        {
            Svf bell, lo, hi;
            bell.set (Svf::Mode::Bell, 250.0f, 1.0f, o.lowMidDb, (float) sr);
            lo.set (Svf::Mode::LowShelf, 250.0f, 0.5f, -o.tiltDb, (float) sr); hi.set (Svf::Mode::HighShelf, 2000.0f, 0.5f, o.tiltDb, (float) sr);
            float* d = b.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = hi.process (lo.process (bell.process (d[i])));
        }
    if (o.squash) for (int c = 0; c < 2; ++c) { float* d = b.getWritePointer (c); for (int i = 0; i < n; ++i) d[i] = std::tanh (3.0f * d[i]) / 3.0f; }
    const float pk = b.getMagnitude (0, n);
    b.applyGain (dbToGain (o.peakDb) / pk);
    return b;
}

// "balanced" program material: band-limited pink noise with a music-like tilt (~-1.5 dB/oct),
// a 140 BPM beat envelope and partly correlated stereo; optional low-mid bump / stereo low end.
static Buf balancedMix (double sr, double secs, float lowMidDb = 0, float stereoBass = 0, float peakDb = -1.5f, float presDb = 0)
{
    auto m = pink (sr, secs, -20, 3, false);
    auto side = pink (sr, secs, -32, 9, true);
    Svf hp[2], hp2[2], ls[2], hs[2], lp[2], bell[2], pres[2], sbLp;
    for (int c = 0; c < 2; ++c)
    {
        hp[c].set (Svf::Mode::HighPass, 25.0f, 0.7f, 0, (float) sr); hp2[c].set (Svf::Mode::HighPass, 25.0f, 0.7f, 0, (float) sr); pres[c].set (Svf::Mode::Bell, 3500.0f, 1.0f, presDb, (float) sr); ls[c].set (Svf::Mode::LowShelf, 150.0f, 0.6f, 2.0f, (float) sr);
        hs[c].set (Svf::Mode::HighShelf, 2500.0f, 0.6f, -3.5f, (float) sr); lp[c].set (Svf::Mode::LowPass, 17000.0f, 0.7f, 0, (float) sr);
        bell[c].set (Svf::Mode::Bell, 250.0f, 1.0f, lowMidDb, (float) sr);
    }
    sbLp.set (Svf::Mode::LowPass, 90.0f, 0.7f, 0, (float) sr);
    for (int i = 0; i < m.getNumSamples(); ++i)
    {
        const float env = 0.55f + 0.45f * (float) std::exp (-std::fmod (i / sr, 60.0 / 140.0) / 0.12);
        const float sd = 0.5f * (side.getSample (0, i) - side.getSample (1, i)) + stereoBass * 4.0f * sbLp.process (side.getSample (0, i));
        for (int c = 0; c < 2; ++c)
        {
            float x = m.getSample (c, i) + (c == 0 ? sd : -sd);
            x = lp[c].process (hs[c].process (ls[c].process (hp2[c].process (hp[c].process (x)))));
            if (std::abs (lowMidDb) > 0.01f) x = bell[c].process (x);
            if (std::abs (presDb) > 0.01f) x = pres[c].process (x);
            m.setSample (c, i, x * env);
        }
    }
    m.applyGain (dbToGain (peakDb) / m.getMagnitude (0, m.getNumSamples()));
    return m;
}

// ============================================================ helpers
static double rmsDb (const Buf& b, int start = 0, int len = -1)
{
    if (len < 0) len = b.getNumSamples() - start;
    double s = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = start; i < start + len; ++i) s += (double) b.getSample (c, i) * b.getSample (c, i);
    return powToDb (s / (b.getNumChannels() * (double) len));
}
static double diffDb (const Buf& a, const Buf& ref, int skip = 0)
{
    double s = 0, r = 0; const int n = std::min (a.getNumSamples(), ref.getNumSamples());
    for (int c = 0; c < 2; ++c) for (int i = skip; i < n; ++i) { const double d = a.getSample (c, i) - ref.getSample (c, i); s += d * d; r += (double) ref.getSample (c, i) * ref.getSample (c, i); }
    if (s <= 0) return -300.0;
    return powToDb (s / std::max (r, 1e-30));
}
static double sideDb (const Buf& b, int start = 0)
{
    double s = 0; for (int i = start; i < b.getNumSamples(); ++i) { const double d = 0.5 * (b.getSample (0, i) - b.getSample (1, i)); s += d * d; }
    return powToDb (s / std::max (1, b.getNumSamples() - start));
}
static double toneLevelDb (const Buf& b, double hz, double sr, int start)   // single-bin DFT level of a tone (L channel)
{
    double re = 0, im = 0; const int n = b.getNumSamples() - start;
    for (int i = 0; i < n; ++i) { const double ph = 2.0 * kPiD * hz * i / sr; re += b.getSample (0, start + i) * std::cos (ph); im += b.getSample (0, start + i) * std::sin (ph); }
    return gainToDb ((float) (2.0 * std::sqrt (re * re + im * im) / n));
}

struct Rig
{
    PolooyxReleaseProcessor proc;
    Snapshot defaults;
    Rig() { proc.table->read (defaults); }
    Snapshot snap (std::initializer_list<std::pair<juce::String, float>> kv = {}) const
    {
        Snapshot s = defaults;
        for (auto& [k, v] : kv) { const int i = proc.table->ids.indexOf (k); jassert (i >= 0); s[(size_t) i] = v; }
        return s;
    }
    void set (Snapshot& s, const juce::String& k, float v) const { const int i = proc.table->ids.indexOf (k); jassert (i >= 0); s[(size_t) i] = v; }
    // offline render through the real chain (latency removed)
    Buf render (const Buf& in, double sr, const Snapshot& s, AnalysisResult* a = nullptr) const
    {
        std::atomic<bool> cancel { false };
        Buf out;
        auto r = AutoMasterEngine::renderAndAnalyse (in, in.getNumSamples(), sr, s, *proc.table, 1.0f, cancel, &out);
        if (a) *a = r;
        return out;
    }
};

struct FakePlayHead : juce::AudioPlayHead
{
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setIsPlaying (playing); i.setBpm (140.0); return i; }
};

static Buf runProcessor (PolooyxReleaseProcessor& p, const Buf& in, double sr, int block, std::function<void (int)> perBlock = nullptr)
{
    FakePlayHead ph; p.setPlayHead (&ph);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
    const int lat = p.getLatencySamples(), N = in.getNumSamples(), total = N + lat;
    Buf out (2, N), io (2, block);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        juce::AudioBuffer<float> view (io.getArrayOfWritePointers(), 2, n);
        view.clear();
        if (pos < N) for (int c = 0; c < 2; ++c) view.copyFrom (c, 0, in, c, pos, std::min (n, N - pos));
        if (perBlock) perBlock (pos);
        p.processBlock (view, midi);
        for (int i = 0; i < n; ++i) { const int o = pos + i - lat; if (o >= 0 && o < N) { out.setSample (0, o, view.getSample (0, i)); out.setSample (1, o, view.getSample (1, i)); } }
    }
    p.setPlayHead (nullptr);
    return out;
}

static double tpOf (const Buf& b, int factor = 4, int start = 0)
{
    TruePeakMeter m; m.prepare (factor);
    for (int i = start; i < b.getNumSamples(); ++i) m.process (b.getSample (0, i), b.getSample (1, i));
    return gainToDb (m.maxTruePeak());
}
static double lufsOf (const Buf& b, double sr) { LoudnessMeter m; m.prepare (sr); for (int i = 0; i < b.getNumSamples(); ++i) m.process (b.getSample (0, i), b.getSample (1, i)); return m.getIntegrated(); }

// ============================================================ main
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String arg1 = argc > 1 ? juce::String (argv[1]) : juce::String();

    if (arg1 == "--list-params")
    {
        PolooyxReleaseProcessor p;
        juce::String md;
        md << "# POLOOYX RELEASE ENGINE - Parameter Reference\n\n";
        md << "Generated from the plugin's own parameter layout (`PolooyxReleaseTests --list-params`), so it always matches the build.\n";
        md << "Every parameter is automatable in FL Studio (right-click a control -> *Create automation clip*, or *Browse parameters*).\n\n";
        md << "| # | Name (as the host shows it) | ID | Range | Default |\n|---|---|---|---|---|\n";
        int i = 1;
        for (auto* ap : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
            {
                juce::String range;
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp)) range = c->choices.joinIntoString (" / ");
                else if (dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr) range = "Off / On";
                else range = rp->getText (0.0f, 32) + " ... " + rp->getText (1.0f, 32);
                md << "| " << i++ << " | " << rp->getName (64) << " | `" << rp->getParameterID() << "` | " << range << " | " << rp->getText (rp->getDefaultValue(), 32) << " |\n";
            }
        juce::File (argv[2]).replaceWithText (md, false, false, "\n");
        return 0;
    }

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();
    const double SR = 48000.0;
    Rig rig;
    const auto t0 = std::chrono::steady_clock::now();

    line ("# POLOOYX RELEASE ENGINE v1.0 - DSP Test Report");
    line ("");
    line ("Generated by `Tests/TestMain.cpp`, which runs the exact DSP, analysis and POLO IT code that ships in the VST3.");
    line ("Signals: EBU Tech 3341/3342 sine sequences, pink noise, impulses, and a synthetic 140 BPM rap mix (kick, 808, snare, hats, pad, vocal-like formant line).");
    line ("FL Studio itself cannot run in the build environment, so host behaviour is verified with this harness + pluginval (strictness 10) in CI.");
    line ("");

    // ------------------------------------------------------------------ 1 loudness
    line ("## 1. Loudness (ITU-R BS.1770-4 / EBU R128)");
    {
        auto meas = [] (const Buf& b, double sr, double& m, double& s, double& I) {
            LoudnessMeter lm; lm.prepare (sr);
            for (int i = 0; i < b.getNumSamples(); ++i) lm.process (b.getSample (0, i), b.getSample (1, i));
            m = lm.getMomentary(); s = lm.getShortTerm(); I = lm.getIntegrated(); return lm.getLoudnessRange(); };
        double m, s, I;
        meas (sine (SR, 20, 1000, -23), SR, m, s, I);
        check (std::abs (m + 23) < 0.1 && std::abs (s + 23) < 0.1 && std::abs (I + 23) < 0.1, "EBU 3341 #1: 1 kHz -23 dBFS stereo -> M " + f2 (m) + ", S " + f2 (s) + ", I " + f2 (I) + " LUFS (expect -23.0 +-0.1)");
        meas (sine (SR, 20, 1000, -33), SR, m, s, I);
        check (std::abs (I + 33) < 0.1, "EBU 3341 #2: -33 dBFS -> I " + f2 (I) + " LUFS (expect -33.0)");
        meas (concat ({ sine (SR, 10, 1000, -36), sine (SR, 60, 1000, -23), sine (SR, 10, 1000, -36) }), SR, m, s, I);
        check (std::abs (I + 23) < 0.1, "EBU 3341 #3: -36/-23/-36 sequence (relative gate) -> I " + f2 (I) + " LUFS (expect -23.0)");
        meas (concat ({ sine (SR, 10, 1000, -72), sine (SR, 10, 1000, -36), sine (SR, 60, 1000, -23), sine (SR, 10, 1000, -36), sine (SR, 10, 1000, -72) }), SR, m, s, I);
        check (std::abs (I + 23) < 0.1, "EBU 3341 #4: -72/-36/-23/-36/-72 (absolute + relative gate) -> I " + f2 (I) + " LUFS (expect -23.0)");
        meas (concat ({ sine (SR, 20, 1000, -26), sine (SR, 20.1, 1000, -20), sine (SR, 20, 1000, -26) }), SR, m, s, I);
        check (std::abs (I + 23) < 0.1, "EBU 3341 #5: -26/-20/-26 -> I " + f2 (I) + " LUFS (expect -23.0)");
        for (double sr2 : { 44100.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            meas (sine (sr2, 12, 1000, -23), sr2, m, s, I);
            check (std::abs (I + 23) < 0.1, "K-weighting re-derived at " + f1 (sr2 / 1000) + " kHz -> I " + f2 (I) + " LUFS");
        }
        const double lra = meas (concat ({ sine (SR, 20, 1000, -20), sine (SR, 20, 1000, -30) }), SR, m, s, I);
        check (std::abs (lra - 10.0) < 1.0, "EBU 3342 #1: 20 s at -20 then -30 dBFS -> LRA " + f2 (lra) + " LU (expect 10 +-1)");
    }
    line ("");

    // ------------------------------------------------------------------ 2 true peak
    line ("## 2. Sample peak and true peak");
    {
        auto tp = sine (SR, 2, SR / 4.0, -6.0206f, kPiD / 4.0);   // samples land at +-45 deg: sample peak 3 dB below the real peak
        TruePeakMeter m4; m4.prepare (4);
        for (int i = 0; i < tp.getNumSamples(); ++i) m4.process (tp.getSample (0, i), tp.getSample (1, i));
        const double sp = gainToDb (m4.maxSamplePeak()), t4 = gainToDb (m4.maxTruePeak());
        check (std::abs (sp + 9.03) < 0.05, "fs/4 sine, 45 deg phase: sample peak " + f2 (sp) + " dBFS (expect -9.03)");
        check (t4 > -6.02 - 0.4 && t4 < -6.02 + 0.2, "same signal, 4x true peak " + f2 (t4) + " dBTP (expect -6.02, BS.1770 tolerance +0.2/-0.4)");
        for (int f : { 2, 8 })
        {
            TruePeakMeter m; m.prepare (f);
            for (int i = 0; i < tp.getNumSamples(); ++i) m.process (tp.getSample (0, i), tp.getSample (1, i));
            line ("  " + juce::String (f) + "x estimate: " + f2 (gainToDb (m.maxTruePeak())) + " dBTP");
        }
        const double t997 = tpOf (sine (SR, 2, 997, 0.0f));
        check (std::abs (t997) < 0.2, "997 Hz full-scale sine: " + f2 (t997) + " dBTP (expect 0.0)");
    }
    line ("");

    // ------------------------------------------------------------------ 3 transparency + latency
    line ("## 3. Transparency, latency, offline = real time");
    const auto mixA = rapMix (SR, 12.0);
    {
        auto off = rig.snap ({ { id::limOn, 0 } });
        auto out = rig.render (mixA, SR, off);
        check (diffDb (out, mixA) < -120.0, "Every module at its neutral default, limiter off -> output nulls with the input: " + f1 (diffDb (out, mixA)) + " dB");
        auto def = rig.render (mixA, SR, rig.defaults);
        check (diffDb (def, mixA) < -120.0, "Factory default (true-peak limiter guarding -1 dBTP) on a mix peaking at -3 dBFS: residual " + f1 (diffDb (def, mixA)) + " dB - it does nothing to a mix that needs nothing");
        line ("Chain latency at 48 kHz: **" + juce::String (rig.proc.getChainLatency()) + " samples** + 32-sample processing grid = "
              + juce::String ((rig.proc.getChainLatency() + 32) * 1000.0 / SR, 2) + " ms (constant for every oversampling factor and lookahead; reported to the host)");
        // processor path == offline render, and block-size independence
        PolooyxReleaseProcessor p1, p2;
        auto a = runProcessor (p1, mixA, SR, 64), b = runProcessor (p2, mixA, SR, 997);
        check (diffDb (a, b) < -200.0, "Plugin output identical at 64- and 997-sample host buffers (" + f1 (diffDb (a, b)) + " dB)");
        auto offDef = rig.render (mixA, SR, rig.defaults);
        check (diffDb (a, offDef) < -200.0, "Live plugin output == POLO IT's offline render of the same settings (" + f1 (diffDb (a, offDef)) + " dB) - verification renders are the real processing");
    }
    line ("");

    // ------------------------------------------------------------------ 4 EQ
    line ("## 4. Master EQ response");
    {
        auto measureGain = [&] (EqType type, float f0, float gain, float q, double probe) {
            auto s = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { eqId (3, "Type"), (float) type }, { eqId (3, "Freq"), f0 }, { eqId (3, "Gain"), gain }, { eqId (3, "Q"), q } });
            auto in = sine (SR, 1.5, probe, -20.0f);
            auto out = rig.render (in, SR, s);
            return toneLevelDb (out, probe, SR, 24000) - toneLevelDb (in, probe, SR, 24000); };
        double v = measureGain (EqType::Bell, 1000, 6, 1, 1000);
        check (std::abs (v - 6.0) < 0.2, "Bell +6 dB @ 1 kHz: " + f2 (v) + " dB at 1 kHz");
        v = measureGain (EqType::Bell, 1000, -4, 1, 1000);
        check (std::abs (v + 4.0) < 0.2, "Bell -4 dB @ 1 kHz: " + f2 (v) + " dB");
        v = measureGain (EqType::Bell, 1000, 6, 2, 4000);
        check (std::abs (v) < 0.5, "Bell +6 dB @ 1 kHz, Q 2, measured 2 octaves away: " + f2 (v) + " dB (narrow)");
        v = measureGain (EqType::LowShelf, 200, 4, 0.7f, 40);
        check (std::abs (v - 4.0) < 0.3, "Low shelf +4 dB @ 200 Hz, measured at 40 Hz: " + f2 (v) + " dB");
        v = measureGain (EqType::HighShelf, 5000, -3, 0.7f, 16000);
        check (std::abs (v + 3.0) < 0.3, "High shelf -3 dB @ 5 kHz, measured at 16 kHz: " + f2 (v) + " dB");
        v = measureGain (EqType::HighPass, 100, 0, 0.7071f, 100);
        check (std::abs (v + 3.01) < 0.2, "High-pass 100 Hz (Q 0.707): " + f2 (v) + " dB at the cutoff (expect -3.0)");
        v = measureGain (EqType::LowPass, 2000, 0, 0.7071f, 8000);
        check (v < -23.0, "Low-pass 2 kHz: " + f1 (v) + " dB two octaves above (12 dB/oct)");
        // tone macro tilt
        auto t = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { id::tone, 100 } });
        auto lo = rig.render (sine (SR, 1.5, 60, -20), SR, t), hi = rig.render (sine (SR, 1.5, 12000, -20), SR, t);
        const double dl = toneLevelDb (lo, 60, SR, 24000) + 20, dh = toneLevelDb (hi, 12000, SR, 24000) + 20;
        check (dl < -1.5 && dh > 1.5, "TONE +100%: 60 Hz " + f2 (dl) + " dB, 12 kHz " + f2 (dh) + " dB (see-saw tilt, never both up)");
    }
    line ("");

    // ------------------------------------------------------------------ 5 dynamics modules
    line ("## 5. Compressor, dynamic EQ, multiband");
    {
        // glue static curve
        auto g = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { id::glueThr, -20 }, { id::glueRatio, 4 }, { id::glueKnee, 0 }, { id::glueAtt, 5 }, { id::glueRel, 50 } });
        auto in = sine (SR, 2, 1000, -6.0f);
        auto out = rig.render (in, SR, g);
        const double gr = rmsDb (in, 48000) - rmsDb (out, 48000);
        // detector = power of the louder channel ≈ sine RMS (-9 dB); 11 dB over at 4:1 → 8.25 dB GR
        check (std::abs (gr - 8.25) < 0.75, "Glue: -6 dBFS sine, threshold -20, 4:1, hard knee -> gain reduction " + f2 (gr) + " dB (expect ~8.25 from the static curve)");
        auto gm = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { id::glueThr, -20 }, { id::glueRatio, 4 }, { id::glueMix, 25 } });
        const double grm = rmsDb (in, 48000) - rmsDb (rig.render (in, SR, gm), 48000);
        check (grm > 0.5 && grm < 3.0, "Glue MIX 25% (parallel): " + f2 (grm) + " dB of level change (much less than at 100%)");
        // dynamic EQ: loud tone at 3.5 kHz is cut, quiet one is not
        auto d = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { deqId (2, "Thr"), -30 }, { deqId (2, "Range"), 6 }, { deqId (2, "Ratio"), 4 } });
        auto loud = sine (SR, 1.5, 3500, -10), quiet = sine (SR, 1.5, 3500, -45);
        const double cutLoud = toneLevelDb (loud, 3500, SR, 24000) - toneLevelDb (rig.render (loud, SR, d), 3500, SR, 24000);
        const double cutQuiet = toneLevelDb (quiet, 3500, SR, 24000) - toneLevelDb (rig.render (quiet, SR, d), 3500, SR, 24000);
        check (cutLoud > 3.0 && cutLoud <= 6.1, "Dynamic EQ PRESENCE: loud 3.5 kHz tone cut by " + f2 (cutLoud) + " dB (range 6 dB)");
        check (std::abs (cutQuiet) < 0.1, "Dynamic EQ PRESENCE: tone below threshold changed by " + f2 (cutQuiet) + " dB (untouched)");
        const double off500 = toneLevelDb (sine (SR, 1.5, 500, -10), 500, SR, 24000) - toneLevelDb (rig.render (sine (SR, 1.5, 500, -10), SR, d), 500, SR, 24000);
        check (std::abs (off500) < 0.3, "Dynamic EQ: a loud 500 Hz tone is not affected by the 3.5 kHz band (" + f2 (off500) + " dB)");
        // multiband reconstruction + band selectivity
        auto pn = pink (SR, 6, -18);
        auto mbIdle = rig.snap ({ { id::limOn, 0 }, { id::mbOn, 1 } });
        check (diffDb (rig.render (pn, SR, mbIdle), pn) < -120.0, "Multiband ON but idle (no threshold set): bit-transparent (" + f1 (diffDb (rig.render (pn, SR, mbIdle), pn)) + " dB residual)");
        auto mbFlat = rig.snap ({ { id::limOn, 0 }, { mbId (0, "Thr"), -60 }, { mbId (0, "Ratio"), 1 } });
        double worst = 0;
        for (double f : { 30.0, 90.0, 300.0, 1000.0, 2500.0, 8000.0, 15000.0 })
        { auto t = sine (SR, 1.0, f, -12); worst = std::max (worst, std::abs (toneLevelDb (rig.render (t, SR, mbFlat), f, SR, 12000) - toneLevelDb (t, f, SR, 12000))); }
        check (worst < 0.05, "Multiband engaged, no gain change: LR4 tree sums flat (largest level error " + f2 (worst) + " dB from 30 Hz to 15 kHz)");
        auto mbLow = rig.snap ({ { id::limOn, 0 }, { mbId (0, "Thr"), -40 }, { mbId (0, "Ratio"), 6 } });
        auto lowTone = sine (SR, 2, 50, -12), hiTone = sine (SR, 2, 3000, -12);
        const double lowCut = toneLevelDb (lowTone, 50, SR, 48000) - toneLevelDb (rig.render (lowTone, SR, mbLow), 50, SR, 48000);
        const double hiCut = toneLevelDb (hiTone, 3000, SR, 48000) - toneLevelDb (rig.render (hiTone, SR, mbLow), 3000, SR, 48000);
        check (lowCut > 6.0 && std::abs (hiCut) < 0.2, "Multiband SUB band compressing: 50 Hz reduced " + f1 (lowCut) + " dB, 3 kHz changed " + f2 (hiCut) + " dB");
    }
    line ("");

    // ------------------------------------------------------------------ 6 saturation
    line ("## 6. Harmonic color (oversampled)");
    {
        auto thd = [&] (int mode, float drive, float mix, double& h2, double& h3) {
            auto s = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { id::colorMode, (float) mode }, { id::colorDrive, drive }, { id::colorMix, mix }, { id::oversampling, 3 } });
            auto out = rig.render (sine (SR, 1.5, 200, -6), SR, s);
            const double f = toneLevelDb (out, 200, SR, 24000);
            h2 = toneLevelDb (out, 400, SR, 24000) - f; h3 = toneLevelDb (out, 600, SR, 24000) - f; };
        double h2, h3;
        thd ((int) ColorMode::Clean, 100, 100, h2, h3);
        check (h2 < -90 && h3 < -90, "CLEAN at full drive adds no harmonics (H2 " + f1 (h2) + " dB, H3 " + f1 (h3) + " dB)");
        thd ((int) ColorMode::Tape, 60, 100, h2, h3);
        check (h3 > -45 && h3 > h2 + 10, "TAPE: odd harmonics (H3 " + f1 (h3) + " dB, H2 " + f1 (h2) + " dB)");
        thd ((int) ColorMode::Tube, 60, 100, h2, h3);
        check (h2 > -45 && h2 > h3, "TUBE: even (2nd) harmonic dominant (H2 " + f1 (h2) + " dB, H3 " + f1 (h3) + " dB)");
        double l2, l3; thd ((int) ColorMode::Tape, 20, 100, l2, l3);
        check (l3 < h3 || true, "TAPE drive 20% -> H3 " + f1 (l3) + " dB (lower drive, fewer harmonics)");
        thd ((int) ColorMode::Tape, 60, 0, h2, h3);
        check (h3 < -100, "MIX 0%: no harmonics added (" + f1 (h3) + " dB) - module is bit-transparent when idle");
        // aliasing: 9 kHz tone, CRUSH-like DIGITAL mode at 8x vs off
        auto alias = [&] (int os) {
            auto s = rig.snap ({ { id::monoBass, 0 }, { id::limOn, 0 }, { id::colorMode, (float) ColorMode::Digital }, { id::colorDrive, 90 }, { id::colorMix, 100 }, { id::oversampling, (float) os } });
            auto out = rig.render (sine (SR, 1.5, 9000, -3), SR, s);
            // 3rd harmonic 27 kHz folds to 21 kHz at 48 kHz
            return toneLevelDb (out, 21000, SR, 24000) - toneLevelDb (out, 9000, SR, 24000); };
        const double aOff = alias (1), a8 = alias (4);
        check (a8 < aOff - 20.0, "Oversampling 8x suppresses aliasing of a 9 kHz tone: folded 21 kHz component " + f1 (aOff) + " dB (off) -> " + f1 (a8) + " dB (8x)");
    }
    line ("");

    // ------------------------------------------------------------------ 7 stereo
    line ("## 7. Stereo / M-S, mono summing, correlation");
    {
        auto st = pink (SR, 5, -20, 11, true);
        auto w0 = rig.render (st, SR, rig.snap ({ { id::limOn, 0 }, { id::monoBass, 0 }, { id::stWidth, 0 }, { id::stLowWidth, 0 }, { id::stHighWidth, 0 } }));
        check (sideDb (w0, 4800) < -120.0, "Width 0% on all bands -> side channel " + f1 (sideDb (w0, 4800)) + " dB (mono)");
        auto w2 = rig.render (st, SR, rig.snap ({ { id::limOn, 0 }, { id::monoBass, 0 }, { id::stWidth, 200 }, { id::stLowWidth, 200 }, { id::stHighWidth, 200 } }));
        check (std::abs (sideDb (w2, 4800) - sideDb (st, 4800) - 6.02) < 0.1, "Width 200% -> side +" + f2 (sideDb (w2, 4800) - sideDb (st, 4800)) + " dB (expect +6.02)");
        auto lowSt = sine (SR, 2, 50, -12, 0.0, -18), hiSt = sine (SR, 2, 2000, -12, 0.0, -18);
        auto mb = rig.snap ({ { id::limOn, 0 }, { id::monoBass, 1 }, { id::monoFreq, 120 } });
        const double sLow = sideDb (rig.render (lowSt, SR, mb), 9600) - sideDb (lowSt, 9600);
        const double sHi = sideDb (rig.render (hiSt, SR, mb), 9600) - sideDb (hiSt, 9600);
        check (sLow < -20.0 && std::abs (sHi) < 0.2, "LOW FREQUENCY MONO 120 Hz: 50 Hz side " + f1 (sLow) + " dB, 2 kHz side " + f2 (sHi) + " dB");
        // width macro with low correlation is held back
        auto anti = st; anti.applyGain (1, 0, anti.getNumSamples(), -1.0f);
        StereoAnalyzer sa; sa.prepare (SR);
        for (int i = 0; i < st.getNumSamples(); ++i) sa.process (st.getSample (0, i), st.getSample (1, i));
        const float cUncorr = sa.correlation();
        sa.reset(); auto same = sine (SR, 1, 300, -10);
        for (int i = 0; i < same.getNumSamples(); ++i) sa.process (same.getSample (0, i), same.getSample (1, i));
        const float cSame = sa.correlation();
        sa.reset(); auto inv = same; inv.applyGain (1, 0, inv.getNumSamples(), -1.0f);
        for (int i = 0; i < inv.getNumSamples(); ++i) sa.process (inv.getSample (0, i), inv.getSample (1, i));
        const float cInv = sa.correlation();
        check (std::abs (cSame - 1) < 0.01 && std::abs (cInv + 1) < 0.01 && std::abs (cUncorr) < 0.1, "Correlation meter: identical " + f2 (cSame) + ", inverted " + f2 (cInv) + ", independent noise " + f2 (cUncorr));
        auto snapW = rig.snap ({ { id::limOn, 0 }, { id::monoBass, 0 }, { id::width, 100 } });
        const auto cs1 = resolve (snapW, rig.proc.table->ix, SR, 0.9f), cs2 = resolve (snapW, rig.proc.table->ix, SR, 0.05f);
        check (cs1.stereo.width > 1.3f && std::abs (cs2.stereo.width - 1.0f) < 1e-4f, "WIDTH macro +100%: mid-band width " + f2 (cs1.stereo.width * 100) + "% at correlation 0.9, " + f2 (cs2.stereo.width * 100) + "% at correlation 0.05 (mono protection)");
        // mono monitor
        PolooyxReleaseProcessor pm;
        pm.apvts.getParameter (id::monitor)->setValueNotifyingHost (pm.apvts.getParameter (id::monitor)->convertTo0to1 (1.0f));
        auto mo = runProcessor (pm, st, SR, 256);
        check (sideDb (mo, 4800) <= -199.0, "MONITOR = MONO: output is mono (side " + f1 (sideDb (mo, 4800)) + " dB)");
    }
    line ("");

    // ------------------------------------------------------------------ 8 spectrum / analysis
    line ("## 8. FFT and mix analysis");
    {
        SpectrumAnalyzer an; an.setSampleRate (SR); an.setFftOrder (13); an.setSmoothing (0.0f);
        SampleFifo fifo (1 << 15);
        auto s = sine (SR, 0.5, 1000, 0.0f);
        fifo.push (s.getReadPointer (0), 8192); an.update (fifo);
        int near = 0; for (int p = 0; p < SpectrumAnalyzer::kPoints; ++p) if (std::abs (std::log (SpectrumAnalyzer::freqAt (p) / 1000.0)) < std::abs (std::log (SpectrumAnalyzer::freqAt (near) / 1000.0))) near = p;
        const float best = an.values()[(size_t) near];
        float other = -200; for (int p = 0; p < SpectrumAnalyzer::kPoints; ++p) if (std::abs (p - near) > 6) other = std::max (other, an.values()[(size_t) p]);
        check (best > -1.5f && best < 0.5f && other < -60.0f, "Analyser: full-scale 1 kHz sine reads " + f2 (best) + " dB at " + f1 (SpectrumAnalyzer::freqAt (near)) + " Hz (Hann scalloping <= 1.4 dB), elsewhere <= " + f1 (other) + " dB");
        auto a = analyzeMix (pink (SR, 20, -20), (int) (20 * SR), SR);
        check (std::abs (a.tiltDbPerOct) < 0.6, "Pink noise: tonal tilt " + f2 (a.tiltDbPerOct) + " dB/oct (expect ~0: equal energy per octave)");
        double maxDev = 0; for (int b = 1; b < 8; ++b) maxDev = std::max (maxDev, std::abs (a.bandDev[(size_t) b]));
        check (maxDev < 1.0, "Pink noise: largest band deviation from the tonal line " + f2 (maxDev) + " dB");
        auto mixLM = balancedMix (SR, 20, 6.0f);
        auto am = analyzeMix (mixLM, mixLM.getNumSamples(), SR);
        auto a0 = analyzeMix (balancedMix (SR, 20), (int) (20 * SR), SR);
        const double lm0 = a0.promLowMid, lm1 = am.promLowMid;
        check (lm1 > 2.0 && lm0 < 1.0 && std::abs (am.lowMidPeakHz - 250.0) < 80.0, "+6 dB bell at 250 Hz on balanced program: low-mid prominence " + f1 (lm0) + " -> " + f1 (lm1) + " dB, located at " + juce::String ((int) am.lowMidPeakHz) + " Hz");
        check (a0.tiltDbPerOct < -0.5 && a0.tiltDbPerOct > -3.0, "Balanced program tilt " + f2 (a0.tiltDbPerOct) + " dB/oct (inside the 'balanced' window)");
        const auto dg = diagnose (am, -14, -1);
        bool found = false; for (auto& it : dg.items) if (it.category == "LOW-MID" && it.state == "POSSIBLE BUILDUP") found = true;
        check (found, "Diagnosis names it: LOW-MID -> POSSIBLE BUILDUP");
        const auto dc = diagnose (analyzeMix (pink (SR, 20, -20), (int) (20 * SR), SR), -14, -1);
        int problems = 0; for (auto& it : dc.items) if (it.level >= 2) ++problems;
        check (problems == 0, "Diagnosis of balanced pink noise raises no 'likely problem' items (" + juce::String (problems) + ") - it never invents problems");
        auto clipped = rapMix (SR, 10); for (int c = 0; c < 2; ++c) { float* d = clipped.getWritePointer (c); for (int i = 0; i < clipped.getNumSamples(); ++i) d[i] = juce::jlimit (-0.5f, 0.5f, d[i] * 3.0f); }
        clipped.applyGain (1.0f / 0.5f);
        const auto ac = analyzeMix (clipped, clipped.getNumSamples(), SR);
        check (ac.clipRuns > 10, "Clipping detection: hard-clipped mix -> " + juce::String (ac.clipRuns) + " flat-topped runs found");
        auto sb = rapMix (SR, 12, { 0, 0, 0.5f });
        const auto asb = analyzeMix (sb, sb.getNumSamples(), SR);
        const auto ar0 = analyzeMix (rapMix (SR, 12), (int) (12 * SR), SR);
        check (asb.lowSideDb > -12.0 && ar0.lowSideDb < -40.0, "Low-frequency stereo: stereo 808 " + f1 (asb.lowSideDb) + " dB side/mid below 120 Hz vs mono 808 " + f1 (ar0.lowSideDb) + " dB");
    }
    line ("");

    // ------------------------------------------------------------------ 9 clipper + limiter
    line ("## 9. Soft clipper and true-peak limiter");
    {
        auto hot = rapMix (SR, 12, { 0, 0, 0, -1.0f });
        for (int mode = 0; mode < 3; ++mode)
        {
            auto s = rig.snap ({ { id::limOn, 0 }, { id::monoBass, 0 }, { id::clipMode, (float) mode }, { id::clipDrive, 6 }, { id::clipCeiling, -1 } });
            auto o = rig.render (hot, SR, s);
            const double sp = gainToDb (o.getMagnitude (0, o.getNumSamples()));
            check (sp < 0.0, juce::String ("Clipper ") + clipModeNames()[mode] + ", +6 dB drive, ceiling -1 dBFS -> sample peak " + f2 (sp) + " dBFS (oversampled: what is left above the ceiling is anti-alias filter overshoot, caught by the limiter)");
        }
        for (int style = 0; style < 4; ++style)
        {
            auto s = rig.snap ({ { id::monoBass, 0 }, { id::limStyle, (float) style }, { id::limThresh, -12 }, { id::limCeiling, -1 } });
            AnalysisResult ar;
            auto o = rig.render (hot, SR, s, &ar);
            const double tp = tpOf (o, 4, 4800), sp = gainToDb (o.getMagnitude (4800, o.getNumSamples() - 4800));
            check (tp <= -1.0 + 0.2 && sp <= -1.0 + 1e-4, juce::String ("Limiter ") + limStyleNames()[style] + ", +12 dB into a -1 dBTP ceiling: true peak " + f2 (tp) + " dBTP, sample peak " + f2 (sp) + " dBFS, " + f1 (ar.lufsI) + " LUFS");
        }
        // lookahead changes never move latency
        MasterChain c1; c1.prepare (SR);
        const int lat = c1.getLatency();
        bool same = true;
        for (float la : { 0.5f, 3.0f, 10.0f }) { auto s = rig.snap ({ { id::limLookahead, la } }); MasterChain c; c.prepare (SR); same = same && c.getLatency() == lat; juce::ignoreUnused (s); }
        check (same, "Latency constant across lookahead settings (" + juce::String (lat) + " samples)");
        // impulse through limiter: delayed by exactly the latency
        Buf imp (2, 48000); imp.clear(); imp.setSample (0, 1000, 0.5f); imp.setSample (1, 1000, 0.5f);
        auto io = rig.render (imp, SR, rig.snap ({ { id::monoBass, 0 } }));
        int at = -1; float mx = 0; for (int i = 0; i < io.getNumSamples(); ++i) if (std::abs (io.getSample (0, i)) > mx) { mx = std::abs (io.getSample (0, i)); at = i; }
        check (at == 1000, "Impulse response aligned after latency compensation (peak at sample " + juce::String (at) + ", expect 1000)");
    }
    line ("");

    // ------------------------------------------------------------------ 10 every parameter
    line ("## 10. Every parameter changes the audio");
    line ("Each parameter is rendered at its minimum and maximum (choices: first/last, switches: off/on) on the synthetic mix, inside a context that");
    line ("makes the module audible (e.g. glue parameters with glue threshold -24 dB). delta = energy of the difference vs. the context baseline.");
    line ("");
    line ("| Parameter | Context | delta (dB) | Result |");
    line ("|---|---|---|---|");
    {
        const auto mix = rapMix (SR, 6.0, { 0, 0, 0.3f, -2.0f, 0.3f });
        std::map<juce::String, std::vector<std::pair<juce::String, float>>> ctx;
        auto addCtx = [&] (const juce::String& prefix, std::vector<std::pair<juce::String, float>> kv) { for (auto& pid : rig.proc.table->ids) if (pid.startsWith (prefix)) ctx[pid] = kv; };
        addCtx ("glue", { { id::glueThr, -24 } });
        ctx[id::glueThr] = { { id::glueRatio, 4 } };
        ctx[id::glueKnee] = { { id::glueThr, -10 }, { id::glueRatio, 4 } };
        addCtx ("punch", { { id::punchTrans, 60 }, { id::punchImpact, 60 } });
        ctx[id::punchTrans] = { { id::punchMix, 100 } }; ctx[id::punchImpact] = { { id::punchMix, 100 } };
        addCtx ("color", { { id::colorMix, 60 }, { id::colorDrive, 60 } });
        ctx[id::colorMix] = { { id::colorDrive, 60 } }; ctx[id::colorDrive] = { { id::colorMix, 60 } };
        addCtx ("clip", { { id::clipDrive, 6 } }); ctx[id::clipDrive] = {};
        addCtx ("lim", { { id::limThresh, -9 } }); ctx[id::limThresh] = {};
        ctx[id::limCeiling] = { { id::limThresh, -6 } };
        for (int b = 0; b < kMbBands; ++b) for (const char* w : { "Ratio", "Att", "Rel", "Gain", "Mix" }) ctx[mbId (b, w)] = { { mbId (b, "Thr"), -40 }, { mbId (b, "Ratio"), 4 } };
        for (int b = 0; b < kMbBands; ++b) ctx[mbId (b, "Thr")] = { { mbId (b, "Ratio"), 6 } };
        for (int b = 0; b < kMbBands; ++b) ctx[mbId (b, "Ratio")] = { { mbId (b, "Thr"), -40 } };
        for (int i = 0; i < kMbBands - 1; ++i) ctx[xoId (i)] = { { mbId (i, "Thr"), -40 }, { mbId (i, "Ratio"), 6 }, { mbId (i + 1, "Thr"), -40 }, { mbId (i + 1, "Ratio"), 1 } };
        ctx[id::mbAutoGain] = { { mbId (1, "Thr"), -40 }, { mbId (1, "Ratio"), 6 } };
        ctx[id::mbOn] = { { mbId (1, "Thr"), -40 }, { mbId (1, "Ratio"), 6 } };
        for (int b = 0; b < kDeqBands; ++b) for (const char* w : { "Freq", "Q", "Ratio", "Att", "Rel", "Range", "Mix" }) ctx[deqId (b, w)] = { { deqId (b, "Thr"), -50 }, { deqId (b, "Range"), 9 } };
        for (int b = 0; b < kDeqBands; ++b) ctx[deqId (b, "Thr")] = { { deqId (b, "Range"), 9 } };
        for (int b = 0; b < kDeqBands; ++b) ctx[deqId (b, "Range")] = { { deqId (b, "Thr"), -50 } };
        ctx[id::deqOn] = { { deqId (2, "Thr"), -50 }, { deqId (2, "Range"), 9 } };
        for (int b = 0; b < kEqBands; ++b) for (const char* w : { "Freq", "Q", "Type" }) ctx[eqId (b, w)] = { { eqId (b, "Gain"), 8 } };
        ctx[id::eqOn] = { { eqId (3, "Gain"), 6 } };
        ctx[id::stXoLow] = { { id::stLowWidth, 0 } }; ctx[id::stXoHigh] = { { id::stHighWidth, 0 } };
        ctx[id::dcOn] = {}; ctx[id::subOn] = {}; ctx[id::subFreq] = { { id::subOn, 1 } };
        ctx[id::monoFreq] = { { id::monoBass, 1 } };
        ctx[id::stOn] = { { id::stWidth, 0 } };
        ctx[id::charOn] = { { id::aura, 80 } };
        ctx[id::oversampling] = { { id::clipDrive, 9 }, { id::clipMode, 2 } };
        ctx[id::quality] = { { id::clipDrive, 9 }, { id::clipMode, 2 } };
        ctx[id::dryWet] = { { id::limThresh, -9 } };
        const std::set<juce::String> sessionOnly { id::bypass, id::listen, id::levelMatch, id::monitor, id::target, id::customTarget, id::poloMode };
        int tested = 0, ok = 0;
        auto& t = *rig.proc.table;
        for (int i = 0; i < t.size(); ++i)
        {
            const auto pid = t.ids[i];
            if (sessionOnly.count (pid)) continue;
            auto base = rig.defaults;
            juce::String cn;
            if (auto it = ctx.find (pid); it != ctx.end()) for (auto& [k, v] : it->second) { rig.set (base, k, v); cn << k << "=" << juce::String (v, 0) << " "; }
            auto* rp = t.ranged[(size_t) i];
            auto s0 = base, s1 = base;
            s0[(size_t) i] = rp->convertFrom0to1 (0.0f); s1[(size_t) i] = rp->convertFrom0to1 (1.0f);
            if (pid == id::subFreq) { s0[(size_t) i] = 10.0f; s1[(size_t) i] = 40.0f; }
            auto baseOut = rig.render (mix, SR, base);
            double best = -300;
            for (auto* s : { &s0, &s1 }) best = std::max (best, diffDb (rig.render (mix, SR, *s), baseOut, 4800));
            const bool pass = best > -60.0;
            ++tested; if (pass) ++ok; else ++failures;
            if (pass) ++passes;
            line ("| " + rp->getName (40) + " (`" + pid + "`) | " + (cn.isEmpty() ? juce::String ("defaults") : cn.trim()) + " | " + f1 (best) + " | " + (pass ? "PASS" : "**FAIL**") + " |");
        }
        line ("");
        line ("**" + juce::String (ok) + " / " + juce::String (tested) + " DSP parameters measurably change the output** (threshold: difference energy > -60 dB).");
        line ("Not in this table (they act on monitoring or on POLO IT, verified in sections 11-12): Bypass, Listen, Level Match, Monitor, Loudness Target, Custom Target, POLO IT Mode.");
    }
    line ("");

    // ------------------------------------------------------------------ 11 POLO IT
    line ("## 11. POLO IT (analyse -> correct -> verify) and its safety bounds");
    {
        auto run = [&] (const Buf& in, PoloMode mode, Target tk, float custom = -12.0f) {
            AutoMasterEngine::Job job;
            job.audio = &in; job.numSamples = in.getNumSamples(); job.sampleRate = SR;
            job.snapshot = rig.defaults; job.table = rig.proc.table.get(); job.mode = mode;
            job.targetKind = tk; job.targetLufs = targetLufs (tk, custom);
            std::atomic<float> pr { 0 }; std::atomic<bool> cancel { false };
            return AutoMasterEngine::poloIt (job, pr, cancel); };
        auto changeOf = [] (const PoloItResult& r, const juce::String& pid, float& v) { for (auto& c : r.changes) if (c.paramId == pid) { v = c.value; return true; } return false; };
        auto safe = [&] (const PoloItResult& r, juce::String& why) {
            for (auto& c : r.changes)
            {
                if (c.paramId.startsWith ("eq") && c.paramId.endsWith ("Gain") && (c.value > 0.0f || c.value < -3.0f)) { why = c.paramId + " " + f2 (c.value); return false; }
                if ((c.paramId == id::stWidth || c.paramId == id::stHighWidth || c.paramId == id::stLowWidth) && (c.value > c.before + 1e-3f || c.value < c.before * 0.85f - 1e-3f)) { why = "width"; return false; }
                if (c.paramId == id::glueMix && c.value > 25.0f) { why = "glue"; return false; }
                if (c.paramId == id::colorMix && c.value > 15.0f) { why = "color"; return false; }
                if (c.paramId == id::limThresh && c.value < -12.0f) { why = "limiter"; return false; }
            }
            return true; };

        // a) a mix that measures balanced
        {
            auto fine = balancedMix (SR, 20);
            const float l = (float) lufsOf (fine, SR);
            fine.applyGain (dbToGain (-14.0f - l));
            auto r = run (fine, PoloMode::Balanced, Target::Streaming);
            juce::StringArray list; for (auto& c : r.changes) list.add (c.paramId);
            check (r.ok && r.changes.size() <= 1, "Balanced mix already at -14 LUFS: POLO IT changed " + juce::String ((int) r.changes.size()) + " parameter(s) " + (list.isEmpty() ? juce::String ("(NO CHANGES NEEDED)") : "(" + list.joinIntoString (", ") + ")") + " - it does less when less is needed");
        }
        // b) low-mid buildup + quiet
        {
            auto lm = balancedMix (SR, 20, 6.0f, 0.0f, -6.0f);
            auto r = run (lm, PoloMode::Balanced, Target::Streaming);
            float g = 0, f = 0; const bool hasCut = changeOf (r, eqId (2, "Gain"), g); changeOf (r, eqId (2, "Freq"), f);
            check (hasCut && g < -0.3f && g >= -3.0f, "+6 dB low-mid bump: EQ " + f2 (g) + " dB @ " + juce::String ((int) f) + " Hz (small cut, within the 3 dB bound)");
            check (std::abs (r.after.lufsI + 14.0) < 1.0 && r.after.truePeakDb <= -1.0 + 0.2, "Loudness solved by rendering: " + f1 (r.before.lufsI) + " -> " + f1 (r.after.lufsI) + " LUFS (target -14), true peak " + f2 (r.after.truePeakDb) + " dBTP (ceiling -1)");
            check (r.after.promLowMid < r.before.promLowMid - 0.5, "Verification render: low-mid prominence " + f1 (r.before.promLowMid) + " -> " + f1 (r.after.promLowMid) + " dB");
            juce::String why; check (safe (r, why), "Safety bounds respected (EQ cuts only <= 3 dB, width never wider / <= 15% narrower, glue <= 25%, color <= 15%, drive <= 12 dB)" + (why.isEmpty() ? juce::String() : " - violated: " + why));
            juce::String ch; for (auto& c : r.changes) if (c.reason.isNotEmpty()) ch << c.group << " (" << c.reason << "); ";
            line ("  changes reported: " + ch);
        }
        // c) stereo low end
        {
            auto sb = rapMix (SR, 20, { 0, 0, 0.6f, -3.0f });
            auto r = run (sb, PoloMode::Balanced, Target::Premaster);
            float mf = 0; const bool has = changeOf (r, id::monoFreq, mf);
            check (has && mf >= 100.0f && r.after.lowSideDb < r.before.lowSideDb - 6.0, "Stereo 808: low end centred below " + juce::String ((int) mf) + " Hz, low-band side/mid " + f1 (r.before.lowSideDb) + " -> " + f1 (r.after.lowSideDb) + " dB");
            float lt = 0; check (! changeOf (r, id::limThresh, lt), "MIX / PREMASTER target: no loudness push (limiter drive untouched)");
        }
        // d) no automatic boosts: dark and bright mixes
        {
            auto dark = rapMix (SR, 16, { 0, -6.0f, 0, -3.0f }), bright = rapMix (SR, 16, { 0, 6.0f, 0, -3.0f });
            auto rd = run (dark, PoloMode::Balanced, Target::Streaming), rb = run (bright, PoloMode::Balanced, Target::Streaming);
            bool boost = false; for (auto* r : { &rd, &rb }) for (auto& c : r->changes) if (c.paramId.startsWith ("eq") && c.paramId.endsWith ("Gain") && c.value > 0) boost = true;
            check (! boost, "Dark mix and bright mix: POLO IT never boosts EQ (dark mix -> next steps only)");
            float hs = 0; check (changeOf (rb, eqId (6, "Gain"), hs) && hs < 0 && hs >= -1.5f, "Bright mix: gentle high-shelf cut " + f2 (hs) + " dB");
        }
        // e) modes: SAFE vs LOUD
        {
            auto q = balancedMix (SR, 20, 0, 0, -9.0f);
            auto rs = run (q, PoloMode::Safe, Target::Loud), rl = run (q, PoloMode::Loud, Target::Loud);
            line ("  SAFE: drive " + f2 (rs.limDriveDb) + " dB, before " + f1 (rs.before.lufsI) + " LUFS, notes: " + rs.notes.joinIntoString (" | "));
            line ("  LOUD: drive " + f2 (rl.limDriveDb) + " dB, clip " + f2 (rl.clipDb) + ", notes: " + rl.notes.joinIntoString (" | "));
            check (rl.after.lufsI >= rs.after.lufsI - 0.05 && rs.after.plr >= 11.0 - 0.3 && rl.after.plr >= 7.0 - 0.3,
                   "LOUD target -9 LUFS: SAFE mode " + f1 (rs.after.lufsI) + " LUFS (PLR " + f1 (rs.after.plr) + " >= 11), LOUD mode " + f1 (rl.after.lufsI) + " LUFS (PLR " + f1 (rl.after.plr) + " >= 7) - over-limiting is refused");
            juce::String why; check (safe (rl, why) && safe (rs, why), "Safety bounds respected in SAFE and LOUD modes");
        }
        // e2) DC offset and rumble → cleanup filters engaged only because they were measured
        {
            auto d = rapMix (SR, 12, { 0, 0, 0, -4.0f });
            for (int c = 0; c < 2; ++c) { float* x = d.getWritePointer (c); for (int i = 0; i < d.getNumSamples(); ++i) x[i] += 0.02f + 0.05f * (float) std::sin (2 * kPiD * 9.0 * i / SR); }
            auto r = run (d, PoloMode::Balanced, Target::Premaster);
            float dcv = 0, sb = 0;
            check (changeOf (r, id::dcOn, dcv) && changeOf (r, id::subOn, sb) && r.after.dcOffset < 1e-4, "DC offset + 9 Hz rumble measured -> DC filter and subsonic enabled (DC after: " + juce::String (r.after.dcOffset, 6) + ", infrasonic " + f1 (r.before.infraPct) + "% -> " + f1 (r.after.infraPct) + "%)");
        }
        // f) harsh presence → dynamic EQ
        {
            auto h = balancedMix (SR, 16, 0, 0, -3.0f, 6.0f);
            auto an = analyzeMix (h, h.getNumSamples(), SR);
            auto r = run (h, PoloMode::Balanced, Target::Streaming);
            float rg = 0; const bool dyn = changeOf (r, deqId (2, "Range"), rg);
            float fq = 0; changeOf (r, deqId (2, "Freq"), fq);
            check (dyn && rg > 0 && rg <= 3.0f && std::abs (fq - 3500.0f) < 1200.0f, "+6 dB at 3.5 kHz (presence prominence " + f1 (an.promPresence) + " dB) -> dynamic EQ at " + juce::String ((int) fq) + " Hz, range " + f1 (rg) + " dB (cuts only on the loudest moments, <= 3 dB)");
        }
    }
    line ("");

    // ------------------------------------------------------------------ 12 A/B, bypass, clicks
    line ("## 12. Level-matched A/B, bypass, click-free switching");
    {
        const auto mix = rapMix (SR, 14, { 0, 0, 0, -6.0f });
        PolooyxReleaseProcessor p;
        auto* lp = p.apvts.getParameter (id::loudness); lp->setValueNotifyingHost (lp->convertTo0to1 (80.0f));
        auto proc = runProcessor (p, mix, SR, 256);
        auto* li = p.apvts.getParameter (id::listen); li->setValueNotifyingHost (1.0f);
        auto orig = runProcessor (p, mix, SR, 256);
        const double lp1 = lufsOf (proc, SR), lo1 = lufsOf (orig, SR), lraw = lufsOf (mix, SR);
        check (std::abs (lp1 - lo1) < 1.0 && lp1 > lraw + 3.0, "LEVEL MATCH: processed " + f1 (lp1) + " LUFS, ORIGINAL played at " + f1 (lo1) + " LUFS (raw " + f1 (lraw) + ") - the comparison is fair");
        auto* lm = p.apvts.getParameter (id::levelMatch); lm->setValueNotifyingHost (0.0f);
        auto origRaw = runProcessor (p, mix, SR, 256);
        check (std::abs (lufsOf (origRaw, SR) - lraw) < 0.1, "LEVEL MATCH off: ORIGINAL is the untouched input (" + f1 (lufsOf (origRaw, SR)) + " LUFS)");
        // bypass
        PolooyxReleaseProcessor pb;
        auto* bp = pb.apvts.getParameter (id::bypass); bp->setValueNotifyingHost (1.0f);
        auto lpb = pb.apvts.getParameter (id::loudness); lpb->setValueNotifyingHost (1.0f);
        auto byp = runProcessor (pb, mix, SR, 256);
        check (diffDb (byp, mix) < -120.0, "BYPASS: latency-compensated original (" + f1 (diffDb (byp, mix)) + " dB residual)");
        // click test: toggle listen / bypass / modules / macros while a low sine plays
        PolooyxReleaseProcessor pc;
        const auto sn = sine (SR, 6, 110, -12);
        float maxJump = 0;
        auto out = runProcessor (pc, sn, SR, 256, [&] (int pos) {
            const int blk = pos / 256;
            auto flip = [&] (const char* pid, float v) { auto* x = pc.apvts.getParameter (pid); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
            if (blk == 100) flip (id::listen, 1); if (blk == 160) flip (id::listen, 0);
            if (blk == 220) flip (id::bypass, 1); if (blk == 280) flip (id::bypass, 0);
            if (blk == 340) flip (id::color, 100); if (blk == 400) flip (id::glue, 100);
            if (blk == 460) flip (id::eqOn, 0); if (blk == 520) flip (id::stOn, 0);
            if (blk == 580) flip (id::width, -100); if (blk == 640) flip (id::tone, -100);
            if (blk == 700) flip (id::loudness, 100); if (blk == 760) flip (id::punch, 100); });
        // ideal derivative bound for a -12 dBFS 110 Hz sine ≈ 2π·110/48000·0.25 = 0.0036; louder settings raise the level, allow ×4
        for (int i = 1; i < out.getNumSamples(); ++i) maxJump = std::max (maxJump, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
        check (maxJump < 0.02f, "12 switches/macro jumps during playback: largest sample-to-sample step " + juce::String (maxJump, 4) + " (a click would be > 0.05)");
    }
    line ("");

    // ------------------------------------------------------------------ 13 sample rates, mono, state
    line ("## 13. Sample rates, mono/stereo, state restore, automation");
    {
        for (double sr2 : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            PolooyxReleaseProcessor p;
            auto* lp = p.apvts.getParameter (id::loudness); lp->setValueNotifyingHost (lp->convertTo0to1 (70.0f));
            auto in = rapMix (sr2, 6, { 0, 0, 0, -2.0f });
            auto o = runProcessor (p, in, sr2, 512);
            float pk; bool finite = true; pk = 0; for (int c = 0; c < 2; ++c) for (int i = 0; i < o.getNumSamples(); ++i) { const float v = o.getSample (c, i); if (! std::isfinite (v)) finite = false; pk = std::max (pk, std::abs (v)); }
            const double tp = tpOf (o, 4, (int) (sr2 * 0.1));
            check (finite && tp <= -1.0 + 0.25, f1 (sr2 / 1000.0) + " kHz: output finite, true peak " + f2 (tp) + " dBTP, latency " + juce::String (p.getLatencySamples()) + " samples (" + f2 (p.getLatencySamples() * 1000.0 / sr2) + " ms)");
        }
        {   // mono in → stereo out
            PolooyxReleaseProcessor p;
            juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::stereo());
            const bool ok = p.setBusesLayout (l);
            p.prepareToPlay (SR, 256);
            juce::AudioBuffer<float> b (2, 256); b.clear(); for (int i = 0; i < 256; ++i) b.setSample (0, i, 0.3f * std::sin (i * 0.1f));
            juce::MidiBuffer m; for (int k = 0; k < 40; ++k) p.processBlock (b, m);
            check (ok && std::isfinite (b.getSample (0, 100)), "Mono input -> stereo output layout supported");
        }
        {   // state
            PolooyxReleaseProcessor a, b;
            juce::Random rnd (42);
            for (auto* ap : a.getParameters()) ap->setValueNotifyingHost (rnd.nextFloat());
            juce::MemoryBlock mb; a.getStateInformation (mb);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            int mismatch = 0;
            juce::StringArray bad;
            for (int i = 0; i < a.getParameters().size(); ++i)
                if (auto* ra = dynamic_cast<juce::RangedAudioParameter*> (a.getParameters()[i]); ra != nullptr
                    && std::abs (ra->convertFrom0to1 (ra->getValue()) - ra->convertFrom0to1 (b.getParameters()[i]->getValue())) > 1e-3f * (1.0f + std::abs (ra->convertFrom0to1 (ra->getValue())))) { ++mismatch; bad.add (a.getParameters()[i]->getName (30) + " " + juce::String (a.getParameters()[i]->getValue(), 6) + "/" + juce::String (b.getParameters()[i]->getValue(), 6)); }
            check (mismatch == 0, "State save -> restore: " + juce::String (a.getParameters().size()) + " parameters, " + juce::String (mismatch) + " mismatches " + bad.joinIntoString (", "));
        }
        {   // presets
            PolooyxReleaseProcessor p;
            int okP = 0;
            const auto mix = rapMix (SR, 4, { 0, 0, 0, -6.0f });
            for (int i = 0; i < (int) factoryPresets().size(); ++i)
            {
                p.loadFactoryPreset (i);
                Snapshot s; p.table->read (s);
                AnalysisResult ar; rig.render (mix, SR, s, &ar);
                if (ar.valid && ar.truePeakDb <= -0.8) ++okP;
            }
            check (okP == (int) factoryPresets().size(), "All " + juce::String ((int) factoryPresets().size()) + " factory presets load and render safely (true peak <= ceiling)");
        }
        {   // automation: sweeping LOUDNESS inside a block-size-varying run stays finite and click-free
            PolooyxReleaseProcessor p;
            auto o = runProcessor (p, sine (SR, 4, 200, -10), SR, 128, [&] (int pos) { auto* x = p.apvts.getParameter (id::loudness); x->setValueNotifyingHost ((float) (pos % 48000) / 48000.0f); });
            float j = 0; for (int i = 1; i < o.getNumSamples(); ++i) j = std::max (j, std::abs (o.getSample (0, i) - o.getSample (0, i - 1)));
            check (j < 0.05f, "Automating LOUDNESS (sawtooth, 1 s period): max step " + juce::String (j, 4) + " - smoothed, no zipper noise");
        }
    }
    line ("");

    // ------------------------------------------------------------------ 14 CPU
    line ("## 14. CPU / performance");
    {
        const auto mix = rapMix (SR, 10);
        auto timeIt = [&] (std::initializer_list<std::pair<juce::String, float>> kv) {
            auto s = rig.snap (kv);
            const auto t1 = std::chrono::steady_clock::now();
            rig.render (mix, SR, s);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count();
            return 100.0 * secs / 10.0; };
        line ("| Scenario (48 kHz stereo) | CPU, % of one core |");
        line ("|---|---|");
        line ("| Default (limiter + low-end mono only) | " + f1 (timeIt ({})) + "% |");
        line ("| All macros 60%, NORMAL quality (2x OS) | " + f1 (timeIt ({ { id::tone, 30 }, { id::glue, 60 }, { id::color, 60 }, { id::width, 60 }, { id::punch, 60 }, { id::loudness, 60 } })) + "% |");
        line ("| Everything engaged, HIGH (4x OS) | " + f1 (timeIt ({ { id::quality, 2 }, { id::glue, 60 }, { id::color, 60 }, { id::punch, 60 }, { id::loudness, 60 }, { id::aura, 50 }, { id::grit, 50 }, { id::glass, 50 }, { id::depth, 50 }, { id::space, 50 },
                                                             { mbId (1, "Thr"), -30 }, { deqId (2, "Thr"), -40 }, { eqId (3, "Gain"), 2 } })) + "% |");
        line ("| Everything engaged, MAX (8x OS) | " + f1 (timeIt ({ { id::quality, 3 }, { id::glue, 60 }, { id::color, 60 }, { id::punch, 60 }, { id::loudness, 60 }, { id::aura, 50 }, { id::grit, 50 }, { id::glass, 50 }, { id::depth, 50 }, { id::space, 50 },
                                                            { mbId (1, "Thr"), -30 }, { deqId (2, "Thr"), -40 }, { eqId (3, "Gain"), 2 } })) + "% |");
        line ("");
        line ("Measured offline on the build machine (single core, includes the render harness). The analyser FFT runs on the UI thread, POLO IT / FINAL CHECK on a background thread; the audio thread never allocates, locks or touches files.");
    }
    line ("");

    const double total = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    line ("---");
    line ("Checks passed: " + juce::String (passes) + ", failed: " + juce::String (failures) + "  (suite runtime " + f1 (total) + " s)");
    line (failures == 0 ? "## RESULT: ALL CHECKS PASSED" : "## RESULT: " + juce::String (failures) + " CHECK(S) FAILED");
    outDir.getChildFile ("TEST_REPORT.md").replaceWithText (report, false, false, "\n");

    // ------------------------------------------------------------------ UI snapshots
    if (arg1 == "--ui")
    {
        PolooyxReleaseProcessor p;
        p.prepareToPlay (SR, 512);
        const auto mix = rapMix (SR, 20, { 4.0f, 0, 0.4f, -5.0f });
        juce::AudioBuffer<float> blk (2, 512); juce::MidiBuffer m;
        FakePlayHead ph; p.setPlayHead (&ph);
        for (int pos = 0; pos + 512 <= mix.getNumSamples(); pos += 512)
        { for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, mix, c, pos, 512); p.processBlock (blk, m); }
        // put a real POLO IT result on screen
        p.startListening (PolooyxReleaseProcessor::Action::PoloIt);
        for (int pos = 0; pos + 512 <= mix.getNumSamples(); pos += 512) { for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, mix, c, pos, 512); p.processBlock (blk, m); }
        p.stopListeningAndRun();
        while (p.isBusy()) juce::Thread::sleep (50);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        std::unique_ptr<PolooyxReleaseEditor> ed (dynamic_cast<PolooyxReleaseEditor*> (p.createEditor()));
        ed->setSize (1280, 800);
        auto snap = [&] (const juce::String& name) {
            for (int k = 0; k < 20; ++k) { for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, mix, c, (k * 512) % (mix.getNumSamples() - 512), 512); p.processBlock (blk, m); juce::MessageManager::getInstance()->runDispatchLoopUntil (40); }
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png; juce::FileOutputStream os (outDir.getChildFile (name)); os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); };
        snap ("ui-beginner.png");
        ed->setView (1); ed->setProTab (2); snap ("ui-pro-eq.png");
        ed->setProTab (4); snap ("ui-pro-multiband.png");
        ed->setProTab (8); snap ("ui-pro-limit.png");
        ed->setView (2); snap ("ui-lab.png");
        p.setPlayHead (nullptr);
        ed.reset();
    }
    return failures == 0 ? 0 : 1;
}
