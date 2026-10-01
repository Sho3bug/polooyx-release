#pragma once
// POLOOYX RELEASE ENGINE — parameter snapshot → resolved DSP settings (incl. the six macros).
//
// A Snapshot is a plain vector of every parameter's real value, in layout order. It is taken from
// the APVTS (and smoothed) on the audio thread, or copied on the message thread for POLO IT's
// offline renders — both go through the same resolve() so live audio and verification renders are
// identical.
//
// Macro network (on top of the module settings, never replacing them):
//   TONE     ±2 dB see-saw tilt (darker ↔ brighter) — never lifts both ends
//   GLUE     glue threshold −14 dB, ratio +2, makeup +3 dB at 100 %
//   COLOR    color mix +60 %, drive +35 %
//   WIDTH    + : mid/high-band side gain up to +35/45 %, scaled down automatically when the measured
//                input correlation is low (protects mono); low band is never widened
//            − : narrows every band down to 50 %
//   PUNCH    transient +60 %, impact +50 %, body +15 %
//   LOUDNESS drives the clipper (+3 dB, engages it) and the limiter (+9 dB into it); the ceiling stays
//            put, so loudness rises only through peak control — never through output gain.

#include "../Params.h"
#include "GainStage.h"
#include "MasterEQ.h"
#include "DynamicEQ.h"
#include "Multiband.h"
#include "GlueCompressor.h"
#include "Punch.h"
#include "Saturation.h"
#include "StereoEngine.h"
#include "Character.h"
#include "Clipper.h"
#include "TruePeakLimiter.h"

