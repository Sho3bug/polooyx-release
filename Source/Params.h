#pragma once
// POLOOYX RELEASE ENGINE — parameter IDs, ranges and layout.
// Every parameter here is automatable; IDs are stable (presets and DAW projects depend on them).

#include <juce_audio_processors/juce_audio_processors.h>

namespace prl
{
namespace id
{
// global / monitoring
inline constexpr auto inGain = "inGain", outGain = "outGain", dryWet = "dryWet", bypass = "bypass";
inline constexpr auto listen = "listen", levelMatch = "levelMatch", monitor = "monitor";
inline constexpr auto quality = "quality", oversampling = "oversampling";
inline constexpr auto target = "target", customTarget = "customTarget", poloMode = "poloMode";
// macros
inline constexpr auto tone = "tone", glue = "glue", color = "color", width = "width", punch = "punch", loudness = "loudness";
// cleanup
inline constexpr auto dcOn = "dcOn", subOn = "subOn", subFreq = "subFreq";
// master eq
inline constexpr auto eqOn = "eqOn";
// dynamic eq / multiband / glue
inline constexpr auto deqOn = "deqOn";
inline constexpr auto mbOn = "mbOn", mbAutoGain = "mbAutoGain";
inline constexpr auto glueOn = "glueOn", glueThr = "glueThr", glueRatio = "glueRatio", glueAtt = "glueAtt", glueRel = "glueRel",
                      glueKnee = "glueKnee", glueMakeup = "glueMakeup", glueMix = "glueMix";
// punch
inline constexpr auto punchOn = "punchOn", punchTrans = "punchTrans", punchBody = "punchBody", punchImpact = "punchImpact", punchMix = "punchMix";
// color
inline constexpr auto colorOn = "colorOn", colorMode = "colorMode", colorDrive = "colorDrive", colorTone = "colorTone",
                      colorMix = "colorMix", colorOut = "colorOut";
// stereo
inline constexpr auto stOn = "stOn", stWidth = "stWidth", stLowWidth = "stLowWidth", stHighWidth = "stHighWidth",
                      stXoLow = "stXoLow", stXoHigh = "stXoHigh", stMid = "stMid", stSide = "stSide",
                      monoBass = "monoBass", monoFreq = "monoFreq";
// character
inline constexpr auto charOn = "charOn", aura = "aura", grit = "grit", glass = "glass", depth = "depth", space = "space";
// clipper / limiter
inline constexpr auto clipOn = "clipOn", clipMode = "clipMode", clipDrive = "clipDrive", clipCeiling = "clipCeiling", clipMix = "clipMix";
inline constexpr auto limOn = "limOn", limStyle = "limStyle", limThresh = "limThresh", limCeiling = "limCeiling",
                      limRelease = "limRelease", limLookahead = "limLookahead", limMix = "limMix";
} // namespace id

// ---- indexed IDs (EQ / dynamic EQ / multiband bands)
inline juce::String eqId (int band, const char* what)  { return "eq" + juce::String (band + 1) + what; }    // Type Freq Gain Q
inline juce::String deqId (int band, const char* what) { return "deq" + juce::String (band + 1) + what; }   // Freq Q Thr Ratio Att Rel Range Mix
inline juce::String mbId (int band, const char* what)  { return "mb" + juce::String (band + 1) + what; }    // Thr Ratio Att Rel Gain Mix
inline juce::String xoId (int i)                       { return "xo" + juce::String (i + 1); }

constexpr int kEqBands = 8, kDeqBands = 4, kMbBands = 5;

inline const char* eqBandName (int b)  { static const char* n[] = { "SUB", "LOW", "LOW-MID", "MID", "UPPER-MID", "PRESENCE", "HIGH", "AIR" }; return n[b]; }
inline const char* deqBandName (int b) { static const char* n[] = { "LOW-MID", "MID", "PRESENCE", "HIGH" }; return n[b]; }
inline const char* mbBandName (int b)  { static const char* n[] = { "SUB", "LOW", "LOW-MID", "HIGH-MID", "HIGH" }; return n[b]; }

enum class EqType { Bell = 0, LowShelf, HighShelf, HighPass, LowPass };
enum class ColorMode { Clean = 0, Tape, Tube, Console, Digital };
enum class ClipMode { Soft = 0, Tape, Hard };
enum class LimStyle { Transparent = 0, Punch, Dense, Modern };
enum class Monitor { Full = 0, Mono, SmallSpeaker };
enum class Target { Premaster = 0, Streaming, Loud, Custom };
enum class PoloMode { Transparent = 0, Balanced, Character, Loud, Safe };

inline juce::StringArray eqTypeNames()   { return { "BELL", "LOW SHELF", "HIGH SHELF", "HIGH-PASS", "LOW-PASS" }; }
inline juce::StringArray colorModeNames(){ return { "CLEAN", "TAPE", "TUBE", "CONSOLE", "DIGITAL" }; }
inline juce::StringArray clipModeNames() { return { "SOFT", "TAPE", "HARD" }; }
inline juce::StringArray limStyleNames() { return { "TRANSPARENT", "PUNCH", "DENSE", "MODERN" }; }
inline juce::StringArray monitorNames()  { return { "FULL", "MONO", "SMALL SPEAKER" }; }
inline juce::StringArray targetNames()   { return { "MIX / PREMASTER", "STREAMING", "LOUD", "CUSTOM" }; }
inline juce::StringArray poloModeNames() { return { "TRANSPARENT", "BALANCED", "CHARACTER", "LOUD", "SAFE" }; }
inline juce::StringArray qualityNames()  { return { "ECO", "NORMAL", "HIGH", "MAX" }; }
inline juce::StringArray osNames()       { return { "AUTO", "OFF", "2X", "4X", "8X" }; }
inline juce::StringArray listenNames()   { return { "PROCESSED", "ORIGINAL" }; }

// target loudness for each Target choice (LUFS-I). Premaster = "do not push": POLO IT never raises a mix above it.
inline float targetLufs (Target t, float custom)
{
    switch (t)
    {
        case Target::Premaster: return -18.0f;
        case Target::Streaming: return -14.0f;
        case Target::Loud:      return -9.0f;
        case Target::Custom:    return custom;
    }
    return -14.0f;
}

struct EqDefault { EqType type; float freq, q; };
inline EqDefault eqDefault (int b)
{
    static const EqDefault d[] = { { EqType::Bell, 45, 0.8f },  { EqType::LowShelf, 110, 0.7f }, { EqType::Bell, 250, 1.0f },
                                   { EqType::Bell, 700, 1.0f }, { EqType::Bell, 1800, 1.0f },    { EqType::Bell, 4000, 1.0f },
                                   { EqType::HighShelf, 9000, 0.7f }, { EqType::HighShelf, 16000, 0.7f } };
    return d[b];
}
inline float deqDefaultFreq (int b) { static const float f[] = { 300, 900, 3500, 8000 }; return f[b]; }
inline float xoDefault (int i)      { static const float f[] = { 90, 300, 2500, 8000 }; return f[i]; }
inline juce::NormalisableRange<float> xoRange (int i)
{
    static const float lo[] = { 40, 150, 800, 4000 }, hi[] = { 200, 800, 5000, 14000 };
    juce::NormalisableRange<float> r (lo[i], hi[i]); r.setSkewForCentre (std::sqrt (lo[i] * hi[i])); return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat; using B = AudioParameterBool; using C = AudioParameterChoice;
    AudioProcessorValueTreeState::ParameterLayout l;
    const int v = 1;

    auto pct  = [] (float x, int) { return String (roundToInt (x)) + "%"; };
    auto spct = [] (float x, int) { const int r = roundToInt (x); return (r > 0 ? "+" : "") + String (r) + "%"; };
    auto db   = [] (float x, int) { return (x > 0.049f ? "+" : "") + String (x, 1) + " dB"; };
    auto hz   = [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, x >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (x)) + " Hz"; };
    auto ms   = [] (float x, int) { return String (x, x < 10 ? 1 : 0) + " ms"; };
    auto ratio= [] (float x, int) { return String (x, 1) + ":1"; };
    auto num  = [] (float x, int) { return String (x, 2); };
    auto lufs = [] (float x, int) { return String (x, 1) + " LUFS"; };

