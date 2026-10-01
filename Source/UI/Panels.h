#pragma once
// POLOOYX RELEASE ENGINE — POLO DIAGNOSIS / NEXT STEPS / RESULT (before-after, changes, final check).

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace prl::ui
{
inline void sectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, const juce::String& sub = {})
{
    g.setColour (col::white); g.setFont (font (10.5f, W::SemiBold, 0.18f));
    g.drawText (t, r, juce::Justification::centredLeft, false);
    if (sub.isNotEmpty())
    {
        g.setColour (col::muted); g.setFont (font (9.5f, W::Regular, 0.04f));
        g.drawText (sub, r, juce::Justification::centredRight, false);
    }
}

// human-readable "CHANGES MADE" lines from a POLO IT result
inline std::vector<std::pair<juce::String, juce::String>> describeChanges (const PoloItResult& p)
{
    std::vector<std::pair<juce::String, juce::String>> out;
    auto find = [&] (const juce::String& pid, float& v) { for (auto& c : p.changes) if (c.paramId == pid) { v = c.value; return true; } return false; };
    for (int b = 0; b < kEqBands; ++b)
    {
        float g = 0, f = 0;
        if (find (eqId (b, "Gain"), g))
        {
            if (! find (eqId (b, "Freq"), f)) f = eqDefault (b).freq;
            out.push_back ({ "EQ", juce::String (g, 1) + " dB @ " + (f >= 1000 ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz") });
        }
    }
    float v = 0;
    if (find (deqId (2, "Range"), v)) { float t = 0; find (deqId (2, "Thr"), t); out.push_back ({ "DYN EQ", "2-5 kHz up to -" + juce::String (v, 1) + " dB above " + juce::String (t, 0) + " dB" }); }
    if (find (id::glueMix, v)) { float t = 0; find (id::glueThr, t); out.push_back ({ "GLUE", "+" + juce::String (v, 0) + "% parallel (thr " + juce::String (t, 0) + " dB, 2:1)" }); }
    if (find (id::colorMix, v)) out.push_back ({ "COLOR", "+" + juce::String (v, 0) + "% TAPE" });
    if (find (id::stWidth, v))
    {
        float before = 100; for (auto& c : p.changes) if (c.paramId == id::stWidth) before = c.before;
        out.push_back ({ "WIDTH", juce::String (v - before, 0) + "%" });
    }
    if (find (id::monoFreq, v)) out.push_back ({ "WIDTH", "low end mono below " + juce::String ((int) v) + " Hz" });
    if (find (id::aura, v)) { float gr = 0; find (id::grit, gr); out.push_back ({ "CHARACTER", "AURA +" + juce::String (v, 0) + "%  GRIT +" + juce::String (gr, 0) + "%" }); }
    if (find (id::clipDrive, v)) out.push_back ({ "CLIP", "+" + juce::String (v, 1) + " dB" });
    if (find (id::limThresh, v)) out.push_back ({ "LIMITER", "+" + juce::String (-v, 1) + " dB" });
    if (find (id::limStyle, v)) out.push_back ({ "LIMITER", "style " + limStyleNames()[(int) v] });
    return out;
}

class DiagnosisPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    void set (const PolooyxReleaseProcessor::Results& r) { res = r; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        sectionTitle (g, r.removeFromTop (16), "POLO DIAGNOSIS", res.hasAnalysis ? juce::String (res.analysis.seconds, 0) + " s analysed" : juce::String());
        r.removeFromTop (6);
        g.setColour (col::line); g.fillRect (r.removeFromTop (1)); r.removeFromTop (6);
        if (! res.hasAnalysis)
        {
            g.setColour (col::muted); g.setFont (font (11.5f));
            g.drawFittedText ("Press ANALYZE POLO and play your mix - the whole song, or at least its loudest part. "
                              "POLO measures loudness, peaks, spectrum, low end, stereo and dynamics, then tells you what it found.",
                              r.reduced (0, 4), juce::Justification::topLeft, 6);
            return;
        }
        const auto& items = res.diagnosis.items;
        const int cols = 2, rowH = 26;
        const int rows = (int) std::ceil (items.size() / (double) cols);
        const int cw = r.getWidth() / cols;
        tips.clear();
        for (size_t i = 0; i < items.size(); ++i)
        {
            const int c = (int) i / juce::jmax (1, rows), row = (int) i % juce::jmax (1, rows);
            auto cell = juce::Rectangle<int> (r.getX() + c * cw, r.getY() + row * rowH, cw - 10, rowH);
            if (cell.getBottom() > getHeight()) continue;
            const auto& it = items[i];
            g.setColour (col::muted); g.setFont (font (8.5f, W::Medium, 0.12f));
            g.drawText (it.category, cell.removeFromTop (11), juce::Justification::centredLeft, false);
            auto st = cell.withHeight (14);
            g.setFont (font (11.0f, it.level > 0 ? W::Medium : W::Regular, 0.03f));
            if (it.level >= 2)
            {
                const int w = (int) textWidth (font (11.0f, W::Medium, 0.03f), it.state) + 10;
                g.setColour (col::white); g.fillRect (st.withWidth (juce::jmin (w, st.getWidth())));
                g.setColour (juce::Colours::black); g.drawText (it.state, st.withTrimmedLeft (5), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (it.level == 1 ? col::white : col::text2);
                if (it.level == 1) { g.fillEllipse ((float) st.getX(), st.getCentreY() - 2.0f, 4.0f, 4.0f); st.removeFromLeft (8); }
                g.drawText (it.state, st, juce::Justification::centredLeft, true);
            }
            tips.push_back ({ juce::Rectangle<int> (r.getX() + c * cw, r.getY() + row * rowH, cw - 10, rowH), it.detail });
        }
    }
    juce::String getTipAt (juce::Point<int> p) const { for (auto& t : tips) if (t.first.contains (p)) return t.second; return {}; }
private:
    PolooyxReleaseProcessor::Results res;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> tips;
};

class NextStepsPanel : public juce::Component
{
public:
    void set (const PolooyxReleaseProcessor::Results& r) { res = r; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        sectionTitle (g, r.removeFromTop (16), "NEXT STEPS");
        r.removeFromTop (6); g.setColour (col::line); g.fillRect (r.removeFromTop (1)); r.removeFromTop (6);
        if (! res.hasAnalysis)
        {
            g.setColour (col::muted); g.setFont (font (11.5f));
            g.drawFittedText ("What to check - based on the measurements, not on taste. Appears after ANALYZE POLO.", r, juce::Justification::topLeft, 4);
            return;
        }
        const auto& steps = res.diagnosis.nextSteps;
        for (int i = 0; i < steps.size(); ++i)
        {
            if (r.getHeight() < 20) break;
            auto row = r.removeFromTop (juce::jmin (r.getHeight(), 34));
            g.setColour (col::muted); g.setFont (font (10.0f, W::Medium));
            g.drawText (juce::String (i + 1).paddedLeft ('0', 2), row.removeFromLeft (22).withHeight (14), juce::Justification::topLeft, false);
            g.setColour (col::text2); g.setFont (font (11.0f));
            g.drawFittedText (steps[i], row, juce::Justification::topLeft, 2, 1.0f);
        }
    }
private:
    PolooyxReleaseProcessor::Results res;
};

class ResultPanel : public juce::Component
{
public:
    ResultPanel()
    {
        for (auto* b : { &tabBA, &tabCh, &tabFc })
        {
            b->getProperties().set ("fontSize", 9.0f); b->getProperties().set ("flat", true);
            b->setClickingTogglesState (false); addAndMakeVisible (*b);
        }
        tabBA.onClick = [this] { page = 0; refreshTabs(); };
        tabCh.onClick = [this] { page = 1; refreshTabs(); };
        tabFc.onClick = [this] { page = 2; refreshTabs(); };
        refreshTabs();
    }
    void set (const PolooyxReleaseProcessor::Results& r)
    {
        if (r.version != res.version) { if (r.lastKind == 2) page = 0; else if (r.lastKind == 3) page = 2; }
        res = r;
        refreshTabs();
    }
    void resized() override
    {
        auto r = getLocalBounds().removeFromTop (16);
        tabFc.setBounds (r.removeFromRight (76)); tabCh.setBounds (r.removeFromRight (62)); tabBA.setBounds (r.removeFromRight (86));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        sectionTitle (g, r.removeFromTop (16), "RESULT");
        r.removeFromTop (6); g.setColour (col::line); g.fillRect (r.removeFromTop (1)); r.removeFromTop (6);
        if (page == 0) paintBeforeAfter (g, r);
        else if (page == 1) paintChanges (g, r);
        else paintFinal (g, r);
    }
private:
    void refreshTabs() { tabBA.setToggleState (page == 0, juce::dontSendNotification); tabCh.setToggleState (page == 1, juce::dontSendNotification); tabFc.setToggleState (page == 2, juce::dontSendNotification); repaint(); }

    void paintBeforeAfter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (! res.hasPolo)
        {
            g.setColour (col::muted); g.setFont (font (11.5f));
            g.drawFittedText ("POLO IT analyses the mix, applies the smallest corrections that solve what it measured, renders the result "
                              "and verifies it. BEFORE / AFTER appears here.", r, juce::Justification::topLeft, 5);
            return;
        }
        const auto& b = res.polo.before; const auto& a = res.polo.after;
        const int c0 = 100, cw = (r.getWidth() - c0) / 2;
        auto head = r.removeFromTop (14);
        g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.12f));
        g.drawText ("BEFORE", head.withX (r.getX() + c0).withWidth (cw), juce::Justification::centredLeft);
        g.setColour (col::white);
        g.drawText ("AFTER", head.withX (r.getX() + c0 + cw).withWidth (cw), juce::Justification::centredLeft);
        auto row = [&] (const char* lab, juce::String vb, juce::String va)
        {
            auto rr = r.removeFromTop (18);
            g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.12f)); g.drawText (lab, rr.withWidth (c0), juce::Justification::centredLeft);
            g.setColour (col::text2); g.setFont (font (11.5f)); g.drawText (vb, rr.withX (rr.getX() + c0).withWidth (cw), juce::Justification::centredLeft);
            g.setColour (col::white); g.setFont (font (11.5f, W::Medium)); g.drawText (va, rr.withX (rr.getX() + c0 + cw).withWidth (cw), juce::Justification::centredLeft);
        };
        row ("LUFS-I", juce::String (b.lufsI, 1), juce::String (a.lufsI, 1));
        row ("TRUE PEAK", juce::String (b.truePeakDb, 1) + " dBTP", juce::String (a.truePeakDb, 1) + " dBTP");
        row ("CREST / PLR", juce::String (b.crestDb, 1) + " / " + juce::String (b.plr, 1), juce::String (a.crestDb, 1) + " / " + juce::String (a.plr, 1));
        row ("WIDTH", juce::String ((int) b.widthPct) + "%", juce::String ((int) a.widthPct) + "%");
        row ("CORRELATION", juce::String (b.correlation, 2), juce::String (a.correlation, 2));
        if (r.getHeight() > 14)
        {
            g.setColour (col::muted); g.setFont (font (10.0f));
            g.drawFittedText (res.polo.nothingToDo ? "NO CHANGES NEEDED - the mix did not cross any correction threshold." : res.polo.notes.joinIntoString ("  "), r, juce::Justification::topLeft, 3);
        }
    }

    void paintChanges (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (! res.hasPolo) { g.setColour (col::muted); g.setFont (font (11.5f)); g.drawText ("Run POLO IT to see exactly what it changed.", r, juce::Justification::topLeft); return; }
        const auto lines = describeChanges (res.polo);
        if (lines.empty()) { g.setColour (col::white); g.setFont (font (12.0f, W::Medium)); g.drawText ("NO CHANGES NEEDED", r, juce::Justification::topLeft); return; }
        for (auto& [grp, txt] : lines)
        {
            if (r.getHeight() < 16) break;
            auto rr = r.removeFromTop (17);
            g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.12f)); g.drawText (grp, rr.removeFromLeft (80), juce::Justification::centredLeft);
            g.setColour (col::white); g.setFont (font (11.5f)); g.drawText (txt, rr, juce::Justification::centredLeft);
        }
    }

    void paintFinal (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (! res.hasFinal) { g.setColour (col::muted); g.setFont (font (11.5f)); g.drawFittedText ("FINAL CHECK renders the captured mix through the current settings and checks loudness, true peak, density, stereo and low-end compatibility.", r, juce::Justification::topLeft, 4); return; }
        const auto& fc = res.finalCheck;
        for (auto& s : fc.states)
        {
            auto rr = r.removeFromTop (22);
            const bool ok = s == "READY FOR EXPORT";
            const int w = (int) textWidth (font (12.0f, W::SemiBold, 0.1f), s) + 16;
            g.setColour (col::white);
            if (ok) { g.fillRect (rr.withWidth (w).reduced (0, 2)); g.setColour (juce::Colours::black); }
            else g.drawRect (rr.withWidth (w).reduced (0, 2), 1);
            g.setFont (font (12.0f, W::SemiBold, 0.1f)); g.drawText (s, rr.withWidth (w), juce::Justification::centred);
            r.removeFromTop (3);
        }
        g.setColour (col::text2); g.setFont (font (10.5f));
        g.drawFittedText (fc.details.joinIntoString ("\n"), r, juce::Justification::topLeft, 6);
    }

    PolooyxReleaseProcessor::Results res;
    int page = 0;
    juce::TextButton tabBA { "BEFORE/AFTER" }, tabCh { "CHANGES" }, tabFc { "FINAL CHECK" };
};

