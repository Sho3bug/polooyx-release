#pragma once
// POLOOYX RELEASE ENGINE — correction data model + per-mode limits for POLO IT.

#include "../Params.h"
#include <vector>

namespace prl
{
struct Correction
{
    juce::String paramId;      // APVTS parameter id
    float value = 0;           // new value (real units)
    float before = 0;          // value before POLO IT (filled in by the engine)
    juce::String group;        // EQ / DYN EQ / GLUE / COLOR / WIDTH / CLIP / LIMITER / CHARACTER
    juce::String reason;       // the measurement that triggered it
};

struct ModeLimits
{
    float eqScale, eqMaxCut;       // EQ: correction scale and the largest cut this mode may apply (dB)
    float glueMaxMix;              // parallel glue mix ceiling (0..1)
    float colorMix;                // harmonic color mix applied (0..1)
    float characterAmt;            // aura / grit amount (0..1)
    float clipMaxDb;               // clipper drive ceiling (dB)
    float plrFloor;                // never limit harder than this peak-to-loudness ratio (dB)
    bool allowAboveTarget;         // may the result end above the target (never for SAFE)
};

inline ModeLimits limitsFor (PoloMode m)
{
    switch (m)
    {
        case PoloMode::Transparent: return { 0.6f, 1.5f, 0.08f, 0.00f, 0.00f, 0.0f, 10.5f, false };
        case PoloMode::Balanced:    return { 1.0f, 2.5f, 0.15f, 0.00f, 0.00f, 0.75f, 9.0f, false };
        case PoloMode::Character:   return { 1.0f, 2.5f, 0.20f, 0.12f, 0.10f, 1.0f, 8.5f, false };
        case PoloMode::Loud:        return { 0.8f, 2.0f, 0.25f, 0.06f, 0.00f, 2.0f, 7.0f, false };
        case PoloMode::Safe:        return { 0.4f, 1.0f, 0.05f, 0.00f, 0.00f, 0.0f, 11.0f, false };
    }
    return limitsFor (PoloMode::Balanced);
}

// hard bounds that no automatic correction may ever exceed (spec §24 / §72)
struct HardLimits
{
    static constexpr float eqMaxDb = 3.0f;          // ±3 dB, and automatic EQ only ever cuts
    static constexpr float colorMaxMix = 0.15f;     // 15 %
    static constexpr float stereoMaxChange = 0.15f; // 15 % (and never wider)
    static constexpr float glueMaxMix = 0.25f;      // 25 %
    static constexpr float clipMaxDb = 3.0f;
    static constexpr float limDriveMaxDb = 12.0f;
    static constexpr float characterMax = 0.15f;
};
} // namespace prl
