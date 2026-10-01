#pragma once
// POLOOYX RELEASE ENGINE — PRO module pages + LAB measurement pages.

#include "Widgets.h"
#include "../PluginProcessor.h"
#include "../DSP/MasterEQ.h"

namespace prl::ui
{
// ---------------------------------------------------------------- generic module page
class ModulePage : public juce::Component
{
public:
    struct Group
    {
        juce::String title, note;
        std::unique_ptr<Toggle> enable;
        std::vector<std::unique_ptr<juce::Component>> items;   // knobs / choices / toggles in flow order
        std::function<void (juce::Graphics&, juce::Rectangle<int>)> extraPaint;   // e.g. gain-reduction strip
        int extraH = 0;
        float weight = 1.0f;
        juce::Rectangle<int> area;
    };
    std::vector<Group> groups;
    int knobW = 74, knobH = 84;

    Group& add (const juce::String& title, APVTS* s = nullptr, const juce::String& enableId = {})
    {
        groups.emplace_back();
        auto& g = groups.back();
        g.title = title;
        if (s != nullptr && enableId.isNotEmpty()) { g.enable = std::make_unique<Toggle> (*s, enableId, "ON"); addAndMakeVisible (*g.enable); }
        return g;
    }
    template <typename T> T* put (Group& g, std::unique_ptr<T> c) { auto* p = c.get(); addAndMakeVisible (*c); g.items.push_back (std::move (c)); return p; }

    void resized() override
    {
        auto r = getLocalBounds();
        float tw = 0; for (auto& g : groups) tw += g.weight;
        const int gap = 18;
        const int avail = r.getWidth() - gap * ((int) groups.size() - 1);
        for (auto& g : groups)
        {
            auto a = r.removeFromLeft ((int) (avail * g.weight / tw));
            r.removeFromLeft (gap);
            g.area = a;
            auto head = a.removeFromTop (20);
            if (g.enable) g.enable->setBounds (head.removeFromRight (40).reduced (0, 2));
            a.removeFromTop (8);
            if (g.extraH > 0) a.removeFromBottom (g.extraH + 6);
            if (g.note.isNotEmpty()) a.removeFromBottom (30);
            int x = a.getX(), y = a.getY();
            for (auto& it : g.items)
            {
                const bool isKnob = dynamic_cast<Knob*> (it.get()) != nullptr;
                const int w = isKnob ? knobW : 112, h = isKnob ? knobH : 40;
                if (x + w > a.getRight()) { x = a.getX(); y += knobH + 6; }
                it->setBounds (x, y + (isKnob ? 0 : 16), w - 6, isKnob ? h : 38);
                x += w;
            }
        }
    }
    void paint (juce::Graphics& g) override
    {
        for (auto& gr : groups)
        {
            auto a = gr.area;
            auto head = a.removeFromTop (20);
            g.setColour (col::white); g.setFont (font (10.5f, W::SemiBold, 0.18f));
            g.drawText (gr.title, head, juce::Justification::centredLeft, false);
            g.setColour (col::line); g.fillRect (a.removeFromTop (1));
            if (gr.extraH > 0 && gr.extraPaint) gr.extraPaint (g, gr.area.withTop (gr.area.getBottom() - gr.extraH - (gr.note.isNotEmpty() ? 30 : 0)).withHeight (gr.extraH));
            if (gr.note.isNotEmpty())
            {
                g.setColour (col::muted); g.setFont (font (10.0f));
                g.drawFittedText (gr.note, gr.area.withTop (gr.area.getBottom() - 28), juce::Justification::topLeft, 2);
            }
        }
    }
};

// ---------------------------------------------------------------- EQ curve
class EqCurve : public juce::Component
{
public:
    EqCurve (PolooyxReleaseProcessor& p) : proc (p) {}
    void update()
    {
        MasterEQ::Settings s;
        for (int b = 0; b < kEqBands; ++b)
            s.bands[(size_t) b] = { (EqType) (int) proc.param (eqId (b, "Type")), proc.param (eqId (b, "Freq")), proc.param (eqId (b, "Gain")), proc.param (eqId (b, "Q")) };
        s.tiltDb = 2.0f * proc.param (id::tone) * 0.01f;
        double h = s.tiltDb; for (auto& b : s.bands) h = h * 1.7 + b.freq * 0.001 + b.gainDb * 3.1 + b.q + (int) b.type;
        if (h == lastHash) return;
        lastHash = h; settings = s; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (col::bg); g.fillRect (r); g.setColour (col::border); g.drawRect (r, 1.0f);
        auto xOf = [&] (double f) { return r.getX() + (float) (std::log10 (f / 20.0) / 3.0) * r.getWidth(); };
        auto yOf = [&] (float db) { return r.getCentreY() - juce::jlimit (-1.0f, 1.0f, db / 12.0f) * r.getHeight() * 0.45f; };
        g.setColour (col::border);
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        for (float db : { -6.0f, 6.0f }) g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
        g.setColour (col::line2); g.drawHorizontalLine ((int) yOf (0.0f), r.getX(), r.getRight());
        g.setColour (col::muted); g.setFont (font (8.5f));
        g.drawText ("+12", (int) r.getX() + 3, (int) r.getY() + 2, 30, 10, juce::Justification::left);
        g.drawText ("-12", (int) r.getX() + 3, (int) r.getBottom() - 12, 30, 10, juce::Justification::left);
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 200.0);
            const float db = MasterEQ::responseDb (settings, f, (float) proc.getSr());
            if (i == 0) p.startNewSubPath (xOf (f), yOf (db)); else p.lineTo (xOf (f), yOf (db));
        }
        g.setColour (col::white); g.strokePath (p, juce::PathStrokeType (1.4f));
        for (int b = 0; b < kEqBands; ++b)
        {
            const auto& bd = settings.bands[(size_t) b];
            const float x = xOf (bd.freq), y = yOf (MasterEQ::responseDb (settings, bd.freq, (float) proc.getSr()));
            g.setColour (MasterEQ::bandActive (bd) ? col::white : col::muted);
            g.drawEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f);
        }
    }