    auto fl = [&] (const String& pid, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt)
    { l.add (std::make_unique<F> (ParameterID { pid, v }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (fmt))); };
    auto bl = [&] (const String& pid, const String& name, bool def) { l.add (std::make_unique<B> (ParameterID { pid, v }, name, def)); };
    auto ch = [&] (const String& pid, const String& name, StringArray items, int def) { l.add (std::make_unique<C> (ParameterID { pid, v }, name, items, def)); };

    auto lin  = [] (float lo, float hi, float step = 0.0f) { return NormalisableRange<float> (lo, hi, step); };
    auto freq = [] (float lo, float hi) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (std::sqrt (lo * hi)); return r; };
    auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    // ---------------- global / monitoring
    fl (id::inGain,  "Input Gain",  lin (-24, 24, 0.1f), 0.0f, db);
    fl (id::outGain, "Output Gain", lin (-24, 12, 0.1f), 0.0f, db);
    fl (id::dryWet,  "Dry/Wet",     lin (0, 100, 0.1f), 100.0f, pct);
    bl (id::bypass,  "Bypass", false);
    ch (id::listen,  "Listen", listenNames(), 0);
    bl (id::levelMatch, "Level Match", true);
    ch (id::monitor, "Monitor", monitorNames(), 0);
    ch (id::quality, "Quality", qualityNames(), 1);
    ch (id::oversampling, "Oversampling", osNames(), 0);
    ch (id::target,  "Loudness Target", targetNames(), 1);
    fl (id::customTarget, "Custom Target", lin (-24, -5, 0.1f), -12.0f, lufs);
    ch (id::poloMode, "POLO IT Mode", poloModeNames(), 1);

    // ---------------- macros
    fl (id::tone,     "TONE",     lin (-100, 100, 0.1f), 0.0f, spct);
    fl (id::glue,     "GLUE",     lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::color,    "COLOR",    lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::width,    "WIDTH",    lin (-100, 100, 0.1f), 0.0f, spct);
    fl (id::punch,    "PUNCH",    lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::loudness, "LOUDNESS", lin (0, 100, 0.1f), 0.0f, pct);

    // ---------------- cleanup
    bl (id::dcOn,  "DC Filter", false);
    bl (id::subOn, "Subsonic Filter", false);
    fl (id::subFreq, "Subsonic Freq", lin (10, 40, 0.1f), 18.0f, hz);

    // ---------------- master EQ
    bl (id::eqOn, "EQ On", true);
    for (int b = 0; b < kEqBands; ++b)
    {
        const auto d = eqDefault (b);
        const String n = String ("EQ ") + eqBandName (b) + " ";
        ch (eqId (b, "Type"), n + "Type", eqTypeNames(), (int) d.type);
        fl (eqId (b, "Freq"), n + "Freq", freq (20, 20000), d.freq, hz);
        fl (eqId (b, "Gain"), n + "Gain", lin (-12, 12, 0.05f), 0.0f, db);
        fl (eqId (b, "Q"),    n + "Q",    skew (0.2f, 6.0f, 1.0f), d.q, num);
    }

    // ---------------- dynamic EQ (downward, cut only)
    bl (id::deqOn, "Dynamic EQ On", true);
    for (int b = 0; b < kDeqBands; ++b)
    {
        const String n = String ("DynEQ ") + deqBandName (b) + " ";
        fl (deqId (b, "Freq"),  n + "Freq",      freq (60, 16000), deqDefaultFreq (b), hz);
        fl (deqId (b, "Q"),     n + "Q",         skew (0.3f, 6.0f, 1.2f), 1.2f, num);
        fl (deqId (b, "Thr"),   n + "Threshold", lin (-60, 0, 0.1f), 0.0f, db);
        fl (deqId (b, "Ratio"), n + "Ratio",     skew (1, 10, 3), 2.0f, ratio);
        fl (deqId (b, "Att"),   n + "Attack",    skew (0.5f, 100, 10), 5.0f, ms);
        fl (deqId (b, "Rel"),   n + "Release",   skew (10, 1000, 150), 120.0f, ms);
        fl (deqId (b, "Range"), n + "Range",     lin (0, 12, 0.1f), 3.0f, db);
        fl (deqId (b, "Mix"),   n + "Mix",       lin (0, 100, 0.1f), 100.0f, pct);
    }

    // ---------------- multiband dynamics
    bl (id::mbOn, "Multiband On", true);
    for (int i = 0; i < kMbBands - 1; ++i)
        fl (xoId (i), "Crossover " + String (i + 1), xoRange (i), xoDefault (i), hz);
    for (int b = 0; b < kMbBands; ++b)
    {
        const String n = String ("MB ") + mbBandName (b) + " ";
        fl (mbId (b, "Thr"),   n + "Threshold", lin (-60, 0, 0.1f), 0.0f, db);
        fl (mbId (b, "Ratio"), n + "Ratio",     skew (1, 10, 3), 2.0f, ratio);
        fl (mbId (b, "Att"),   n + "Attack",    skew (0.1f, 200, 20), 20.0f, ms);
        fl (mbId (b, "Rel"),   n + "Release",   skew (10, 1000, 150), 150.0f, ms);
        fl (mbId (b, "Gain"),  n + "Makeup",    lin (-12, 12, 0.1f), 0.0f, db);
        fl (mbId (b, "Mix"),   n + "Mix",       lin (0, 100, 0.1f), 100.0f, pct);
    }
    bl (id::mbAutoGain, "Multiband Auto Gain", false);

    // ---------------- glue compressor
    bl (id::glueOn, "Glue On", true);
    fl (id::glueThr,    "Glue Threshold", lin (-40, 0, 0.1f), 0.0f, db);
    fl (id::glueRatio,  "Glue Ratio",     skew (1, 10, 3), 2.0f, ratio);
    fl (id::glueAtt,    "Glue Attack",    skew (0.1f, 100, 10), 30.0f, ms);
    fl (id::glueRel,    "Glue Release",   skew (20, 1200, 200), 200.0f, ms);
    fl (id::glueKnee,   "Glue Knee",      lin (0, 12, 0.1f), 6.0f, db);
    fl (id::glueMakeup, "Glue Makeup",    lin (0, 12, 0.1f), 0.0f, db);
    fl (id::glueMix,    "Glue Mix",       lin (0, 100, 0.1f), 100.0f, pct);

    // ---------------- punch
    bl (id::punchOn, "Punch On", true);
    fl (id::punchTrans,  "Punch Transient", lin (-100, 100, 0.1f), 0.0f, spct);
    fl (id::punchBody,   "Punch Body",      lin (-100, 100, 0.1f), 0.0f, spct);
    fl (id::punchImpact, "Punch Impact",    lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::punchMix,    "Punch Mix",       lin (0, 100, 0.1f), 100.0f, pct);

    // ---------------- harmonic color
    bl (id::colorOn, "Color On", true);
    ch (id::colorMode,  "Color Mode", colorModeNames(), 1);
    fl (id::colorDrive, "Color Drive", lin (0, 100, 0.1f), 30.0f, pct);
    fl (id::colorTone,  "Color Tone",  lin (-100, 100, 0.1f), 0.0f, spct);
    fl (id::colorMix,   "Color Mix",   lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::colorOut,   "Color Output", lin (-12, 12, 0.1f), 0.0f, db);

    // ---------------- stereo / M-S
    bl (id::stOn, "Stereo On", true);
    fl (id::stWidth,     "Width",           lin (0, 200, 0.1f), 100.0f, pct);
    fl (id::stLowWidth,  "Low Width",       lin (0, 200, 0.1f), 100.0f, pct);
    fl (id::stHighWidth, "High Width",      lin (0, 200, 0.1f), 100.0f, pct);
    fl (id::stXoLow,     "Low Width Xover", freq (100, 800), 250.0f, hz);
    fl (id::stXoHigh,    "High Width Xover", freq (1500, 12000), 5000.0f, hz);
    fl (id::stMid,       "Mid Level",       lin (-6, 6, 0.05f), 0.0f, db);
    fl (id::stSide,      "Side Level",      lin (-6, 6, 0.05f), 0.0f, db);
    bl (id::monoBass,    "Low Frequency Mono", false);
    fl (id::monoFreq,    "Mono Below",      freq (40, 300), 80.0f, hz);

    // ---------------- POLOOYX character
    bl (id::charOn, "Character On", true);
    fl (id::aura,  "Aura",  lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::grit,  "Grit",  lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::glass, "Glass", lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::depth, "Depth", lin (0, 100, 0.1f), 0.0f, pct);
    fl (id::space, "Space", lin (0, 100, 0.1f), 0.0f, pct);

    // ---------------- soft clipper
    bl (id::clipOn, "Clipper On", true);
    ch (id::clipMode,    "Clipper Mode", clipModeNames(), 0);
    fl (id::clipDrive,   "Clipper Drive",   lin (0, 12, 0.05f), 0.0f, db);
    fl (id::clipCeiling, "Clipper Ceiling", lin (-6, 0, 0.05f), -0.3f, db);
    fl (id::clipMix,     "Clipper Mix",     lin (0, 100, 0.1f), 100.0f, pct);

    // ---------------- true-peak limiter
    bl (id::limOn, "Limiter On", true);
    ch (id::limStyle,     "Limiter Style", limStyleNames(), 0);
    fl (id::limThresh,    "Limiter Threshold", lin (-24, 0, 0.05f), 0.0f, db);
    fl (id::limCeiling,   "Limiter Ceiling",   lin (-6, 0, 0.05f), -1.0f, [] (float x, int) { return String (x, 1) + " dBTP"; });
    fl (id::limRelease,   "Limiter Release",   skew (1, 1000, 100), 120.0f, ms);
    fl (id::limLookahead, "Limiter Lookahead", lin (0.5f, 10, 0.1f), 5.0f, ms);
    fl (id::limMix,       "Limiter Mix",       lin (0, 100, 0.1f), 100.0f, pct);
    return l;
}
} // namespace prl