namespace prl
{
enum class PKind { Float, Bool, Choice };

// parameter indices into a Snapshot (built once from the layout)
struct Ix
{
    int inGain, outGain, dryWet, bypass, listen, levelMatch, monitor, quality, oversampling, target, customTarget, poloMode;
    int tone, glue, color, width, punch, loudness;
    int dcOn, subOn, subFreq;
    int eqOn; std::array<int, kEqBands> eqType, eqFreq, eqGain, eqQ;
    int deqOn; std::array<int, kDeqBands> deqFreq, deqQ, deqThr, deqRatio, deqAtt, deqRel, deqRange, deqMix;
    int mbOn, mbAutoGain; std::array<int, kMbBands - 1> xo; std::array<int, kMbBands> mbThr, mbRatio, mbAtt, mbRel, mbGain, mbMix;
    int glueOn, glueThr, glueRatio, glueAtt, glueRel, glueKnee, glueMakeup, glueMix;
    int punchOn, punchTrans, punchBody, punchImpact, punchMix;
    int colorOn, colorMode, colorDrive, colorTone, colorMix, colorOut;
    int stOn, stWidth, stLowWidth, stHighWidth, stXoLow, stXoHigh, stMid, stSide, monoBass, monoFreq;
    int charOn, aura, grit, glass, depth, space;
    int clipOn, clipMode, clipDrive, clipCeiling, clipMix;
    int limOn, limStyle, limThresh, limCeiling, limRelease, limLookahead, limMix;
};

class ParamTable
{
public:
    explicit ParamTable (const juce::AudioProcessorValueTreeState& s)
    {
        auto params = s.processor.getParameters();
        for (auto* p : params)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                ids.add (rp->getParameterID());
                kinds.push_back (dynamic_cast<juce::AudioParameterBool*> (rp) ? PKind::Bool
                               : dynamic_cast<juce::AudioParameterChoice*> (rp) ? PKind::Choice : PKind::Float);
                raw.push_back (s.getRawParameterValue (rp->getParameterID()));
                ranged.push_back (rp);
            }
        auto f = [this] (const juce::String& pid) { const int i = ids.indexOf (pid); jassert (i >= 0); return i; };
        using namespace id;
        ix.inGain = f (inGain); ix.outGain = f (outGain); ix.dryWet = f (dryWet); ix.bypass = f (bypass); ix.listen = f (listen);
        ix.levelMatch = f (levelMatch); ix.monitor = f (monitor); ix.quality = f (quality); ix.oversampling = f (oversampling);
        ix.target = f (target); ix.customTarget = f (customTarget); ix.poloMode = f (poloMode);
        ix.tone = f (tone); ix.glue = f (glue); ix.color = f (color); ix.width = f (width); ix.punch = f (punch); ix.loudness = f (loudness);
        ix.dcOn = f (dcOn); ix.subOn = f (subOn); ix.subFreq = f (subFreq);
        ix.eqOn = f (eqOn);
        for (int b = 0; b < kEqBands; ++b) { ix.eqType[(size_t) b] = f (eqId (b, "Type")); ix.eqFreq[(size_t) b] = f (eqId (b, "Freq")); ix.eqGain[(size_t) b] = f (eqId (b, "Gain")); ix.eqQ[(size_t) b] = f (eqId (b, "Q")); }
        ix.deqOn = f (deqOn);
        for (int b = 0; b < kDeqBands; ++b)
        {
            ix.deqFreq[(size_t) b] = f (deqId (b, "Freq")); ix.deqQ[(size_t) b] = f (deqId (b, "Q")); ix.deqThr[(size_t) b] = f (deqId (b, "Thr"));
            ix.deqRatio[(size_t) b] = f (deqId (b, "Ratio")); ix.deqAtt[(size_t) b] = f (deqId (b, "Att")); ix.deqRel[(size_t) b] = f (deqId (b, "Rel"));
            ix.deqRange[(size_t) b] = f (deqId (b, "Range")); ix.deqMix[(size_t) b] = f (deqId (b, "Mix"));
        }
        ix.mbOn = f (mbOn); ix.mbAutoGain = f (mbAutoGain);
        for (int i = 0; i < kMbBands - 1; ++i) ix.xo[(size_t) i] = f (xoId (i));
        for (int b = 0; b < kMbBands; ++b)
        {
            ix.mbThr[(size_t) b] = f (mbId (b, "Thr")); ix.mbRatio[(size_t) b] = f (mbId (b, "Ratio")); ix.mbAtt[(size_t) b] = f (mbId (b, "Att"));
            ix.mbRel[(size_t) b] = f (mbId (b, "Rel")); ix.mbGain[(size_t) b] = f (mbId (b, "Gain")); ix.mbMix[(size_t) b] = f (mbId (b, "Mix"));
        }
        ix.glueOn = f (glueOn); ix.glueThr = f (glueThr); ix.glueRatio = f (glueRatio); ix.glueAtt = f (glueAtt); ix.glueRel = f (glueRel);
        ix.glueKnee = f (glueKnee); ix.glueMakeup = f (glueMakeup); ix.glueMix = f (glueMix);
        ix.punchOn = f (punchOn); ix.punchTrans = f (punchTrans); ix.punchBody = f (punchBody); ix.punchImpact = f (punchImpact); ix.punchMix = f (punchMix);
        ix.colorOn = f (colorOn); ix.colorMode = f (colorMode); ix.colorDrive = f (colorDrive); ix.colorTone = f (colorTone); ix.colorMix = f (colorMix); ix.colorOut = f (colorOut);
        ix.stOn = f (stOn); ix.stWidth = f (stWidth); ix.stLowWidth = f (stLowWidth); ix.stHighWidth = f (stHighWidth); ix.stXoLow = f (stXoLow);
        ix.stXoHigh = f (stXoHigh); ix.stMid = f (stMid); ix.stSide = f (stSide); ix.monoBass = f (monoBass); ix.monoFreq = f (monoFreq);
        ix.charOn = f (charOn); ix.aura = f (aura); ix.grit = f (grit); ix.glass = f (glass); ix.depth = f (depth); ix.space = f (space);
        ix.clipOn = f (clipOn); ix.clipMode = f (clipMode); ix.clipDrive = f (clipDrive); ix.clipCeiling = f (clipCeiling); ix.clipMix = f (clipMix);
        ix.limOn = f (limOn); ix.limStyle = f (limStyle); ix.limThresh = f (limThresh); ix.limCeiling = f (limCeiling);
        ix.limRelease = f (limRelease); ix.limLookahead = f (limLookahead); ix.limMix = f (limMix);
    }

