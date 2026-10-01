#pragma once
// POLOOYX RELEASE ENGINE — editor. Designed at 1280 x 800 and scaled (1024x640 … 1440x900 and beyond).

#include "PluginProcessor.h"
#include "UI/Look.h"
#include "UI/Widgets.h"
#include "UI/AnalyzerViews.h"
#include "UI/Panels.h"
#include "UI/Pages.h"
#include "UI/PresetBrowser.h"

class PolooyxReleaseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PolooyxReleaseEditor (PolooyxReleaseProcessor&);
    ~PolooyxReleaseEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int kW = 1280, kH = 800;
    void setView (int v);          // 0 BEGINNER, 1 PRO, 2 LAB (public for tests / snapshots)
    void setProTab (int t);

private:
    struct Content : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    void timerCallback() override;
    void layout();
    void paintContent (juce::Graphics&);
    void refreshResults();
    void updateActionButtons();
    void openPresetBrowser();
    void openSettings();
    void savePresetDialog();
    void stepPreset (int dir);

    PolooyxReleaseProcessor& proc;
    prl::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltip { this, 600 };
    Content content;

    // header
    juce::OwnedArray<juce::TextButton> viewBtns;
    juce::TextButton presetBtn, prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, aBtn { "A" }, bBtn { "B" },
                     undoBtn { "UNDO" }, redoBtn { "REDO" }, settingsBtn { "SETTINGS" };
    // analyzer
    prl::ui::SpectrumView spec;
    prl::ui::StereoView stereo;
    juce::OwnedArray<juce::TextButton> specModeBtns;
    juce::TextButton freezeBtn { "FREEZE" }, holdBtn { "PEAK HOLD" }, rangeBtn, smoothBtn;
    // actions
    juce::TextButton analyzeBtn { "ANALYZE POLO" }, finalBtn { "FINAL CHECK" }, poloBtn { "POLO IT" };
    std::unique_ptr<prl::ui::Choice> modeChoice, targetChoice;
    std::unique_ptr<prl::ui::Segmented> listenSeg;
    std::unique_ptr<prl::ui::Toggle> levelMatchBtn;
    // beginner middle
    prl::ui::DiagnosisPanel diag;
    prl::ui::NextStepsPanel next;
    prl::ui::ResultPanel result;
    prl::ui::StatusOverlay overlay;
    // macros
    std::vector<std::unique_ptr<prl::ui::Knob>> macros;
    // pro / lab
    juce::OwnedArray<juce::TextButton> tabBtns;
    std::vector<juce::Component*> tabPages;
    std::unique_ptr<prl::ui::EqPage> eqPage;
    std::unique_ptr<prl::ui::BandPage> deqPage, mbPage;
    std::unique_ptr<prl::ui::ModulePage> dynPage, tonePage, stereoPage, outPage, ioPage;
    std::unique_ptr<prl::ui::LabPage> labPage;
    juce::Component macroHolder, poloHolder;
    // sidebar
    prl::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    std::unique_ptr<prl::ui::Knob> inKnob, outKnob, wetKnob;
    std::unique_ptr<prl::ui::Segmented> monitorSeg;
    std::unique_ptr<prl::ui::Toggle> bypassBtn;
    juce::TextButton resetBtn { "RESET PEAKS" };

    int view = 0, proTab = 0, lastResultVersion = -1;
    float scan = 0.0f;
    int histTick = 0;
    juce::String statusText;
    PolooyxReleaseProcessor::Results lastRes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolooyxReleaseEditor)
};
