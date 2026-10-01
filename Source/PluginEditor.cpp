#include "PluginEditor.h"

using namespace prl;
using namespace prl::ui;

namespace
{
const char* kTabNames[] = { "POLO", "MACROS", "EQ", "DYN EQ", "MULTIBAND", "GLUE / PUNCH", "COLOR / CHARACTER", "STEREO", "CLIP / LIMIT", "I/O / QUALITY", "MEASURE" };
constexpr int kNumTabs = 11, kTabPolo = 0, kTabMacros = 1, kTabMeasure = 10;
using KP = std::initializer_list<std::pair<const char*, const char*>>;

juce::String mmss (float s) { const int t = (int) s; return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); }
}

PolooyxReleaseEditor::PolooyxReleaseEditor (PolooyxReleaseProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.painter = [this] (juce::Graphics& g) { paintContent (g); };
    auto& s = proc.apvts;

    // ---------------- header
    const char* views[] = { "BEGINNER", "PRO", "LAB" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = viewBtns.add (new juce::TextButton (views[i]));
        b->getProperties().set ("fontSize", 10.0f);
        b->onClick = [this, i] { setView (i); };
        content.addAndMakeVisible (b);
    }
    for (auto* b : { &presetBtn, &prevBtn, &nextBtn, &saveBtn, &aBtn, &bBtn, &undoBtn, &redoBtn, &settingsBtn })
    { b->getProperties().set ("fontSize", 10.5f); content.addAndMakeVisible (*b); }
    presetBtn.getProperties().set ("fontSize", 12.0f);
    presetBtn.onClick = [this] { openPresetBrowser(); };
    presetBtn.setTooltip ("Browse and search presets");
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    aBtn.onClick = [this] { proc.switchAB (0); };
    bBtn.onClick = [this] { proc.switchAB (1); };
    aBtn.setTooltip ("Settings slot A"); bBtn.setTooltip ("Settings slot B - compare two complete settings");
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    settingsBtn.onClick = [this] { openSettings(); };

    // ---------------- analyzer
    content.addAndMakeVisible (spec);
    content.addAndMakeVisible (stereo);
    const char* modes[] = { "PRE", "POST", "BOTH" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = specModeBtns.add (new juce::TextButton (modes[i]));
        b->getProperties().set ("fontSize", 9.5f); b->getProperties().set ("flat", true);
        b->onClick = [this, i] { proc.uiState.setProperty ("specMode", i, nullptr); };
        content.addAndMakeVisible (b);
    }
    for (auto* b : { &freezeBtn, &holdBtn, &rangeBtn, &smoothBtn })
    { b->getProperties().set ("fontSize", 9.5f); b->getProperties().set ("flat", true); content.addAndMakeVisible (*b); }
    freezeBtn.setClickingTogglesState (true);
    freezeBtn.onClick = [this] { spec.pre.setFrozen (freezeBtn.getToggleState()); spec.post.setFrozen (freezeBtn.getToggleState()); spec.frozen = freezeBtn.getToggleState(); };
    holdBtn.setClickingTogglesState (true);
    holdBtn.onClick = [this] { proc.uiState.setProperty ("peakHold", holdBtn.getToggleState(), nullptr); };
    rangeBtn.onClick = [this] { const int r = (int) proc.uiState.getProperty ("specRange", 90); proc.uiState.setProperty ("specRange", r == 60 ? 90 : r == 90 ? 120 : 60, nullptr); };
    smoothBtn.onClick = [this] { proc.uiState.setProperty ("specSmooth", ((int) proc.uiState.getProperty ("specSmooth", 1) + 1) % 3, nullptr); };
    holdBtn.setToggleState ((bool) proc.uiState.getProperty ("peakHold", false), juce::dontSendNotification);

    // ---------------- actions
    analyzeBtn.getProperties().set ("fontSize", 12.5f); analyzeBtn.getProperties().set ("bold", true);
    finalBtn.getProperties().set ("fontSize", 10.5f);
    poloBtn.getProperties().set ("primary", true); poloBtn.getProperties().set ("bold", true); poloBtn.getProperties().set ("fontSize", 17.0f);
    for (auto* b : { &analyzeBtn, &finalBtn, &poloBtn }) content.addAndMakeVisible (*b);
    analyzeBtn.setTooltip ("Listen to the incoming mix and measure loudness, peaks, spectrum, low end, stereo and dynamics");
    poloBtn.setTooltip ("Analyse, apply the smallest corrections that solve what was measured, render, verify and show every change");
    finalBtn.setTooltip ("Render the captured mix through the current settings and check it is ready to export");
    analyzeBtn.onClick = [this]
    {
        if (proc.isBusy()) { proc.cancelJob(); return; }
        if (proc.isListening()) { proc.stopListeningAndRun(); return; }
        proc.startListening (PolooyxReleaseProcessor::Action::Analyze);
    };
    poloBtn.onClick = [this]
    {
        if (proc.isBusy()) return;
        if (proc.isListening()) { proc.stopListeningAndRun(); return; }
        if (proc.hasCapture()) proc.runOnCapture (PolooyxReleaseProcessor::Action::PoloIt);
        else proc.startListening (PolooyxReleaseProcessor::Action::PoloIt);
    };
    finalBtn.onClick = [this]
    {
        if (proc.isBusy() || proc.isListening()) return;
        if (proc.hasCapture()) proc.runOnCapture (PolooyxReleaseProcessor::Action::FinalCheck);
        else proc.startListening (PolooyxReleaseProcessor::Action::FinalCheck);
    };
    modeChoice = std::make_unique<Choice> (s, id::poloMode, "POLO IT MODE");
    targetChoice = std::make_unique<Choice> (s, id::target, "LOUDNESS TARGET");
    listenSeg = std::make_unique<Segmented> (s, id::listen, juce::StringArray { "PROCESSED", "ORIGINAL" });
    levelMatchBtn = std::make_unique<Toggle> (s, id::levelMatch, "LEVEL MATCH");
    levelMatchBtn->setTooltip ("ORIGINAL is played at the processed loudness, so you compare sound, not volume");
    for (juce::Component* c : { (juce::Component*) modeChoice.get(), (juce::Component*) targetChoice.get(), (juce::Component*) listenSeg.get(), (juce::Component*) levelMatchBtn.get() })
        content.addAndMakeVisible (c);

    // ---------------- beginner panels
    for (juce::Component* c : { (juce::Component*) &diag, (juce::Component*) &next, (juce::Component*) &result })
        content.addAndMakeVisible (c);
    content.addChildComponent (overlay);

    // ---------------- macros
    const char* mIds[] = { id::tone, id::glue, id::color, id::width, id::punch, id::loudness };
    const char* mNames[] = { "TONE", "GLUE", "COLOR", "WIDTH", "PUNCH", "LOUDNESS" };
    const char* mTips[] = { "Broad tonal shaping: a +-2 dB see-saw tilt (never lifts both ends)",
                            "Bus compression / cohesion",
                            "Harmonic character (oversampled)",
                            "Stereo image. Widening is reduced automatically when correlation is low; the low end is never widened",
                            "Transient impact and low-end punch (kick, not 808 sustain)",
                            "Drives the clipper and true-peak limiter. The ceiling never moves - it is not an output-gain knob" };
    for (int i = 0; i < 6; ++i)
    {
        macros.push_back (std::make_unique<Knob> (s, mIds[i], mNames[i], true));
        macros.back()->slider.setTooltip (mTips[i]);
        content.addAndMakeVisible (*macros.back());
    }

    // ---------------- pro / lab pages
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto* b = tabBtns.add (new juce::TextButton (kTabNames[i]));
        b->getProperties().set ("fontSize", 9.5f); b->getProperties().set ("flat", true);
        b->onClick = [this, i] { setProTab (i); };
        content.addChildComponent (b);
    }
    eqPage = std::make_unique<EqPage> (proc);

    deqPage = std::make_unique<BandPage>();
    deqPage->title = "DYNAMIC EQ"; deqPage->subtitle = "cut-only, acts only while a band exceeds its threshold";
    deqPage->enable = std::make_unique<Toggle> (s, id::deqOn, "ON"); deqPage->addAndMakeVisible (*deqPage->enable);
    for (int b = 0; b < kDeqBands; ++b)
    {
        deqPage->bandNames.push_back (deqBandName (b));
        deqPage->bands.emplace_back();
        const char* w[] = { "Freq", "Q", "Thr", "Ratio", "Att", "Rel", "Range", "Mix" };
        const char* n[] = { "FREQ", "Q", "THRESH", "RATIO", "ATTACK", "RELEASE", "RANGE", "MIX" };
        for (int k = 0; k < 8; ++k) { deqPage->bands.back().push_back (std::make_unique<Knob> (s, deqId (b, w[k]), n[k])); deqPage->addAndMakeVisible (*deqPage->bands.back().back()); }
    }
    deqPage->grOf = [this] (int b) { return proc.meters.deqGr[(size_t) b].load(); };

    mbPage = std::make_unique<BandPage>();
    mbPage->title = "MULTIBAND"; mbPage->subtitle = "5 bands, LR4 crossovers - bypassed exactly until a band gets a threshold, makeup or AUTO GAIN";
    mbPage->enable = std::make_unique<Toggle> (s, id::mbOn, "ON"); mbPage->addAndMakeVisible (*mbPage->enable);
    mbPage->perRow = 3;
    for (int i = 0; i < kMbBands - 1; ++i) { mbPage->topRow.push_back (std::make_unique<Knob> (s, xoId (i), "XOVER " + juce::String (i + 1))); mbPage->addAndMakeVisible (*mbPage->topRow.back()); }
    mbPage->topRow.push_back (std::make_unique<Toggle> (s, id::mbAutoGain, "AUTO GAIN")); mbPage->addAndMakeVisible (*mbPage->topRow.back());
    for (int b = 0; b < kMbBands; ++b)
    {
        mbPage->bandNames.push_back (mbBandName (b));
        mbPage->bands.emplace_back();
        const char* w[] = { "Thr", "Ratio", "Att", "Rel", "Gain", "Mix" };
        const char* n[] = { "THRESH", "RATIO", "ATTACK", "RELEASE", "MAKEUP", "MIX" };
        for (int k = 0; k < 6; ++k) { mbPage->bands.back().push_back (std::make_unique<Knob> (s, mbId (b, w[k]), n[k])); mbPage->addAndMakeVisible (*mbPage->bands.back().back()); }
    }
    mbPage->grOf = [this] (int b) { return proc.meters.mbGr[(size_t) b].load(); };

    auto knob = [&s] (const char* pid, const char* name) { return std::make_unique<Knob> (s, pid, name); };
    dynPage = std::make_unique<ModulePage>();
    {
        auto& g = dynPage->add ("GLUE COMPRESSOR", &s, id::glueOn);
        g.weight = 1.45f;
        for (auto [pid, n] : KP { { id::glueThr, "THRESH" }, { id::glueRatio, "RATIO" }, { id::glueAtt, "ATTACK" }, { id::glueRel, "RELEASE" },
                               { id::glueKnee, "KNEE" }, { id::glueMakeup, "MAKEUP" }, { id::glueMix, "MIX" } })
            dynPage->put (g, knob (pid, n));
        g.extraH = 18; g.extraPaint = [this] (juce::Graphics& gg, juce::Rectangle<int> r) { drawGr (gg, r.toFloat().withWidth (360.0f), "GR", proc.meters.glueGr.load()); };
        g.note = "RMS-style detector, stereo-linked. GLUE macro lowers the threshold (-14 dB), raises ratio (+2) and adds makeup.";
        auto& pu = dynPage->add ("PUNCH", &s, id::punchOn);
        pu.weight = 0.85f;
        for (auto [pid, n] : KP { { id::punchTrans, "TRANSIENT" }, { id::punchBody, "BODY" }, { id::punchImpact, "IMPACT" }, { id::punchMix, "MIX" } })
            dynPage->put (pu, knob (pid, n));
        pu.note = "IMPACT works on the low band's own attacks (kick hits), leaving long 808 tails alone. Gains are bounded.";
    }
    tonePage = std::make_unique<ModulePage>();
    {
        auto& c = tonePage->add ("HARMONIC COLOR", &s, id::colorOn);
        tonePage->put (c, std::make_unique<Choice> (s, id::colorMode, "MODE"));
        for (auto [pid, n] : KP { { id::colorDrive, "DRIVE" }, { id::colorTone, "TONE" }, { id::colorMix, "MIX" }, { id::colorOut, "OUTPUT" } })
            tonePage->put (c, knob (pid, n));
        c.note = "Oversampled. Unity small-signal gain: DRIVE changes harmonics, not level. CLEAN adds no harmonics.";
        auto& h = tonePage->add ("POLOOYX CHARACTER", &s, id::charOn);
        for (auto [pid, n] : KP { { id::aura, "AURA" }, { id::grit, "GRIT" }, { id::glass, "GLASS" }, { id::depth, "DEPTH" }, { id::space, "SPACE" } })
            tonePage->put (h, knob (pid, n));
        h.note = "Subtle, master-safe. DEPTH and SPACE live only in the side channel - they cancel in mono.";
    }
    stereoPage = std::make_unique<ModulePage>();
    {
        auto& g = stereoPage->add ("STEREO / M-S", &s, id::stOn);
        g.weight = 1.6f;
        for (auto [pid, n] : KP { { id::stWidth, "WIDTH" }, { id::stLowWidth, "LOW WIDTH" }, { id::stHighWidth, "HIGH WIDTH" }, { id::stXoLow, "LOW XOVER" },
                               { id::stXoHigh, "HIGH XOVER" }, { id::stMid, "MID" }, { id::stSide, "SIDE" } })
            stereoPage->put (g, knob (pid, n));
        g.note = "Per-band side gain (0% = mono, 100% = untouched). Use MONO in the sidebar to check compatibility.";
        auto& m = stereoPage->add ("LOW FREQUENCY MONO", &s, id::monoBass);
        m.weight = 0.55f;
        stereoPage->put (m, knob (id::monoFreq, "MONO BELOW"));
        m.note = "Removes side content below the crossover (LR4) - the mid channel is untouched.";
    }
    outPage = std::make_unique<ModulePage>();
    {
        auto& c = outPage->add ("SOFT CLIPPER", &s, id::clipOn);
        c.weight = 0.9f;
        outPage->put (c, std::make_unique<Choice> (s, id::clipMode, "MODE"));
        for (auto [pid, n] : KP { { id::clipDrive, "DRIVE" }, { id::clipCeiling, "CEILING" }, { id::clipMix, "MIX" } }) outPage->put (c, knob (pid, n));
        c.extraH = 18; c.extraPaint = [this] (juce::Graphics& gg, juce::Rectangle<int> r) { drawGr (gg, r.toFloat().withWidth (300.0f), "CLIP", proc.meters.clipDb.load(), 6.0f); };
        c.note = "Oversampled. DRIVE 0 dB = idle. Shaves the sharpest peaks so the limiter works less.";
        auto& l = outPage->add ("TRUE-PEAK LIMITER", &s, id::limOn);
        l.weight = 1.3f;
        outPage->put (l, std::make_unique<Choice> (s, id::limStyle, "STYLE"));
        for (auto [pid, n] : KP { { id::limThresh, "THRESHOLD" }, { id::limCeiling, "CEILING" }, { id::limRelease, "RELEASE" }, { id::limLookahead, "LOOKAHEAD" }, { id::limMix, "MIX" } })
            outPage->put (l, knob (pid, n));
        l.extraH = 18; l.extraPaint = [this] (juce::Graphics& gg, juce::Rectangle<int> r) { drawGr (gg, r.toFloat().withWidth (360.0f), "GR", proc.meters.limGr.load()); };
        l.note = "4x true-peak detection, lookahead, constant latency. LOUDNESS macro drives into it; the ceiling never moves.";
    }
    ioPage = std::make_unique<ModulePage>();
    {
        auto& i = ioPage->add ("INPUT / CLEANUP");
        ioPage->put (i, knob (id::inGain, "INPUT"));
        ioPage->put (i, std::make_unique<Toggle> (s, id::dcOn, "DC FILTER"));
        ioPage->put (i, std::make_unique<Toggle> (s, id::subOn, "SUBSONIC"));
        ioPage->put (i, knob (id::subFreq, "SUB FREQ"));
        auto& o = ioPage->add ("OUTPUT");
        o.weight = 0.6f;
        ioPage->put (o, knob (id::outGain, "OUTPUT"));
        ioPage->put (o, knob (id::dryWet, "DRY/WET"));
        auto& q = ioPage->add ("QUALITY / TARGET");
        q.weight = 1.2f;
        ioPage->put (q, std::make_unique<Choice> (s, id::quality, "QUALITY"));
        ioPage->put (q, std::make_unique<Choice> (s, id::oversampling, "OVERSAMPLING"));
        ioPage->put (q, std::make_unique<Choice> (s, id::target, "TARGET"));
        ioPage->put (q, knob (id::customTarget, "CUSTOM LUFS"));
        q.note = "QUALITY sets oversampling when OVERSAMPLING is AUTO: ECO off, NORMAL 2x, HIGH 4x, MAX 8x (halved at 88.2/96 kHz).";
    }
    labPage = std::make_unique<LabPage> (proc);
    tabPages = { nullptr, nullptr, eqPage.get(), deqPage.get(), mbPage.get(), dynPage.get(), tonePage.get(), stereoPage.get(), outPage.get(), ioPage.get(), labPage.get() };
    for (auto* pg : tabPages) if (pg != nullptr) content.addChildComponent (pg);

    // ---------------- sidebar
    content.addAndMakeVisible (inMeter); content.addAndMakeVisible (outMeter);
    inKnob = std::make_unique<Knob> (s, id::inGain, "INPUT");
    outKnob = std::make_unique<Knob> (s, id::outGain, "OUTPUT");
    wetKnob = std::make_unique<Knob> (s, id::dryWet, "DRY/WET");
    monitorSeg = std::make_unique<Segmented> (s, id::monitor, juce::StringArray { "FULL", "MONO", "SMALL" });
    monitorSeg->setTooltip ("Monitoring only: MONO sums to mono, SMALL is a filtered small-speaker approximation (not a speaker model). Switch back to FULL before exporting.");
    bypassBtn = std::make_unique<Toggle> (s, id::bypass, "BYPASS");
    resetBtn.getProperties().set ("fontSize", 10.0f);
    resetBtn.onClick = [this] { proc.resetPeaks(); };
    for (juce::Component* c : { (juce::Component*) inKnob.get(), (juce::Component*) outKnob.get(), (juce::Component*) wetKnob.get(),
                                (juce::Component*) monitorSeg.get(), (juce::Component*) bypassBtn.get(), (juce::Component*) &resetBtn })
        content.addAndMakeVisible (c);

    addMouseListener (this, true);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (960, 600, 2560, 1600);
    setSize (kW, kH);
    const auto v = proc.uiState.getProperty ("view", "BEGINNER").toString();
    setView (v == "PRO" ? 1 : v == "LAB" ? 2 : 0);
    startTimerHz (30);
}

