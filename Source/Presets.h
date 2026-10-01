#pragma once
// POLOOYX RELEASE ENGINE — factory presets.
// Applying a factory preset resets every DSP parameter to its default first, then sets the listed
// values. Session settings (bypass, listen, level match, monitor, quality, oversampling, loudness
// target, POLO IT mode) are left alone. All presets are deliberately restrained.

#include "Params.h"
#include <vector>
#include <utility>

namespace prl
{
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<juce::String, float>> values;
};

inline bool isSessionParam (const juce::String& pid)
{
    return pid == id::bypass || pid == id::listen || pid == id::levelMatch || pid == id::monitor || pid == id::quality
        || pid == id::oversampling || pid == id::target || pid == id::customTarget || pid == id::poloMode;
}

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace id;
    static const std::vector<FactoryPreset> p = {
        // ---------------------------------------------------------------- CORE
        { "POLOOYX DEFAULT", "CORE", {} },
        { "POLOOYX CLEAN", "CORE", { { glue, 8 }, { limLookahead, 8 }, { limRelease, 180 } } },
        { "POLOOYX BALANCED", "CORE", { { glue, 20 }, { punch, 10 }, { color, 8 }, { loudness, 25 } } },
        { "POLOOYX DARK", "CORE", { { tone, -35 }, { glue, 20 }, { color, 12 }, { colorMode, (float) ColorMode::Tube }, { eqId (6, "Gain"), -1.0f }, { loudness, 20 } } },
        { "POLOOYX POLISHED", "CORE", { { glue, 25 }, { tone, 10 }, { aura, 15 }, { glass, 25 }, { punch, 15 }, { loudness, 30 } } },
        // ---------------------------------------------------------------- RELEASE
        { "POLOOYX RELEASE", "RELEASE", { { glue, 20 }, { punch, 15 }, { color, 8 }, { loudness, 35 } } },
        { "POLOOYX STREAM", "RELEASE", { { glue, 12 }, { loudness, 25 }, { limStyle, (float) LimStyle::Transparent }, { limCeiling, -1.0f } } },
        { "POLOOYX MODERN", "RELEASE", { { glue, 25 }, { punch, 20 }, { loudness, 45 }, { limStyle, (float) LimStyle::Modern }, { clipMode, (float) ClipMode::Soft } } },
        { "POLOOYX LOUD", "RELEASE", { { glue, 30 }, { punch, 15 }, { loudness, 65 }, { clipDrive, 1.0f }, { limStyle, (float) LimStyle::Dense } } },
        { "POLOOYX TRANSPARENT", "RELEASE", { { limStyle, (float) LimStyle::Transparent }, { limLookahead, 8 }, { limRelease, 220 } } },
        // ---------------------------------------------------------------- CHARACTER
        { "POLOOYX AURA", "CHARACTER", { { aura, 40 }, { glass, 15 }, { width, 15 }, { monoBass, 1 }, { loudness, 25 } } },
        { "POLOOYX GRIT", "CHARACTER", { { grit, 40 }, { color, 25 }, { colorMode, (float) ColorMode::Tube }, { punch, 15 }, { loudness, 25 } } },
        { "POLOOYX GLASS", "CHARACTER", { { glass, 50 }, { aura, 15 }, { tone, 8 }, { loudness, 25 } } },
        { "POLOOYX DEPTH", "CHARACTER", { { depth, 45 }, { space, 15 }, { width, 10 }, { loudness, 25 } } },
        { "POLOOYX SPACE", "CHARACTER", { { space, 45 }, { depth, 20 }, { width, 15 }, { monoBass, 1 }, { monoFreq, 100 }, { loudness, 25 } } },
        // ---------------------------------------------------------------- STYLE
        { "UNDERGROUND", "STYLE", { { tone, -20 }, { grit, 25 }, { color, 18 }, { punch, 25 }, { glue, 25 }, { loudness, 40 } } },
        { "DARK", "STYLE", { { tone, -45 }, { color, 15 }, { colorMode, (float) ColorMode::Tube }, { depth, 20 }, { loudness, 30 } } },
        { "PSYCHEDELIC", "STYLE", { { aura, 30 }, { space, 35 }, { depth, 30 }, { width, 25 }, { monoBass, 1 }, { color, 12 }, { loudness, 30 } } },
        { "HEAVY", "STYLE", { { glue, 35 }, { punch, 30 }, { grit, 20 }, { color, 20 }, { loudness, 55 }, { limStyle, (float) LimStyle::Dense } } },
        { "WIDE", "STYLE", { { width, 40 }, { aura, 15 }, { monoBass, 1 }, { monoFreq, 100 }, { loudness, 25 } } },
        { "VOCAL FORWARD", "STYLE", { { eqId (5, "Gain"), 1.0f }, { eqId (5, "Freq"), 3200.0f }, { eqId (5, "Q"), 0.8f },
                                      { eqId (2, "Gain"), -0.8f }, { eqId (2, "Freq"), 300.0f }, { glue, 15 }, { loudness, 25 } } },
    };
    return p;
}
} // namespace prl