// live status shown across the middle panel while listening / working
class StatusOverlay : public juce::Component
{
public:
    juce::String title, line;
    float prog = 0.0f;
    int stage = -1;            // 0 analysing, 1 correcting, 2 verifying (POLO IT)
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (col::bg.withAlpha (0.94f)); g.fillRect (r);
        auto c = r.withSizeKeepingCentre (juce::jmin (640, r.getWidth() - 40), 110);
        g.setColour (col::white); g.setFont (font (16.0f, W::SemiBold, 0.2f));
        g.drawText (title, c.removeFromTop (26), juce::Justification::centred);
        if (stage >= 0)
        {
            auto st = c.removeFromTop (24);
            const char* names[] = { "ANALYZING", "CORRECTING", "VERIFYING" };
            const int w = st.getWidth() / 3;
            for (int i = 0; i < 3; ++i)
            {
                auto b = st.removeFromLeft (w);
                g.setColour (i == stage ? col::white : i < stage ? col::text2 : col::muted);
                g.setFont (font (10.5f, i == stage ? W::SemiBold : W::Regular, 0.16f));
                g.drawText (juce::String (names[i]) + (i < 2 ? "    >" : ""), b, juce::Justification::centred);
            }
        }
        c.removeFromTop (8);
        auto bar = c.removeFromTop (2).toFloat();
        g.setColour (col::line); g.fillRect (bar);
        g.setColour (col::white); g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, prog)));
        c.removeFromTop (10);
        g.setColour (col::text2); g.setFont (font (11.5f));
        g.drawFittedText (line, c, juce::Justification::centredTop, 3);
    }
};
} // namespace prl::ui