PolooyxReleaseEditor::~PolooyxReleaseEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PolooyxReleaseEditor::mouseDown (const juce::MouseEvent&) { proc.undo.beginNewTransaction(); }

void PolooyxReleaseEditor::resized()
{
    content.setBounds (0, 0, kW, kH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / kW, (float) getHeight() / kH));
    layout();
}

void PolooyxReleaseEditor::setView (int v)
{
    view = v;
    proc.uiState.setProperty ("view", v == 1 ? "PRO" : v == 2 ? "LAB" : "BEGINNER", nullptr);
    for (int i = 0; i < 3; ++i) viewBtns[i]->setToggleState (i == v, juce::dontSendNotification);
    if (v == 2) proTab = kTabMeasure;
    else if (v == 1 && (proTab == kTabMeasure || proTab < 0)) proTab = 2;
    layout();
}

void PolooyxReleaseEditor::setProTab (int t) { proTab = t; layout(); }

void PolooyxReleaseEditor::layout()
{
    // header
    for (int i = 0; i < 3; ++i) viewBtns[i]->setBounds (244 + i * 80, 20, 78, 24);
    prevBtn.setBounds (588, 14, 26, 28);
    presetBtn.setBounds (616, 14, 236, 28);
    nextBtn.setBounds (854, 14, 26, 28);
    saveBtn.setBounds (888, 14, 58, 28);
    aBtn.setBounds (954, 14, 30, 28); bBtn.setBounds (986, 14, 30, 28);
    undoBtn.setBounds (1024, 14, 60, 28); redoBtn.setBounds (1086, 14, 60, 28);
    settingsBtn.setBounds (1154, 14, 102, 28);

    // analyzer
    for (int i = 0; i < 3; ++i) specModeBtns[i]->setBounds (24 + i * 46, 78, 44, 18);
    smoothBtn.setBounds (712, 78, 96, 18); rangeBtn.setBounds (620, 78, 88, 18);
    holdBtn.setBounds (540, 78, 76, 18); freezeBtn.setBounds (474, 78, 62, 18);
    spec.setBounds (24, 100, 784, 234);
    stereo.setBounds (820, 78, 240, 256);

    // actions
    analyzeBtn.setBounds (24, 352, 196, 54);
    finalBtn.setBounds (228, 352, 120, 54);
    poloBtn.setBounds (372, 348, 312, 62);
    modeChoice->setBounds (700, 346, 136, 38);
    targetChoice->setBounds (700, 388, 136, 38);
    listenSeg->setBounds (852, 364, 208, 26);
    levelMatchBtn->setBounds (852, 396, 112, 24);

    // middle + macros / pages
    const bool beginner = view == 0;
    const bool showPolo = beginner || proTab == kTabPolo;
    const bool showMacros = beginner || proTab == kTabMacros;
    const auto mid = beginner ? juce::Rectangle<int> (24, 446, 1036, 186) : juce::Rectangle<int> (24, 478, 1036, 312);
    diag.setVisible (showPolo); next.setVisible (showPolo); result.setVisible (showPolo);
    if (showPolo)
    {
        auto r = mid;
        diag.setBounds (r.removeFromLeft (404)); r.removeFromLeft (20);
        next.setBounds (r.removeFromLeft (300)); r.removeFromLeft (20);
        result.setBounds (r);
    }
    for (auto& m : macros) m->setVisible (showMacros);
    if (showMacros)
    {
        const auto mr = beginner ? juce::Rectangle<int> (24, 644, 1036, 146) : juce::Rectangle<int> (24, 520, 1036, 200);
        const int w = mr.getWidth() / 6;
        for (int i = 0; i < 6; ++i) macros[(size_t) i]->setBounds (mr.getX() + i * w + 14, mr.getY(), w - 28, mr.getHeight());
    }
    int x = 24;
    for (int i = 0; i < kNumTabs; ++i)
    {
        auto* b = tabBtns[i];
        const bool vis = ! beginner && (i != kTabMeasure || view == 2);
        b->setVisible (vis);
        b->setToggleState (i == proTab, juce::dontSendNotification);
        if (vis) { const int w = (int) textWidth (font (9.5f, W::Medium, 0.06f), kTabNames[i]) + 22; b->setBounds (x, 446, w, 22); x += w + 2; }
    }
    for (int i = 0; i < kNumTabs; ++i)
        if (auto* pg = tabPages[(size_t) i])
        {
            pg->setVisible (! beginner && i == proTab);
            if (pg->isVisible()) pg->setBounds (mid);
        }
    overlay.setBounds (beginner ? mid.expanded (0, 4) : juce::Rectangle<int> (24, 474, 1036, 318));

    // sidebar
    inMeter.setBounds (1084, 290, 20, 186);
    outMeter.setBounds (1110, 290, 20, 186);
    inKnob->setBounds (1080, 494, 58, 74); outKnob->setBounds (1138, 494, 58, 74); wetKnob->setBounds (1196, 494, 60, 74);
    monitorSeg->setBounds (1082, 598, 174, 24);
    bypassBtn->setBounds (1082, 636, 84, 26);
    resetBtn.setBounds (1172, 636, 84, 26);
    content.repaint();
}

void PolooyxReleaseEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // header
    g.setColour (col::bg2); g.fillRect (0, 0, kW, 62);
    g.setColour (col::border); g.drawHorizontalLine (62, 0.0f, (float) kW);
    g.setColour (col::white); g.setFont (font (21.0f, W::SemiBold, 0.34f));
    g.drawText ("POLOOYX", 28, 11, 220, 26, juce::Justification::centredLeft);
    g.setColour (col::text2); g.setFont (font (9.5f, W::Medium, 0.42f));
    g.drawText ("RELEASE ENGINE", 29, 37, 220, 14, juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (9.5f, W::Regular, 0.06f));
    g.drawText (statusText, 588, 45, 668, 14, juce::Justification::centredRight);

    // separators
    g.setColour (col::border);
    g.drawHorizontalLine (338, 24.0f, 1060.0f);
    g.drawHorizontalLine (436, 24.0f, 1060.0f);
    if (view == 0) g.drawHorizontalLine (636, 24.0f, 1060.0f);
    g.drawVerticalLine (1070, 70.0f, 792.0f);

    // compare block caption + level match readout
    g.setColour (col::white); g.setFont (font (10.0f, W::SemiBold, 0.18f));
    g.drawText ("A / B COMPARE", 852, 346, 208, 14, juce::Justification::centredLeft);
    g.setColour (col::muted); g.setFont (font (10.0f));
    const float lm = proc.meters.levelMatchDb.load();
    g.drawText (proc.param (id::levelMatch) > 0.5f ? "ORIGINAL " + juce::String (lm >= 0 ? "+" : "") + juce::String (lm, 1) + " dB" : "unmatched",
                968, 396, 92, 24, juce::Justification::centredRight);

    // sidebar readouts
    auto& m = proc.meters;
    g.setColour (col::white); g.setFont (font (10.5f, W::SemiBold, 0.18f));
    g.drawText ("LOUDNESS", 1082, 76, 174, 14, juce::Justification::centredLeft);
    const float target = targetLufs ((Target) (int) proc.param (id::target), proc.param (id::customTarget));
    Readout::draw (g, { 1082, 96, 174, 40 }, "LUFS INTEGRATED", fmtDb (m.lufsI, 1), 26.0f, true);
    Readout::draw (g, { 1082, 140, 87, 30 }, "SHORT-TERM", fmtDb (m.lufsS, 1), 14.0f);
    Readout::draw (g, { 1169, 140, 87, 30 }, "MOMENTARY", fmtDb (m.lufsM, 1), 14.0f);
    Readout::draw (g, { 1082, 174, 87, 30 }, "LRA", juce::String (m.lra.load(), 1) + " LU", 14.0f);
    Readout::draw (g, { 1169, 174, 87, 30 }, "TARGET", juce::String (target, 1), 14.0f);
    Readout::draw (g, { 1082, 210, 174, 40 }, "TRUE PEAK  (4x est.)", fmtDb (m.outTpMax, 1, "dBTP"), 22.0f, true);
    Readout::draw (g, { 1082, 252, 87, 30 }, "RMS", fmtDb (m.rms, 1), 14.0f);
    Readout::draw (g, { 1169, 252, 87, 30 }, "CREST", juce::String (m.crest.load(), 1) + " dB", 14.0f);
    // GR strip next to meters
    auto gr = juce::Rectangle<float> (1138, 292, 118, 150);
    using GT = std::tuple<const char*, float, float>;
    for (auto [lab, v, rng] : std::initializer_list<GT> { GT { "LIMIT", m.limGr.load(), 12.0f }, GT { "GLUE", m.glueGr.load(), 12.0f }, GT { "CLIP", m.clipDb.load(), 6.0f } })
    {
        g.setColour (col::muted); g.setFont (font (8.5f, W::Medium, 0.1f));
        g.drawText (lab, gr.removeFromTop (12), juce::Justification::centredLeft);
        auto bar = gr.removeFromTop (6);
        g.setColour (col::bg2); g.fillRect (bar); g.setColour (col::border); g.drawRect (bar, 1.0f);
        g.setColour (col::white); g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v / rng)));
        g.setColour (col::text2); g.setFont (font (10.0f));
        g.drawText (v > 0.05f ? "-" + juce::String (v, 1) + " dB" : "0.0 dB", gr.removeFromTop (14), juce::Justification::centredRight);
        gr.removeFromTop (8);
    }
    g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.12f));
    g.drawText ("CORR " + juce::String (m.corr.load(), 2), 1138, 452, 118, 12, juce::Justification::centredLeft);
    g.drawText ("MONITOR", 1082, 584, 174, 12, juce::Justification::centredLeft);
    if ((int) proc.param (id::monitor) != 0)
    {
        g.setColour (col::white); g.setFont (font (9.0f, W::SemiBold, 0.1f));
        g.drawText ("MONITORING - NOT FULL", 1082, 672, 174, 14, juce::Justification::centredLeft);
    }
    if (proc.param (id::listen) > 0.5f || proc.param (id::bypass) > 0.5f)
    {
        g.setColour (col::white); g.setFont (font (9.0f, W::SemiBold, 0.1f));
        g.drawText (proc.param (id::bypass) > 0.5f ? "BYPASSED" : "LISTENING TO ORIGINAL", 1082, 688, 174, 14, juce::Justification::centredLeft);
    }
    g.setColour (col::muted); g.setFont (font (9.0f));
    g.drawFittedText ("All meters measure the processed output before MONITOR.", juce::Rectangle<int> (1082, 744, 174, 40), juce::Justification::bottomLeft, 3);
}

