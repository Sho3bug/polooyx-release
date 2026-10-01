#pragma once
// POLOOYX RELEASE ENGINE — reusable controls, all attached to real parameters.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Look.h"

namespace prl::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// rotary knob with a small white label above and the numeric value below
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& pid, const juce::String& title, bool large = false)
        : name (title), big (large)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        slider.setMouseDragSensitivity (large ? 260 : 200);
        slider.setVelocityBasedMode (false);
        if (auto* p = s.getParameter (pid))
        {
            const auto r = dynamic_cast<juce::RangedAudioParameter*> (p)->getNormalisableRange();
            slider.getProperties().set ("bipolar", r.start < 0 && r.end > 0 && std::abs (r.start + r.end) < 1e-3f);
            param = p;
        }
        att = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
        slider.setDoubleClickReturnValue (true, (double) dynamic_cast<juce::RangedAudioParameter*> (param)->convertFrom0to1 (param->getDefaultValue()));
        slider.setPopupDisplayEnabled (false, false, nullptr);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (isEnabled() ? col::white : col::muted);
        g.setFont (font (big ? 12.5f : 10.0f, big ? W::SemiBold : W::Medium, big ? 0.16f : 0.08f));
        g.drawText (name, r.removeFromTop (big ? 18 : 14), juce::Justification::centred, false);
        g.setColour (col::text2);
        g.setFont (font (big ? 12.0f : 10.5f));
        g.drawText (param != nullptr ? param->getCurrentValueAsText() : juce::String(), r.removeFromBottom (big ? 18 : 14), juce::Justification::centred, false);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (big ? 18 : 14); r.removeFromBottom (big ? 18 : 14);
        const int d = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (d, d));
    }

    juce::Slider slider;
private:
    juce::String name;
    bool big;
    juce::AudioProcessorParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// toggle button bound to a bool parameter (active → inverted white)
class Toggle : public juce::TextButton
{
public:
    Toggle (APVTS& s, const juce::String& pid, const juce::String& label) : juce::TextButton (label)
    {
        setClickingTogglesState (true);
        att = std::make_unique<APVTS::ButtonAttachment> (s, pid, *this);
        getProperties().set ("fontSize", 10.5f);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// segmented selector for a choice parameter (one button per option)
class Segmented : public juce::Component, public juce::SettableTooltipClient, private juce::AudioProcessorParameter::Listener, private juce::AsyncUpdater
{
public:
    Segmented (APVTS& s, const juce::String& pid, juce::StringArray labels = {}) : state (s)
    {
        param = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid));
        jassert (param != nullptr);
        if (labels.isEmpty()) labels = param->choices;
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->getProperties().set ("fontSize", 10.5f);
            b->onClick = [this, i] { param->beginChangeGesture(); *param = i; param->endChangeGesture(); };
            addAndMakeVisible (b);
        }
        param->addListener (this);
        refresh();
    }
    ~Segmented() override { param->removeListener (this); cancelPendingUpdate(); }
    void resized() override
    {
        const int n = buttons.size(); if (n == 0) return;
        auto r = getLocalBounds();
        const int w = r.getWidth() / n;
        for (int i = 0; i < n; ++i) buttons[i]->setBounds (i == n - 1 ? r : r.removeFromLeft (w - 0).withTrimmedRight (-1));
    }
    void refresh() { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (param->getIndex() == i, juce::dontSendNotification); }
private:
    void parameterValueChanged (int, float) override { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override { refresh(); }
    APVTS& state;
    juce::AudioParameterChoice* param = nullptr;
    juce::OwnedArray<juce::TextButton> buttons;
};

