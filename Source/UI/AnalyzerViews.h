#pragma once
// POLOOYX RELEASE ENGINE — analyser views (spectrum, stereo field, loudness history).
// Thin white lines on black, subtle grid, small technical labels. No glow, no particles.

#include "Widgets.h"
#include "../Analysis/SpectrumAnalyzer.h"
#include "../Analysis/MixDiagnostics.h"

namespace prl::ui
{
class SpectrumView : public juce::Component
{
public:
    int mode = 2;              // 0 PRE, 1 POST, 2 BOTH
    float rangeDb = 90.0f;
    bool scanning = false;     // ANALYZE POLO listening → thin scan line
    float scanPos = 0.0f;
    const AnalysisResult* longTerm = nullptr;   // long-term spectrum from the last analysis (dashed)

    SpectrumAnalyzer pre, post;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg); g.fillRect (r);
        plot = r.reduced (34.0f, 10.0f).withTrimmedBottom (8.0f).withTrimmedLeft (-6.0f);
        const float top = 6.0f, bottom = top - rangeDb;

        // grid
        g.setFont (font (9.0f));
        for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
        {
            const float x = xOf (f);
            g.setColour (col::border); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::muted);
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 16, (int) plot.getBottom() + 2, 32, 12, juce::Justification::centred);
        }
        const float step = rangeDb > 100 ? 20.0f : rangeDb > 70 ? 15.0f : 10.0f;
        for (float db = 0.0f; db >= bottom; db -= step)
        {
            const float y = yOf (db, top, bottom);
            g.setColour (db == 0.0f ? col::line : col::border); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col::muted); g.drawText (juce::String ((int) db), (int) plot.getX() - 34, (int) y - 6, 28, 12, juce::Justification::centredRight);
        }

        // long-term analysis spectrum (dashed, gray) — normalised to the post curve's level
        if (longTerm != nullptr && ! longTerm->avgSpectrumDb.empty())
        {
            juce::Path p; bool started = false;
            for (size_t i = 0; i < longTerm->avgSpectrumDb.size(); ++i)
            {
                const float x = xOf (longTerm->avgSpectrumHz[i]), y = yOf (longTerm->avgSpectrumDb[i] - 18.0f, top, bottom);
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            juce::Path dashed; const float dl[] = { 3.0f, 4.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, p, dl, 2);
            g.setColour (col::muted.withAlpha (0.8f)); g.fillPath (dashed);
        }

        auto curve = [&] (const SpectrumAnalyzer& s, bool peaks) {
            juce::Path p; bool started = false;
            const auto& v = peaks ? s.peaks() : s.values();
            for (int i = 0; i < SpectrumAnalyzer::kPoints; ++i)
            {
                const float x = xOf (SpectrumAnalyzer::freqAt (i)), y = yOf (v[(size_t) i], top, bottom);
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            return p;
        };
        g.saveState(); g.reduceClipRegion (plot.toNearestInt());
        if (mode == 0 || mode == 2)
        {
            auto p = curve (pre, false);
            g.setColour (col::muted); g.strokePath (p, juce::PathStrokeType (1.1f));
        }
        if (mode == 1 || mode == 2)
        {
            auto p = curve (post, false);
            juce::Path fill (p); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
            g.setColour (col::white.withAlpha (0.035f)); g.fillPath (fill);
            g.setColour (col::white); g.strokePath (p, juce::PathStrokeType (1.25f));
        }
        if (holdShown)
        {
            auto p = curve (mode == 0 ? pre : post, true);
            juce::Path dotted; const float dl[] = { 1.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dotted, p, dl, 2);
            g.setColour (col::text2); g.fillPath (dotted);
        }
        if (scanning)
        {
            const float x = plot.getX() + scanPos * plot.getWidth();
            g.setColour (col::white.withAlpha (0.85f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::white.withAlpha (0.06f)); g.fillRect (plot.withRight (x).withLeft (juce::jmax (plot.getX(), x - 60.0f)));
        }
        g.restoreState();

        // legend
        g.setFont (font (9.0f, W::Medium, 0.1f));
        auto lg = juce::Rectangle<int> ((int) plot.getX() + 8, (int) plot.getY() + 4, 220, 12);
        if (mode != 1) { g.setColour (col::muted); g.fillRect (lg.getX(), lg.getCentreY(), 12, 1); g.drawText ("PRE", lg.withTrimmedLeft (16), juce::Justification::centredLeft); lg.translate (52, 0); }
        if (mode != 0) { g.setColour (col::white); g.fillRect (lg.getX(), lg.getCentreY(), 12, 1); g.drawText ("POST", lg.withTrimmedLeft (16), juce::Justification::centredLeft); lg.translate (58, 0); }
        if (longTerm != nullptr) { g.setColour (col::muted); g.drawText ("- - ANALYSIS AVG", lg.withWidth (120), juce::Justification::centredLeft); }
        if (frozen) { g.setColour (col::white); g.drawText ("FROZEN", (int) plot.getRight() - 70, (int) plot.getY() + 4, 64, 12, juce::Justification::centredRight); }
    }

    bool holdShown = false, frozen = false;

private:
    float xOf (double f) const { return plot.getX() + (float) (std::log10 (f / 20.0) / 3.0) * plot.getWidth(); }
    float yOf (float db, float top, float bottom) const { return plot.getY() + juce::jlimit (0.0f, 1.0f, (top - db) / (top - bottom)) * plot.getHeight(); }
    juce::Rectangle<float> plot;
};

// goniometer + correlation + width/mid/side + short-term loudness history
class StereoView : public juce::Component
{
public:
    void push (const float* l, const float* r, int n)
    {
        for (int i = 0; i < n; ++i) { pts[(size_t) w] = { l[i], r[i] }; w = (w + 1) % (int) pts.size(); }
    }
    void setValues (float c, float wPct, float midDb, float sideDb, float bal) { corr = c; width = wPct; mid = midDb; side = sideDb; balance = bal; }
    void pushHistory (float lufsS) { hist[(size_t) hw] = lufsS; hw = (hw + 1) % (int) hist.size(); }
    float target = -14.0f;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg); g.fillRect (r);
        // goniometer
        auto gon = r.removeFromTop (juce::jmin (r.getWidth(), r.getHeight() * 0.56f)).reduced (8.0f);
        const float sz = juce::jmin (gon.getWidth(), gon.getHeight());
        gon = gon.withSizeKeepingCentre (sz, sz);
        const auto c = gon.getCentre();
        g.setColour (col::border);
        g.drawLine (c.x, gon.getY(), c.x, gon.getBottom(), 1.0f);
        g.drawLine (gon.getX(), c.y, gon.getRight(), c.y, 1.0f);
        g.drawLine (gon.getX(), gon.getY(), gon.getRight(), gon.getBottom(), 0.6f);
        g.drawLine (gon.getRight(), gon.getY(), gon.getX(), gon.getBottom(), 0.6f);
        g.setColour (col::muted); g.setFont (font (8.5f, W::Medium, 0.1f));
        g.drawText ("M", (int) c.x - 6, (int) gon.getY() - 1, 12, 10, juce::Justification::centred);
        g.drawText ("L", (int) gon.getX(), (int) gon.getY() - 1, 10, 10, juce::Justification::centred);
        g.drawText ("R", (int) gon.getRight() - 10, (int) gon.getY() - 1, 10, 10, juce::Justification::centred);
        float mx = 1e-4f;
        for (auto& p : pts) mx = std::max (mx, std::max (std::abs (p.first), std::abs (p.second)));
        const float scale = 0.48f * sz / std::max (0.25f, mx);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto& p = pts[i];
            const float x = (p.second - p.first) * 0.7071f, y = (p.first + p.second) * 0.7071f;
            const float age = (float) ((i + pts.size() - (size_t) w) % pts.size()) / (float) pts.size();
            g.setColour (col::white.withAlpha (0.08f + 0.5f * age));
            g.fillRect (c.x + x * scale, c.y - y * scale, 1.2f, 1.2f);
        }
        // correlation bar
        r.removeFromTop (6);
        auto cb = r.removeFromTop (26).reduced (8.0f, 0.0f);
        g.setColour (col::muted); g.setFont (font (8.5f, W::Medium, 0.1f));
        g.drawText ("-1", cb.removeFromLeft (14), juce::Justification::centredLeft);
        g.drawText ("+1", cb.removeFromRight (14), juce::Justification::centredRight);
        auto bar = cb.reduced (4.0f, 8.0f);
        g.setColour (col::bg2); g.fillRect (bar); g.setColour (col::border); g.drawRect (bar, 1.0f);
        g.setColour (col::line2); g.fillRect (bar.getCentreX(), bar.getY(), 1.0f, bar.getHeight());
        const float cx = bar.getX() + (corr + 1.0f) * 0.5f * bar.getWidth();
        g.setColour (corr < 0 ? col::text2 : col::white); g.fillRect (cx - 1.5f, bar.getY() - 3, 3.0f, bar.getHeight() + 6);
        // readouts
        auto rd = r.removeFromTop (34).reduced (8.0f, 0.0f).toNearestInt();
        const int cw = rd.getWidth() / 3;
        Readout::draw (g, rd.removeFromLeft (cw), "CORRELATION", juce::String (corr, 2), 12.5f);
        Readout::draw (g, rd.removeFromLeft (cw), "WIDTH", juce::String ((int) width) + "%", 12.5f);
        Readout::draw (g, rd, "SIDE / MID", side > -150 && mid > -150 ? juce::String (side - mid, 1) + " dB" : "-", 12.5f);
        // loudness history (short-term, last 60 s)
        r.removeFromTop (6);
        auto h = r.reduced (8.0f, 2.0f);
        g.setColour (col::muted); g.setFont (font (8.5f, W::Medium, 0.1f));
        g.drawText ("SHORT-TERM LUFS  60 s", h.removeFromTop (11), juce::Justification::centredLeft);
        g.setColour (col::bg2); g.fillRect (h); g.setColour (col::border); g.drawRect (h, 1.0f);
        auto yOf = [&] (float l) { return h.getBottom() - juce::jlimit (0.0f, 1.0f, (l + 40.0f) / 40.0f) * h.getHeight(); };
        juce::Path tl; const float dl[] = { 2.0f, 3.0f };
        juce::Path tline; tline.startNewSubPath (h.getX(), yOf (target)); tline.lineTo (h.getRight(), yOf (target));
        juce::PathStrokeType (1.0f).createDashedStroke (tl, tline, dl, 2);
        g.setColour (col::muted); g.fillPath (tl);
        juce::Path p; bool started = false;
        for (int i = 0; i < (int) hist.size(); ++i)
        {
            const float v = hist[(size_t) ((hw + i) % (int) hist.size())];
            const float x = h.getX() + h.getWidth() * (float) i / (float) (hist.size() - 1);
            if (v < -60) { started = false; continue; }
            if (! started) { p.startNewSubPath (x, yOf (v)); started = true; } else p.lineTo (x, yOf (v));
        }
        g.setColour (col::white); g.strokePath (p, juce::PathStrokeType (1.0f));
    }

private:
    std::array<std::pair<float, float>, 1024> pts {};
    int w = 0;
    std::array<float, 600> hist {};   // 60 s at 10 Hz
    int hw = 0;
    float corr = 1, width = 0, mid = -200, side = -200, balance = 0;
public:
    StereoView() { hist.fill (-200.0f); }
};
} // namespace prl::ui