void PolooyxReleaseEditor::updateActionButtons()
{
    using A = PolooyxReleaseProcessor::Action;
    const bool busy = proc.isBusy(), listening = proc.isListening();
    if (busy)
    {
        analyzeBtn.setButtonText ("CANCEL"); analyzeBtn.setEnabled (true);
        poloBtn.setButtonText ("POLO IT"); poloBtn.setEnabled (false); finalBtn.setEnabled (false);
        const auto a = proc.currentJob();
        const float pr = proc.jobProgress();
        overlay.title = a == A::PoloIt ? "POLO IT" : a == A::FinalCheck ? "FINAL CHECK" : "ANALYZE POLO";
        overlay.stage = a == A::PoloIt ? (pr < 0.12f ? 0 : pr < 0.88f ? 1 : 2) : -1;
        overlay.prog = pr;
        overlay.line = a == A::PoloIt ? "Rendering your mix through the real chain to verify every correction."
                     : a == A::FinalCheck ? "Rendering the captured mix through the current settings." : "Measuring loudness, peaks, spectrum, stereo and dynamics.";
        overlay.setVisible (true); overlay.repaint();
    }
    else if (listening)
    {
        const auto pend = proc.getPendingAction();
        analyzeBtn.setButtonText (pend == A::Analyze ? "STOP & ANALYZE" : "STOP"); analyzeBtn.setEnabled (true);
        poloBtn.setButtonText (pend == A::PoloIt ? "STOP & POLO IT" : "POLO IT"); poloBtn.setEnabled (pend == A::PoloIt);
        finalBtn.setEnabled (false);
        overlay.title = "LISTENING";
        overlay.stage = -1;
        overlay.prog = proc.listenSeconds() / proc.maxCaptureSeconds();
        overlay.line = proc.isRecording()
            ? mmss (proc.listenSeconds()) + " captured (max " + mmss (proc.maxCaptureSeconds()) + ").  Play the whole song, or at least its loudest part.  Press STOP when done - capture also ends when playback stops after 10 s."
            : "Waiting for audio - press play in your DAW.";
        overlay.setVisible (true); overlay.repaint();
    }
    else
    {
        analyzeBtn.setButtonText ("ANALYZE POLO"); analyzeBtn.setEnabled (true);
        poloBtn.setButtonText ("POLO IT"); poloBtn.setEnabled (true); finalBtn.setEnabled (true);
        overlay.setVisible (false);
    }
}

