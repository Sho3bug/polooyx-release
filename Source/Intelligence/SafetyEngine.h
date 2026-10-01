#pragma once
// POLOOYX RELEASE ENGINE — safety engine: validates every automatic correction against hard bounds.
// It never adds processing; it only clamps or removes, and reports what it clamped.

#include "CorrectionModel.h"

namespace prl
{
class SafetyEngine
{
public:
    // clamps corrections in place, returns notes describing anything that was limited/removed
    static juce::StringArray validate (std::vector<Correction>& cs)
    {
        juce::StringArray notes;
        for (auto it = cs.begin(); it != cs.end();)
        {
            auto& c = *it;
            const auto pid = c.paramId;
            float v = c.value;
            if (pid.startsWith ("eq") && pid.endsWith ("Gain"))
            {
                const float lim = juce::jlimit (-HardLimits::eqMaxDb, 0.0f, v);   // cuts only, max 3 dB
                if (lim != v) notes.add ("Safety: " + pid + " limited to " + juce::String (lim, 1) + " dB (automatic EQ may only cut, max 3 dB).");
                v = lim;
            }
            else if (pid == id::colorMix)   v = clampNote (v, 0.0f, HardLimits::colorMaxMix * 100.0f, "color mix", notes);
            else if (pid == id::glueMix)    v = clampNote (v, 0.0f, HardLimits::glueMaxMix * 100.0f, "glue mix", notes);
            else if (pid == id::stWidth || pid == id::stHighWidth || pid == id::stLowWidth)
            {
                const float lo = c.before * (1.0f - HardLimits::stereoMaxChange);
                const float hi = c.before;   // never widen automatically
                v = clampNote (v, lo, hi, "stereo width", notes);
            }
            else if (pid == id::clipDrive)  v = clampNote (v, 0.0f, HardLimits::clipMaxDb, "clipper drive", notes);
            else if (pid == id::limThresh)  v = clampNote (v, -HardLimits::limDriveMaxDb, 0.0f, "limiter drive", notes);
            else if (pid == id::aura || pid == id::grit) v = clampNote (v, 0.0f, HardLimits::characterMax * 100.0f, "character", notes);
            c.value = v;
            // drop no-op corrections
            if (std::abs (c.value - c.before) < 1.0e-4f) it = cs.erase (it); else ++it;
        }
        return notes;
    }

private:
    static float clampNote (float v, float lo, float hi, const char* what, juce::StringArray& notes)
    {
        const float c = juce::jlimit (lo, hi, v);
        if (std::abs (c - v) > 1.0e-4f) notes.add (juce::String ("Safety: ") + what + " limited to " + juce::String (c, 1) + ".");
        return c;
    }
};
} // namespace prl
