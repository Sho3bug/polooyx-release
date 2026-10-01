#pragma once
// POLOOYX RELEASE ENGINE — recommendation engine: measurements → proposed corrections.
//
// Rules (thresholds documented in docs/DSP_NOTES.md):
//   * Automatic EQ only CUTS, and only where a band stands out from the mix's own tonal line.
//     Dark or bright mixes are never "fixed" with boosts — those become NEXT STEPS instead.
//   * Harsh 2–5 kHz → dynamic EQ (acts only on the loudest ~10 % of moments), not a static cut.
//   * Stereo is never widened; stereo low end is centred; very low correlation narrows ≤ 15 %.
//   * Glue only for genuinely dynamic mixes (high PLR + LRA), parallel, ≤ the mode's mix ceiling.
//   * Color / character only in CHARACTER and LOUD modes, and only a few percent.
//   * Loudness is solved separately (iterative, verified by rendering) in AutoMasterEngine.

#include "CorrectionModel.h"
#include "../DSP/DspCommon.h"
#include "../Analysis/MixDiagnostics.h"
#include <functional>

namespace prl
{
struct RecommendationInputs
{
    const AnalysisResult* a = nullptr;
    PoloMode mode = PoloMode::Balanced;
    float presenceP90Db = -200;    // 90th percentile of the dynamic-EQ presence sidechain level (dB, after input gain)
    float inputGainDb = 0;
    std::function<float (const juce::String&)> current;   // current parameter value (real units)
};

inline std::vector<Correction> recommend (const RecommendationInputs& in)
{
    std::vector<Correction> out;
    if (in.a == nullptr || ! in.a->valid) return out;
    const auto& a = *in.a;
    const auto lim = limitsFor (in.mode);
    auto add = [&] (const juce::String& pid, float v, const char* group, juce::String why)
    { out.push_back ({ pid, v, in.current (pid), group, why }); };

    // ---------------- EQ (cuts only)
    {
        const double dev = a.promLowMid;
        if (dev > 2.0)
        {
            const float g = -std::min (lim.eqMaxCut, (float) (0.6 * (dev - 1.0)) * lim.eqScale);
            add (eqId (2, "Type"), (float) EqType::Bell, "EQ", {});
            add (eqId (2, "Freq"), juce::jlimit (140.0f, 450.0f, (float) a.lowMidPeakHz), "EQ", {});
            add (eqId (2, "Q"), 1.0f, "EQ", {});
            add (eqId (2, "Gain"), g, "EQ", "low-mid +" + juce::String (dev, 1) + " dB around " + juce::String ((int) a.lowMidPeakHz) + " Hz");
        }
        if (a.subRelDb > 14.0 && a.subRelDb > a.bassRelDb + 6.0)
        {
            const float g = -std::min (lim.eqMaxCut, (float) (0.3 * (a.subRelDb - 12.0)) * lim.eqScale);
            add (eqId (0, "Type"), (float) EqType::Bell, "EQ", {});
            add (eqId (0, "Freq"), 40.0f, "EQ", {});
            add (eqId (0, "Q"), 0.9f, "EQ", {});
            add (eqId (0, "Gain"), g, "EQ", "sub " + juce::String (a.subRelDb, 1) + " dB over the 120-500 Hz region");
        }
        if (a.promMid > 2.5)
        {
            const float g = -std::min (lim.eqMaxCut, (float) (0.5 * (a.promMid - 1.0)) * lim.eqScale);
            add (eqId (3, "Type"), (float) EqType::Bell, "EQ", {});
            add (eqId (3, "Freq"), juce::jlimit (500.0f, 1000.0f, (float) a.midPeakHz), "EQ", {});
            add (eqId (3, "Q"), 1.0f, "EQ", {});
            add (eqId (3, "Gain"), g, "EQ", "+" + juce::String (a.promMid, 1) + " dB around " + juce::String ((int) a.midPeakHz) + " Hz");
        }
        if (a.bandDev[8] > 0.5 || a.tiltDbPerOct > 0.0)
        {
            const float g = -std::min (std::min (1.5f, lim.eqMaxCut), (0.6f + 0.3f * (float) std::max (0.0, a.bandDev[8])) * lim.eqScale);
            add (eqId (6, "Type"), (float) EqType::HighShelf, "EQ", {});
            add (eqId (6, "Freq"), 9000.0f, "EQ", {});
            add (eqId (6, "Gain"), g, "EQ", "top end bright (tilt " + juce::String (a.tiltDbPerOct, 1) + " dB/oct)");
        }
    }

    // ---------------- dynamic EQ for harsh presence (intermittent → dynamic, not static)
    if (a.promPresence > 2.0 && in.presenceP90Db > -150.0f)
    {
        const float range = std::min (3.0f, (float) (0.8 * a.promPresence) * lim.eqScale + 0.5f);
        add (deqId (2, "Freq"), juce::jlimit (2000.0f, 5000.0f, (float) a.presencePeakHz), "DYN EQ", {});
        add (deqId (2, "Q"), 1.2f, "DYN EQ", {});
        add (deqId (2, "Thr"), juce::jlimit (-60.0f, 0.0f, in.presenceP90Db - 1.0f), "DYN EQ", {});
        add (deqId (2, "Ratio"), 2.5f, "DYN EQ", {});
        add (deqId (2, "Range"), range, "DYN EQ", "+" + juce::String (a.promPresence, 1) + " dB around " + juce::String ((int) a.presencePeakHz) + " Hz - acts on the loudest moments only");
        add (deqId (2, "Mix"), 100.0f, "DYN EQ", {});
        add (id::deqOn, 1.0f, "DYN EQ", {});
    }

    // ---------------- cleanup: only when the problem is measured
    if (a.dcOffset > 0.001) add (id::dcOn, 1.0f, "CLEANUP", "DC offset measured (" + juce::String (gainToDb ((float) a.dcOffset), 1) + " dBFS)");
    if (a.infraPct > 2.0)
    {
        add (id::subOn, 1.0f, "CLEANUP", juce::String (a.infraPct, 1) + "% of the energy below 20 Hz");
        add (id::subFreq, 18.0f, "CLEANUP", {});
    }

    // ---------------- stereo (never wider)
    if (a.lowSideDb > -12.0)
    {
        add (id::monoBass, 1.0f, "WIDTH", {});
        add (id::monoFreq, a.lowSideDb > -6.0 ? 120.0f : 100.0f, "WIDTH", "stereo content below 120 Hz (" + juce::String (a.lowSideDb, 1) + " dB side/mid)");
    }
    if (a.correlation < 0.2)
    {
        const float cur = in.current (id::stWidth);
        const float k = std::min (HardLimits::stereoMaxChange, (float) (0.2 - a.correlation) * 0.5f);
        add (id::stWidth, cur * (1.0f - k), "WIDTH", "low correlation (" + juce::String (a.correlation, 2) + ")");
    }

    // ---------------- glue (only for genuinely dynamic material)
    if (a.plr > 12.0 && a.lra > 5.0 && lim.glueMaxMix > 0.0f)
    {
        const float mix = lim.glueMaxMix * clamp01 ((float) (a.plr - 11.0) / 6.0f);
        add (id::glueThr, juce::jlimit (-40.0f, 0.0f, (float) a.lufsI + in.inputGainDb + 3.0f), "GLUE", {});
        add (id::glueRatio, 2.0f, "GLUE", {});
        add (id::glueAtt, 30.0f, "GLUE", {});
        add (id::glueRel, 200.0f, "GLUE", {});
        add (id::glueMakeup, 0.0f, "GLUE", {});
        add (id::glueMix, 100.0f * mix, "GLUE", "dynamic mix (PLR " + juce::String (a.plr, 1) + " dB, LRA " + juce::String (a.lra, 1) + " LU)");
        add (id::glueOn, 1.0f, "GLUE", {});
    }

    // ---------------- color / character (CHARACTER, LOUD only)
    if (lim.colorMix > 0.0f)
    {
        add (id::colorMode, (float) ColorMode::Tape, "COLOR", {});
        add (id::colorDrive, 30.0f, "COLOR", {});
        add (id::colorMix, 100.0f * lim.colorMix, "COLOR", in.mode == PoloMode::Character ? "CHARACTER mode: POLOOYX harmonic identity" : "LOUD mode: harmonic density");
        add (id::colorOn, 1.0f, "COLOR", {});
    }
    if (lim.characterAmt > 0.0f)
    {
        add (id::aura, 100.0f * lim.characterAmt, "CHARACTER", "CHARACTER mode");
        add (id::grit, 80.0f * lim.characterAmt, "CHARACTER", {});
        add (id::charOn, 1.0f, "CHARACTER", {});
    }
    return out;
}
} // namespace prl