    int size() const noexcept { return (int) raw.size(); }
    // current values (real units) — safe on any thread (atomics)
    void read (std::vector<float>& out) const
    {
        out.resize (raw.size());
        for (size_t i = 0; i < raw.size(); ++i) out[i] = raw[i]->load (std::memory_order_relaxed);
    }
    // smooth continuous values / switches; snap choices
    void smooth (std::vector<float>& sm, const std::vector<float>& target, float a) const noexcept
    {
        for (size_t i = 0; i < sm.size(); ++i)
        {
            if (kinds[i] == PKind::Choice) { sm[i] = target[i]; continue; }
            sm[i] = target[i] + a * (sm[i] - target[i]);
            if (std::abs (sm[i] - target[i]) < 1.0e-5f * (1.0f + std::abs (target[i]))) sm[i] = target[i];
        }
    }

    Ix ix {};
    juce::StringArray ids;
    std::vector<PKind> kinds;
    std::vector<std::atomic<float>*> raw;
    std::vector<juce::RangedAudioParameter*> ranged;
};

using Snapshot = std::vector<float>;

struct ChainSettings
{
    GainStage::Settings gain;
    MasterEQ::Settings eq;
    DynamicEQ::Settings deq;
    Multiband::Settings mb;
    GlueCompressor::Settings glue;
    Punch::Settings punch;
    Saturation::Settings color;
    StereoEngine::Settings stereo;
    Character::Settings character;
    Clipper::Settings clip;
    TruePeakLimiter::Settings lim;
    float outGainDb = 0, dryWet = 1;
    int osFactor = 2;
};

inline int osFactorFor (int osChoice, int quality, double sr) noexcept
{
    if (osChoice >= 1) return osChoice == 1 ? 1 : osChoice == 2 ? 2 : osChoice == 3 ? 4 : 8;   // explicit
    int f = quality == 0 ? 1 : quality == 1 ? 2 : quality == 2 ? 4 : 8;                       // AUTO from quality
    if (sr >= 176000.0) f = std::max (1, f / 4); else if (sr >= 88000.0) f = std::max (1, f / 2);
    return f;
}