void PolooyxReleaseEditor::refreshResults()
{
    const auto r = proc.getResults();
    diag.set (r); next.set (r); result.set (r); labPage->set (r);
    lastRes = r;   // long-lived copy for the spectrum overlay
    spec.longTerm = r.hasAnalysis ? &lastRes.analysis : nullptr;
}

void PolooyxReleaseEditor::timerCallback()
{
    // spectrum + scope
    const int fftSel = (int) proc.uiState.getProperty ("fftSize", 0);
    int order = fftSel > 0 ? 10 + fftSel : 11 + (int) proc.param (id::quality);
    if (fftSel == 0) order += (int) std::round (std::log2 (std::max (1.0, proc.getSr() / 48000.0)));
    spec.pre.setFftOrder (juce::jlimit (10, 15, order)); spec.post.setFftOrder (juce::jlimit (10, 15, order));
    spec.pre.setSampleRate (proc.getSr()); spec.post.setSampleRate (proc.getSr());
    const int sm = (int) proc.uiState.getProperty ("specSmooth", 1);
    const float smv = sm == 0 ? 0.45f : sm == 1 ? 0.72f : 0.88f;
    spec.pre.setSmoothing (smv); spec.post.setSmoothing (smv);
    const bool hold = (bool) proc.uiState.getProperty ("peakHold", false);
    if (hold != spec.holdShown) { spec.pre.setPeakHold (hold); spec.post.setPeakHold (hold); spec.holdShown = hold; }
    spec.mode = (int) proc.uiState.getProperty ("specMode", 2);
    spec.rangeDb = (float) (int) proc.uiState.getProperty ("specRange", 90);
    for (int i = 0; i < 3; ++i) specModeBtns[i]->setToggleState (spec.mode == i, juce::dontSendNotification);
    rangeBtn.setButtonText ("RANGE " + juce::String ((int) spec.rangeDb) + " dB");
    smoothBtn.setButtonText (juce::String ("SMOOTH ") + (sm == 0 ? "LOW" : sm == 1 ? "MED" : "HIGH"));
    spec.pre.update (proc.specPre); spec.post.update (proc.specPost);
    spec.scanning = proc.isListening();
    if (spec.scanning) { scan += 0.012f; if (scan > 1.0f) scan = 0.0f; spec.scanPos = scan; }
    spec.repaint();

    float l[1024], r[1024];
    int nl = proc.scopeL.pull (l, 1024), nr = proc.scopeR.pull (r, 1024);
    stereo.push (l, r, std::min (nl, nr));
    auto& m = proc.meters;
    stereo.setValues (m.corr, m.width, m.midDb, m.sideDb, m.balance);
    stereo.target = targetLufs ((Target) (int) proc.param (id::target), proc.param (id::customTarget));
    if (++histTick >= 3) { histTick = 0; stereo.pushHistory (m.lufsS); }
    stereo.repaint();

    inMeter.setLevels (m.inPeak, m.inTp, m.inTpMax);
    outMeter.setLevels (m.outPeak, m.outTp, m.outTpMax);

    // header status
    const auto q = qualityNames()[(int) proc.param (id::quality)];
    statusText = juce::String (proc.getSr() / 1000.0, 1) + " kHz   " + (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? "STANDALONE" : "VST3")
               + "   " + q + "   " + juce::String (m.osFactor.load()) + "X OS   LATENCY " + juce::String (proc.getLatencySamples() * 1000.0 / proc.getSr(), 1)
               + " ms   CPU " + juce::String (juce::roundToInt (m.cpu.load() * 100.0f)) + "%";
    presetBtn.setButtonText (proc.currentPresetName);
    aBtn.setToggleState (proc.abSlot == 0, juce::dontSendNotification);
    bBtn.setToggleState (proc.abSlot == 1, juce::dontSendNotification);
    undoBtn.setEnabled (proc.undo.canUndo()); redoBtn.setEnabled (proc.undo.canRedo());

    updateActionButtons();
    if (proc.resultsVersion() != lastResultVersion) { lastResultVersion = proc.resultsVersion(); refreshResults(); }
    if (eqPage->isVisible()) eqPage->tick();
    if (! (view == 0)) { if (auto* pg = tabPages[(size_t) proTab]) pg->repaint(); }
    if (diag.isVisible())
    {
        const auto mp = diag.getMouseXYRelative();
        diag.setTooltip (diag.getTipAt (mp));
    }
    content.repaint (juce::Rectangle<int> (1072, 64, 208, 736));
    content.repaint (juce::Rectangle<int> (588, 44, 668, 16));
    content.repaint (juce::Rectangle<int> (960, 394, 100, 28));
}