private:
    PolooyxReleaseProcessor& proc;
    MasterEQ::Settings settings;
    double lastHash = -1;
};

class EqPage : public juce::Component
{
public:
    EqPage (PolooyxReleaseProcessor& p) : curve (p)
    {
        auto& s = p.apvts;
        enable = std::make_unique<Toggle> (s, id::eqOn, "ON"); addAndMakeVisible (*enable);
        addAndMakeVisible (curve);
        for (int b = 0; b < kEqBands; ++b)
        {
            auto& c = cols[(size_t) b];
            c.type = std::make_unique<Choice> (s, eqId (b, "Type"), "");
            c.freq = std::make_unique<Knob> (s, eqId (b, "Freq"), "FREQ");
            c.gain = std::make_unique<Knob> (s, eqId (b, "Gain"), "GAIN");
            c.q    = std::make_unique<Knob> (s, eqId (b, "Q"), "Q");
            for (juce::Component* x : { (juce::Component*) c.type.get(), (juce::Component*) c.freq.get(), (juce::Component*) c.gain.get(), (juce::Component*) c.q.get() }) addAndMakeVisible (x);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (20);
        enable->setBounds (head.removeFromRight (40).reduced (0, 2));
        r.removeFromTop (8);
        curve.setBounds (r.removeFromTop (96));
        r.removeFromTop (6);
        const int w = r.getWidth() / kEqBands;
        for (int b = 0; b < kEqBands; ++b)
        {
            auto c = r.removeFromLeft (w).reduced (4, 0);
            c.removeFromTop (14);
            cols[(size_t) b].type->setBounds (c.removeFromTop (22));
            c.removeFromTop (4);
            auto k = c.removeFromTop (juce::jmin (c.getHeight(), 78));
            const int kw = k.getWidth() / 3;
            cols[(size_t) b].freq->setBounds (k.removeFromLeft (kw));
            cols[(size_t) b].gain->setBounds (k.removeFromLeft (kw));
            cols[(size_t) b].q->setBounds (k);
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (20);
        g.setColour (col::white); g.setFont (font (10.5f, W::SemiBold, 0.18f));
        g.drawText ("MASTER EQ", head, juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (10.0f));
        g.drawText ("8 bands  -  TONE macro adds a see-saw tilt on top", head.withTrimmedLeft (110).withTrimmedRight (50), juce::Justification::centredLeft);
        r.removeFromTop (8 + 96 + 6);
        const int w = r.getWidth() / kEqBands;
        for (int b = 0; b < kEqBands; ++b)
        {
            g.setColour (col::text2); g.setFont (font (9.5f, W::SemiBold, 0.14f));
            g.drawText (eqBandName (b), r.getX() + b * w + 4, r.getY(), w - 8, 12, juce::Justification::centredLeft);
        }
    }
    void tick() { curve.update(); }
private:
    struct Col { std::unique_ptr<Choice> type; std::unique_ptr<Knob> freq, gain, q; };
    std::array<Col, kEqBands> cols;
    std::unique_ptr<Toggle> enable;
    EqCurve curve;
};

// generic multi-column band page (dynamic EQ / multiband)
class BandPage : public juce::Component
{
public:
    std::unique_ptr<Toggle> enable;
    juce::String title, subtitle;
    std::vector<std::vector<std::unique_ptr<Knob>>> bands;
    std::vector<juce::String> bandNames;
    std::vector<std::unique_ptr<juce::Component>> topRow;
    std::function<float (int)> grOf;
    int perRow = 4;

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (20);
        if (enable) enable->setBounds (head.removeFromRight (40).reduced (0, 2));
        r.removeFromTop (8);
        if (! topRow.empty())
        {
            auto t = r.removeFromTop (78);
            int x = t.getX();
            for (auto& c : topRow) { const bool k = dynamic_cast<Knob*> (c.get()) != nullptr; c->setBounds (x, t.getY() + (k ? 0 : 22), k ? 70 : 110, k ? 76 : 26); x += k ? 76 : 118; }
            r.removeFromTop (6);
        }
        const int w = r.getWidth() / juce::jmax (1, (int) bands.size());
        for (size_t b = 0; b < bands.size(); ++b)
        {
            auto c = r.removeFromLeft (w).reduced (6, 0);
            c.removeFromTop (16);
            c.removeFromBottom (22);
            const int kw = c.getWidth() / perRow, kh = juce::jmin (78, c.getHeight() / ((int) std::ceil (bands[b].size() / (double) perRow)));
            for (size_t k = 0; k < bands[b].size(); ++k)
                bands[b][k]->setBounds (c.getX() + (int) (k % (size_t) perRow) * kw, c.getY() + (int) (k / (size_t) perRow) * kh, kw, kh);
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (20);
        g.setColour (col::white); g.setFont (font (10.5f, W::SemiBold, 0.18f)); g.drawText (title, head, juce::Justification::centredLeft);
        g.setColour (col::muted); g.setFont (font (10.0f)); g.drawText (subtitle, head.withTrimmedLeft (150).withTrimmedRight (50), juce::Justification::centredLeft);
        r.removeFromTop (8);
        if (! topRow.empty()) r.removeFromTop (84);
        const int w = r.getWidth() / juce::jmax (1, (int) bands.size());
        for (size_t b = 0; b < bands.size(); ++b)
        {
            auto c = r.removeFromLeft (w).reduced (6, 0);
            g.setColour (col::text2); g.setFont (font (9.5f, W::SemiBold, 0.14f));
            g.drawText (bandNames[b], c.removeFromTop (14), juce::Justification::centredLeft);
            if (grOf) drawGr (g, c.removeFromBottom (18).toFloat(), "GR", grOf ((int) b), 12.0f);
            g.setColour (col::border); g.drawVerticalLine (c.getRight() + 5, (float) c.getY(), (float) c.getBottom());
        }
    }
};

// ---------------------------------------------------------------- LAB measurement page
class LabPage : public juce::Component
{
public:
    explicit LabPage (PolooyxReleaseProcessor& p) : proc (p) {}
    void set (const PolooyxReleaseProcessor::Results& r) { res = r; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        const int cw = (r.getWidth() - 36) / 3;
        auto c1 = r.removeFromLeft (cw); r.removeFromLeft (18);
        auto c2 = r.removeFromLeft (cw); r.removeFromLeft (18);
        auto c3 = r;
        auto& m = proc.meters;
        auto row = [&] (juce::Rectangle<int>& col, const juce::String& k, const juce::String& v)
        {
            auto rr = col.removeFromTop (17);
            g.setColour (col::muted); g.setFont (font (9.5f, W::Medium, 0.08f)); g.drawText (k, rr, juce::Justification::centredLeft);
            g.setColour (col::white); g.setFont (font (11.0f)); g.drawText (v, rr, juce::Justification::centredRight);
            g.setColour (col::border); g.fillRect (rr.getX(), rr.getBottom() - 1, rr.getWidth(), 1);
        };
        sectionTitle (g, c1.removeFromTop (18), "LIVE MEASUREMENT", "post");
        c1.removeFromTop (4);
        row (c1, "LUFS MOMENTARY", fmtDb (m.lufsM, 1));
        row (c1, "LUFS SHORT-TERM", fmtDb (m.lufsS, 1));
        row (c1, "LUFS INTEGRATED", fmtDb (m.lufsI, 1));
        row (c1, "LOUDNESS RANGE", juce::String (m.lra.load(), 1) + " LU");
        row (c1, "TRUE PEAK (MAX)", fmtDb (m.outTpMax, 2, "dBTP"));
        row (c1, "SAMPLE PEAK", fmtDb (m.outPeak, 2, "dBFS"));
        row (c1, "RMS", fmtDb (m.rms, 1, "dBFS"));
        row (c1, "CREST", juce::String (m.crest.load(), 1) + " dB");
        row (c1, "CORRELATION", juce::String (m.corr.load(), 2));
        row (c1, "MID / SIDE", fmtDb (m.midDb, 1) + " / " + fmtDb (m.sideDb, 1));
        row (c1, "BALANCE L-R", juce::String (m.balance.load(), 2) + " dB");
        row (c1, "INPUT LUFS-I / TP", fmtDb (m.inLufsI, 1) + " / " + fmtDb (m.inTpMax, 1));

        sectionTitle (g, c2.removeFromTop (18), "GAIN STAGING", "last block");
        c2.removeFromTop (4);
        for (int b = 0; b < kDeqBands; ++b) drawGr (g, c2.removeFromTop (17).toFloat(), juce::String ("DEQ ") + juce::String (b + 1), m.deqGr[(size_t) b], 6.0f);
        static const char* mbShort[] = { "MB SUB", "MB LOW", "MB L-MID", "MB H-MID", "MB HIGH" };
        for (int b = 0; b < kMbBands; ++b) drawGr (g, c2.removeFromTop (17).toFloat(), mbShort[b], m.mbGr[(size_t) b], 12.0f);
        drawGr (g, c2.removeFromTop (17).toFloat(), "GLUE", m.glueGr, 12.0f);
        drawGr (g, c2.removeFromTop (17).toFloat(), "CLIP", m.clipDb, 6.0f);
        drawGr (g, c2.removeFromTop (17).toFloat(), "LIMITER", m.limGr, 12.0f);
        c2.removeFromTop (6);
        row (c2, "OVERSAMPLING", juce::String (m.osFactor.load()) + "x");
        row (c2, "LATENCY", juce::String (proc.getLatencySamples()) + " smp  (" + juce::String (proc.getLatencySamples() * 1000.0 / proc.getSr(), 1) + " ms)");
        row (c2, "LEVEL MATCH OFFSET", juce::String (m.levelMatchDb.load(), 1) + " dB");
        row (c2, "CPU", juce::String (m.cpu.load() * 100.0f, 1) + " %");

        sectionTitle (g, c3.removeFromTop (18), "LAST ANALYSIS", res.hasAnalysis ? juce::String (res.analysis.seconds, 0) + " s" : "none");
        c3.removeFromTop (4);
        if (! res.hasAnalysis) { g.setColour (col::muted); g.setFont (font (11.0f)); g.drawText ("Run ANALYZE POLO.", c3, juce::Justification::topLeft); return; }
        const auto& a = res.analysis;
        for (int b = 0; b < kAnBands; ++b)
            row (c3, anBandName (b), juce::String (a.bandShare[(size_t) b], 1) + "%   " + (a.bandDev[(size_t) b] > 0 ? "+" : "") + juce::String (a.bandDev[(size_t) b], 1) + " dB");
        row (c3, "TILT 100 Hz-8 kHz", juce::String (a.tiltDbPerOct, 2) + " dB/oct");
        row (c3, "LOW-MID / PRESENCE", "+" + juce::String (a.promLowMid, 1) + " @ " + juce::String ((int) a.lowMidPeakHz) + "  /  +" + juce::String (a.promPresence, 1) + " @ " + juce::String ((int) a.presencePeakHz));
        row (c3, "CENTROID", juce::String ((int) a.centroidHz) + " Hz");
        row (c3, "PLR / LRA", juce::String (a.plr, 1) + " dB / " + juce::String (a.lra, 1) + " LU");
        row (c3, "LOW SIDE (<120 Hz)", juce::String (a.lowSideDb, 1) + " dB");
    }
private:
    PolooyxReleaseProcessor& proc;
    PolooyxReleaseProcessor::Results res;
};
} // namespace prl::ui