// liveCorrelation: measured input correlation (−1..1) used by the WIDTH macro's mono protection
inline ChainSettings resolve (const Snapshot& v, const Ix& ix, double sr, float liveCorrelation = 1.0f)
{
    ChainSettings c;
    auto b = [&] (int i) { return v[(size_t) i]; };          // switches are smoothed 0..1
    auto ci = [&] (int i) { return (int) std::lround (v[(size_t) i]); };
    const float tone = b (ix.tone) * 0.01f, glue = b (ix.glue) * 0.01f, color = b (ix.color) * 0.01f;
    const float width = b (ix.width) * 0.01f, punch = b (ix.punch) * 0.01f, loud = b (ix.loudness) * 0.01f;

    c.gain = { b (ix.inGain), b (ix.dcOn), b (ix.subOn), b (ix.subFreq) };

    c.eq.amount = b (ix.eqOn);
    for (int k = 0; k < kEqBands; ++k)
        c.eq.bands[(size_t) k] = { (EqType) ci (ix.eqType[(size_t) k]), b (ix.eqFreq[(size_t) k]), b (ix.eqGain[(size_t) k]), b (ix.eqQ[(size_t) k]) };
    c.eq.tiltDb = 2.0f * tone;
    if (c.eq.amount <= 0.0f && std::abs (tone) > 0.0f) c.eq.amount = 1.0f;   // TONE works even with the band EQ off

    c.deq.amount = b (ix.deqOn);
    for (int k = 0; k < kDeqBands; ++k)
        c.deq.bands[(size_t) k] = { b (ix.deqFreq[(size_t) k]), b (ix.deqQ[(size_t) k]), b (ix.deqThr[(size_t) k]), b (ix.deqRatio[(size_t) k]),
                                    b (ix.deqAtt[(size_t) k]), b (ix.deqRel[(size_t) k]), b (ix.deqRange[(size_t) k]), b (ix.deqMix[(size_t) k]) * 0.01f };

    c.mb.autoGain = b (ix.mbAutoGain) > 0.5f;
    for (int k = 0; k < kMbBands - 1; ++k) c.mb.xo[(size_t) k] = b (ix.xo[(size_t) k]);
    for (int k = 1; k < kMbBands - 1; ++k) c.mb.xo[(size_t) k] = std::max (c.mb.xo[(size_t) k], c.mb.xo[(size_t) k - 1] * 1.25f);
    for (int k = 0; k < kMbBands; ++k)
        c.mb.bands[(size_t) k] = { b (ix.mbThr[(size_t) k]), b (ix.mbRatio[(size_t) k]), b (ix.mbAtt[(size_t) k]), b (ix.mbRel[(size_t) k]),
                                   b (ix.mbGain[(size_t) k]), b (ix.mbMix[(size_t) k]) * 0.01f };
    c.mb.amount = Multiband::engaged (c.mb) ? b (ix.mbOn) : 0.0f;   // idle multiband = exact bypass (no crossover phase)

    c.glue = { b (ix.glueThr) - 14.0f * glue, b (ix.glueRatio) + 2.0f * glue, b (ix.glueAtt), b (ix.glueRel), b (ix.glueKnee),
               b (ix.glueMakeup) + 3.0f * glue, b (ix.glueMix) * 0.01f, b (ix.glueOn) };

    c.punch = { clampf (b (ix.punchTrans) * 0.01f + 0.6f * punch, -1.0f, 1.0f), clampf (b (ix.punchBody) * 0.01f + 0.15f * punch, -1.0f, 1.0f),
                clamp01 (b (ix.punchImpact) * 0.01f + 0.5f * punch), b (ix.punchMix) * 0.01f, b (ix.punchOn) };

    c.color = { (ColorMode) ci (ix.colorMode), clamp01 (b (ix.colorDrive) * 0.01f + 0.35f * color), clampf (b (ix.colorTone) * 0.01f, -1.0f, 1.0f),
                clamp01 (b (ix.colorMix) * 0.01f + 0.6f * color), b (ix.colorOut), b (ix.colorOn) };

    {
        const float guard = clamp01 ((liveCorrelation - 0.1f) / 0.4f);   // low correlation → no extra widening
        float wMul = 1.0f, hMul = 1.0f, lMul = 1.0f;
        if (width > 0.0f) { wMul = 1.0f + 0.35f * width * guard; hMul = 1.0f + 0.45f * width * guard; }
        else if (width < 0.0f) { wMul = hMul = lMul = 1.0f + 0.5f * width; }
        c.stereo.width = b (ix.stWidth) * 0.01f * wMul;
        c.stereo.lowWidth = b (ix.stLowWidth) * 0.01f * lMul;
        c.stereo.highWidth = b (ix.stHighWidth) * 0.01f * hMul;
        c.stereo.xoLow = b (ix.stXoLow); c.stereo.xoHigh = b (ix.stXoHigh);
        c.stereo.midDb = b (ix.stMid); c.stereo.sideDb = b (ix.stSide);
        c.stereo.monoBass = b (ix.monoBass); c.stereo.monoHz = b (ix.monoFreq);
        c.stereo.amount = b (ix.stOn);
    }

    c.character = { b (ix.aura) * 0.01f, b (ix.grit) * 0.01f, b (ix.glass) * 0.01f, b (ix.depth) * 0.01f, b (ix.space) * 0.01f, b (ix.charOn) };

    {
        const float drive = b (ix.clipDrive) + 3.0f * loud;
        c.clip = { (ClipMode) ci (ix.clipMode), drive, b (ix.clipCeiling), b (ix.clipMix) * 0.01f,
                   b (ix.clipOn) * clamp01 (drive / 0.25f) };   // DRIVE 0 dB = clipper idle (no shaping at all)
    }

    c.lim = { (LimStyle) ci (ix.limStyle), b (ix.limThresh) - 9.0f * loud, b (ix.limCeiling), b (ix.limRelease), b (ix.limLookahead),
              b (ix.limMix) * 0.01f, b (ix.limOn) };

    c.outGainDb = b (ix.outGain);
    c.dryWet = b (ix.dryWet) * 0.01f;
    c.osFactor = osFactorFor (ci (ix.oversampling), ci (ix.quality), sr);
    return c;
}
} // namespace prl
