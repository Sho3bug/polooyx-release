#pragma once
// POLOOYX RELEASE ENGINE — visual language. Black / white / grayscale only.
// State is communicated by contrast and inversion (active = white background, black text), never colour.

#include <juce_gui_basics/juce_gui_basics.h>
#if PRL_HAS_INTER
 #include "PrlFonts.h"
#endif

namespace prl::ui
{
namespace col
{
    const juce::Colour bg      { 0xff050505 };
    const juce::Colour bg2     { 0xff080808 };
    const juce::Colour panel   { 0xff0d0d0d };
    const juce::Colour raised  { 0xff121212 };
    const juce::Colour hover   { 0xff181818 };
    const juce::Colour border  { 0xff1d1d1d };
    const juce::Colour line    { 0xff262626 };
    const juce::Colour line2   { 0xff333333 };
    const juce::Colour faint   { 0xff3c3c3c };
    const juce::Colour muted   { 0xff666666 };
    const juce::Colour text2   { 0xffa0a0a0 };
    const juce::Colour white   { 0xffffffff };
}

class Fonts
{
public:
    static Fonts& get() { static Fonts f; return f; }
    juce::Typeface::Ptr regular, medium, semibold;
private:
    Fonts()
    {
       #if PRL_HAS_INTER
        regular  = juce::Typeface::createSystemTypefaceFor (PrlFonts::InterRegular_ttf,  PrlFonts::InterRegular_ttfSize);
        medium   = juce::Typeface::createSystemTypefaceFor (PrlFonts::InterMedium_ttf,   PrlFonts::InterMedium_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor (PrlFonts::InterSemiBold_ttf, PrlFonts::InterSemiBold_ttfSize);
       #else   // system sans-serif fallback
        const juce::Font base { juce::FontOptions (14.0f) };
        regular = base.getTypefacePtr();
        medium = semibold = juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).getTypefacePtr();
       #endif
    }
};

enum class W { Light, Regular, Medium, SemiBold };
inline float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }
inline juce::Font font (float size, W w = W::Regular, float tracking = 0.0f)
{
    auto& f = Fonts::get();
    auto tf = w == W::Medium ? f.medium : w == W::SemiBold ? f.semibold : f.regular;
    juce::Font fnt { juce::FontOptions (tf).withHeight (size) };
    if (tracking != 0.0f) fnt.setExtraKerningFactor (tracking);
    return fnt;
}