// combo box bound to a choice parameter, with a small caption
class Choice : public juce::Component
{
public:
    Choice (APVTS& s, const juce::String& pid, const juce::String& title) : caption (title)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid))) box.addItemList (c->choices, 1);
        att = std::make_unique<APVTS::ComboBoxAttachment> (s, pid, box);
        addAndMakeVisible (box);
    }
    void paint (juce::Graphics& g) override
    {
        if (caption.isEmpty()) return;
        g.setColour (col::muted); g.setFont (font (9.5f, W::Medium, 0.1f));
        g.drawText (caption, getLocalBounds().removeFromTop (13), juce::Justification::centredLeft, false);
    }
    void resized() override { auto r = getLocalBounds(); if (caption.isNotEmpty()) r.removeFromTop (14); box.setBounds (r.withHeight (juce::jmin (r.getHeight(), 24))); }
    juce::ComboBox box;
private:
    juce::String caption;
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// label + value readout
struct Readout
{
    static void draw (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label, const juce::String& value, float valueSize = 15.0f, bool emphasise = false)
    {
        g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.12f));
        g.drawText (label, r.removeFromTop (12), juce::Justification::centredLeft, false);
        g.setColour (emphasise ? col::white : col::white.withAlpha (0.92f));
        g.setFont (font (valueSize, emphasise ? W::Medium : W::Regular));
        g.drawText (value, r, juce::Justification::centredLeft, false);
    }
};

inline juce::String fmtDb (float v, int dp = 1, const char* unit = "")
{
    if (v < -150.0f) return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + juce::String (" ") + unit;   // en dash
    return juce::String (v, dp) + (juce::String (unit).isEmpty() ? "" : " " + juce::String (unit));
}

// vertical level meter (peak bar + true-peak tick + max hold); grayscale
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String t) : title (std::move (t)) {}
    void setLevels (float peakDb, float tpDb, float maxDb) { pk = peakDb; tp = tpDb; mx = maxDb; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        auto lab = r.removeFromBottom (13);
        g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.1f));
        g.drawText (title, lab, juce::Justification::centred, false);
        auto bar = r.toFloat();
        g.setColour (col::bg2); g.fillRect (bar);
        g.setColour (col::border); g.drawRect (bar, 1.0f);
        auto y = [&] (float db) { const float n = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); return bar.getBottom() - n * bar.getHeight(); };
        const float top = y (pk);
        juce::ColourGradient gr (col::white, 0, bar.getY(), col::faint, 0, bar.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRect (bar.withTop (top).reduced (2.0f, 0.0f).withBottom (bar.getBottom() - 1));
        g.setColour (col::white);
        if (tp > -60) g.fillRect (bar.getX() + 1, y (tp) - 1, bar.getWidth() - 2, 2.0f);
        if (mx > -60) { g.setColour (col::text2); g.fillRect (bar.getX() + 1, y (mx), bar.getWidth() - 2, 1.0f); }
        // 0 dB and -1 dB marks
        g.setColour (col::muted.withAlpha (0.6f));
        for (float m : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f }) g.fillRect (bar.getRight() - 4, y (m), 3.0f, 1.0f);
    }
private:
    juce::String title;
    float pk = -200, tp = -200, mx = -200;
};

// horizontal gain-reduction strip
inline void drawGr (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, float grDb, float rangeDb = 12.0f)
{
    g.setColour (col::muted); g.setFont (font (9.0f, W::Medium, 0.1f));
    g.drawText (label, r.removeFromLeft (64), juce::Justification::centredLeft, false);
    auto val = r.removeFromRight (46);
    auto bar = r.reduced (0, r.getHeight() * 0.3f);
    g.setColour (col::bg2); g.fillRect (bar);
    g.setColour (col::border); g.drawRect (bar, 1.0f);
    const float n = juce::jlimit (0.0f, 1.0f, grDb / rangeDb);
    g.setColour (col::white); g.fillRect (bar.withWidth (bar.getWidth() * n).reduced (0, 1));
    g.setColour (col::text2); g.setFont (font (10.5f));
    g.drawText (grDb > 0.05f ? "-" + juce::String (grDb, 1) + " dB" : "0.0 dB", val, juce::Justification::centredRight, false);
}
} // namespace prl::ui
