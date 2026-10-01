#pragma once
// POLOOYX RELEASE ENGINE — searchable preset browser (factory + user presets).

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace prl::ui
{
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void()> onPicked;

    PresetBrowser (PolooyxReleaseProcessor& p, std::function<void()> cb) : onPicked (std::move (cb)), proc (p)
    {
        search.setTextToShowWhenEmpty ("SEARCH PRESETS", col::muted);
        search.setFont (font (13.0f));
        search.onTextChange = [this] { rebuild(); };
        search.onReturnKey = [this] { if (! rows.empty()) choose (0); };
        addAndMakeVisible (search);
        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, col::panel);
        addAndMakeVisible (list);
        rebuild();
        setSize (320, 420);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        search.setBounds (r.removeFromTop (28));
        r.removeFromTop (8);
        list.setBounds (r);
    }
    void paint (juce::Graphics& g) override { g.fillAll (col::panel); }
    void visibilityChanged() override { if (isVisible()) search.grabKeyboardFocus(); }

private:
    struct Row { juce::String name, category; int factory = -1; juce::File file; bool header = false; };
    void rebuild()
    {
        rows.clear();
        const auto q = search.getText().trim().toUpperCase();
        juce::String lastCat;
        const auto& f = factoryPresets();
        for (int i = 0; i < (int) f.size(); ++i)
        {
            const juce::String n (f[(size_t) i].name), c (f[(size_t) i].category);
            if (q.isNotEmpty() && ! n.toUpperCase().contains (q) && ! c.contains (q)) continue;
            if (c != lastCat) { rows.push_back ({ c, c, -1, {}, true }); lastCat = c; }
            rows.push_back ({ n, c, i, {}, false });
        }
        bool userHeader = false;
        for (auto& file : proc.userPresets())
        {
            const auto n = file.getFileNameWithoutExtension();
            if (q.isNotEmpty() && ! n.toUpperCase().contains (q)) continue;
            if (! userHeader) { rows.push_back ({ "USER", "USER", -1, {}, true }); userHeader = true; }
            rows.push_back ({ n, "USER", -1, file, false });
        }
        list.updateContent(); list.repaint();
    }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
        const auto& r = rows[(size_t) row];
        if (r.header)
        {
            g.setColour (col::muted); g.setFont (font (9.5f, W::SemiBold, 0.16f));
            g.drawText (r.name, 8, 0, w - 16, h, juce::Justification::bottomLeft);
            return;
        }
        const bool current = r.name == proc.currentPresetName;
        if (selected) { g.setColour (col::white); g.fillRect (0, 0, w, h); }
        g.setColour (selected ? juce::Colours::black : current ? col::white : col::text2);
        g.setFont (font (12.5f, current ? W::Medium : W::Regular, 0.03f));
        g.drawText (r.name, 14, 0, w - 20, h, juce::Justification::centredLeft);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { choose (row); }
    void returnKeyPressed (int row) override { choose (row); }
    void choose (int row)
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].header) return;
        const auto& r = rows[(size_t) row];
        if (r.factory >= 0) proc.loadFactoryPreset (r.factory);
        else proc.loadUserPreset (r.file);
        if (onPicked) onPicked();
    }

    PolooyxReleaseProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    std::vector<Row> rows;
};
} // namespace prl::ui