// draw text with explicit letter spacing (tracking) — used for the wordmark and section titles
inline void drawTracked (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j)
{
    g.setFont (f);
    g.drawText (s, r, j, false);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::PopupMenu::backgroundColourId, col::raised);
        setColour (juce::PopupMenu::textColourId, col::text2);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::white);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::PopupMenu::headerTextColourId, col::muted);
        setColour (juce::ComboBox::backgroundColourId, col::bg2);
        setColour (juce::ComboBox::textColourId, col::white);
        setColour (juce::ComboBox::outlineColourId, col::border);
        setColour (juce::ComboBox::arrowColourId, col::text2);
        setColour (juce::TextEditor::backgroundColourId, col::bg2);
        setColour (juce::TextEditor::textColourId, col::white);
        setColour (juce::TextEditor::outlineColourId, col::border);
        setColour (juce::TextEditor::focusedOutlineColourId, col::muted);
        setColour (juce::TextEditor::highlightColourId, col::line2);
        setColour (juce::CaretComponent::caretColourId, col::white);
        setColour (juce::ListBox::backgroundColourId, col::bg2);
        setColour (juce::TooltipWindow::backgroundColourId, col::raised);
        setColour (juce::TooltipWindow::textColourId, col::text2);
        setColour (juce::TooltipWindow::outlineColourId, col::line2);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::white);
        setColour (juce::AlertWindow::outlineColourId, col::line2);
        setColour (juce::TextButton::buttonColourId, col::bg2);
        setColour (juce::TextButton::buttonOnColourId, col::white);
        setColour (juce::TextButton::textColourOffId, col::white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::ScrollBar::thumbColourId, col::line2);
        setColour (juce::Label::textColourId, col::text2);
       #if PRL_HAS_INTER
        setDefaultSansSerifTypeface (Fonts::get().regular);
       #endif
    }

    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        const float sz = (float) b.getProperties().getWithDefault ("fontSize", juce::jmin (12.0f, h * 0.42f));
        return font (sz, (bool) b.getProperties().getWithDefault ("bold", false) ? W::SemiBold : W::Medium, 0.06f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
        const bool flat = (bool) b.getProperties().getWithDefault ("flat", false);
        juce::Colour fill = col::bg2, edge = primary ? col::white.withAlpha (0.85f) : col::border;
        if (on) { fill = col::white; edge = col::white; }
        else if (down) fill = primary ? col::text2 : col::raised;
        else if (over) { fill = primary ? col::white : col::hover; if (! primary) edge = col::line2; }
        if (! b.isEnabled()) { fill = col::bg2; edge = col::border; }
        if (flat && ! on && ! over) { fill = juce::Colours::transparentBlack; edge = juce::Colours::transparentBlack; }
        g.setColour (fill); g.fillRect (r);
        g.setColour (edge); g.drawRect (r, primary ? 1.2f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        const bool on = b.getToggleState();
        const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
        juce::Colour c = col::white;
        if (on || (primary && over && ! down)) c = juce::Colours::black;
        else if (! b.isEnabled()) c = col::muted;
        else if (! over && (bool) b.getProperties().getWithDefault ("dim", false)) c = col::text2;
        g.setColour (c);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.isMouseOver (true) ? col::hover : col::bg2); g.fillRect (r);
        g.setColour (box.isMouseOver (true) ? col::line2 : col::border); g.drawRect (r, 1.0f);
        juce::Path p; const float cx = (float) w - 11.0f, cy = h * 0.5f;
        p.addTriangle (cx - 3.5f, cy - 1.5f, cx + 3.5f, cy - 1.5f, cx, cy + 2.5f);
        g.setColour (col::text2); g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return font (juce::jmin (12.0f, b.getHeight() * 0.48f), W::Medium, 0.04f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, col::white);
    }
    juce::Font getPopupMenuFont() override { return font (13.0f, W::Regular); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::raised);
        g.setColour (col::line2); g.drawRect (0, 0, w, h, 1);
    }
    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (col::raised);
        g.setColour (col::line2); g.drawRect (0, 0, w, h, 1);
        g.setColour (col::text2); g.setFont (font (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (0, 0, w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (360, (int) textWidth (font (12.0f), text) + 20), lines = 1 + (int) textWidth (font (12.0f), text) / 340;
        const int h = 10 + 16 * lines;
        return juce::Rectangle<int> (pos.x + 12, pos.y + 16, w, h).constrainedWithin (parent);
    }

    // rotary knob: matte black body, subtle dimensional shading, thin white indicator
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float d = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float rOuter = d * 0.5f, rBody = rOuter * 0.80f;
        const bool bip = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float ang = a0 + pos * (a1 - a0), mid = 0.5f * (a0 + a1);

        // track + value arc (thin, grayscale)
        juce::Path track; track.addCentredArc (c.x, c.y, rOuter - 1.5f, rOuter - 1.5f, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (1.5f));
        juce::Path val; val.addCentredArc (c.x, c.y, rOuter - 1.5f, rOuter - 1.5f, 0, bip ? mid : a0, ang, true);
        g.setColour (s.isEnabled() ? col::text2 : col::faint); g.strokePath (val, juce::PathStrokeType (1.5f));

        // body
        juce::ColourGradient body (juce::Colour (0xff1e1e1e), c.x - rBody * 0.6f, c.y - rBody * 0.8f,
                                   juce::Colour (0xff070707), c.x + rBody * 0.5f, c.y + rBody * 0.9f, true);
        g.setGradientFill (body); g.fillEllipse (c.x - rBody, c.y - rBody, 2 * rBody, 2 * rBody);
        g.setColour (juce::Colour (0xff2a2a2a)); g.drawEllipse (c.x - rBody, c.y - rBody, 2 * rBody, 2 * rBody, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rBody + 1.5f, c.y - rBody + 1.5f, 2 * rBody - 3, 2 * rBody - 3, 1.0f);

        // indicator
        const float r0 = rBody * 0.35f, r1 = rBody * 0.86f;
        const auto p0 = c.getPointOnCircumference (r0, ang), p1 = c.getPointOnCircumference (r1, ang);
        g.setColour (s.isEnabled() ? col::white : col::muted);
        g.drawLine ({ p0, p1 }, juce::jmax (1.2f, d * 0.018f));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::textColourId, col::text2);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setFont (font (11.0f));
        return l;
    }
};
} // namespace prl::ui