void PolooyxReleaseEditor::stepPreset (int dir)
{
    const int n = (int) factoryPresets().size();
    int cur = 0;
    for (int i = 0; i < n; ++i) if (proc.currentPresetName == factoryPresets()[(size_t) i].name) cur = i;
    proc.loadFactoryPreset ((cur + dir + n) % n);
}

void PolooyxReleaseEditor::openPresetBrowser()
{
    auto browser = std::make_unique<PresetBrowser> (proc, nullptr);
    auto* raw = browser.get();
    raw->onPicked = [raw] { if (auto* cb = raw->findParentComponentOfClass<juce::CallOutBox>()) cb->dismiss(); };
    juce::CallOutBox::launchAsynchronously (std::move (browser), getLocalArea (&presetBtn, presetBtn.getLocalBounds()), this);
}

void PolooyxReleaseEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("SAVE PRESET", "Name for this preset (saved to Documents/POLOOYX/Release Presets):", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName);
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int choice)
    {
        if (choice == 1) proc.saveUserPreset (w->getTextEditorContents ("name"));
    }), true);
}

void PolooyxReleaseEditor::openSettings()
{
    struct Settings : juce::Component
    {
        Settings (PolooyxReleaseProcessor& p) : proc (p)
        {
            auto& s = p.apvts;
            quality = std::make_unique<Choice> (s, id::quality, "QUALITY");
            os = std::make_unique<Choice> (s, id::oversampling, "OVERSAMPLING");
            custom = std::make_unique<Knob> (s, id::customTarget, "CUSTOM TARGET");
            fft.addItemList ({ "AUTO", "2048", "4096", "8192", "16384" }, 1);
            fft.setSelectedItemIndex ((int) p.uiState.getProperty ("fftSize", 0), juce::dontSendNotification);
            fft.onChange = [this] { proc.uiState.setProperty ("fftSize", fft.getSelectedItemIndex(), nullptr); };
            tp.addItemList ({ "2X", "4X", "8X" }, 1);
            const int f = (int) p.uiState.getProperty ("tpFactor", 4);
            tp.setSelectedItemIndex (f == 2 ? 0 : f == 8 ? 2 : 1, juce::dontSendNotification);
            tp.onChange = [this] { const int i = tp.getSelectedItemIndex(); proc.uiState.setProperty ("tpFactor", i == 0 ? 2 : i == 2 ? 8 : 4, nullptr); };
            for (juce::Component* c : { (juce::Component*) quality.get(), (juce::Component*) os.get(), (juce::Component*) custom.get(), (juce::Component*) &fft, (juce::Component*) &tp }) addAndMakeVisible (c);
            setSize (300, 300);
        }
        void resized() override
        {
            quality->setBounds (16, 16, 128, 40); os->setBounds (156, 16, 128, 40);
            fft.setBounds (16, 84, 128, 24); tp.setBounds (156, 84, 128, 24);
            custom->setBounds (16, 124, 90, 96);
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (col::panel);
            g.setColour (col::muted); g.setFont (font (9.5f, W::Medium, 0.1f));
            g.drawText ("ANALYZER FFT", 16, 68, 128, 14, juce::Justification::centredLeft);
            g.drawText ("TRUE-PEAK METER", 156, 68, 128, 14, juce::Justification::centredLeft);
            g.setColour (col::text2); g.setFont (font (10.5f));
            g.drawFittedText ("True-peak meter oversampling applies the next time playback starts. The limiter always detects at 4x. "
                              "CUSTOM TARGET is used when LOUDNESS TARGET = CUSTOM.", juce::Rectangle<int> (116, 128, 170, 150), juce::Justification::topLeft, 9);
        }
        PolooyxReleaseProcessor& proc;
        std::unique_ptr<Choice> quality, os;
        std::unique_ptr<Knob> custom;
        juce::ComboBox fft, tp;
    };
    juce::CallOutBox::launchAsynchronously (std::make_unique<Settings> (proc), getLocalArea (&settingsBtn, settingsBtn.getLocalBounds()), this);
}
