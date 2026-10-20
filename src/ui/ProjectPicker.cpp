#include "ProjectPicker.h"

#include "BrandLookAndFeel.h"

namespace dpm
{

ProjectPicker::ProjectPicker()
{
    heading.setText (TRANS ("Choose project"), juce::dontSendNotification);
    heading.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    addAndMakeVisible (heading);

    search.setTextToShowWhenEmpty (TRANS ("Search projects..."), palette::textTertiary);
    search.setIndents (10, 7);
    search.onTextChange = [this] { refresh (hasIndex, unlinkButton.isEnabled()); };
    search.onReturnKey = [this] { pickRow (list.getSelectedRow() >= 0 ? list.getSelectedRow() : 0); };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (44);
    list.setOutlineThickness (0);
    list.setColour (juce::ListBox::backgroundColourId, palette::background);
    addAndMakeVisible (list);

    pickButton.setButtonText (TRANS ("Link"));
    makePrimary (pickButton);
    pickButton.onClick = [this] { pickRow (list.getSelectedRow()); };
    addAndMakeVisible (pickButton);

    unlinkButton.setButtonText (TRANS ("Unlink"));
    unlinkButton.onClick = [this] { if (onUnlink) onUnlink(); };
    addAndMakeVisible (unlinkButton);

    closeButton.setButtonText (TRANS ("Cancel"));
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    setWantsKeyboardFocus (true);
}

void ProjectPicker::refresh (bool appHasSharedProjects, bool isLinked)
{
    hasIndex = appHasSharedProjects;
    rows = rowsFor ? rowsFor (search.getText()) : std::vector<PickerRow> {};
    list.updateContent();
    list.selectRow (rows.empty() ? -1 : 0);
    unlinkButton.setEnabled (isLinked);
    pickButton.setEnabled (! rows.empty());
    repaint();
}

void ProjectPicker::visibilityChanged()
{
    if (isVisible())
    {
        search.clear();
        search.grabKeyboardFocus();
    }
}

bool ProjectPicker::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }
    return false;
}

void ProjectPicker::paint (juce::Graphics& g)
{
    g.fillAll (palette::background.withAlpha (0.97f));
    drawCard (g, getLocalBounds().reduced (12));

    if (rows.empty())
    {
        g.setColour (palette::textTertiary);
        g.setFont (juce::FontOptions (14.0f));
        const auto message = hasIndex ? TRANS ("No projects found.")
                                      : TRANS ("The app hasn't shared its project list yet. Open DAW Project Manager once with this plugin installed.");
        g.drawFittedText (message, list.getBounds().reduced (16), juce::Justification::centred, 4);
    }
}

void ProjectPicker::resized()
{
    auto area = getLocalBounds().reduced (24);
    heading.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    search.setBounds (area.removeFromTop (34));
    area.removeFromTop (10);

    auto buttons = area.removeFromBottom (34);
    closeButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (8);
    pickButton.setBounds (buttons.removeFromRight (100));
    unlinkButton.setBounds (buttons.removeFromLeft (100));
    area.removeFromBottom (10);

    list.setBounds (area);
}

int ProjectPicker::getNumRows()
{
    return (int) rows.size();
}

void ProjectPicker::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    const auto& r = rows[(size_t) row];

    if (selected)
    {
        const juce::Rectangle<float> rowArea (0, 1, (float) width, (float) height - 2);
        g.setColour (palette::selection);
        g.fillRoundedRectangle (rowArea, 6.0f);
        g.setColour (palette::accent);
        g.fillRoundedRectangle (rowArea.withWidth (3.0f).reduced (0, 8), 1.5f);
    }

    auto area = juce::Rectangle<int> (10, 4, width - 20, height - 8);
    auto top = area.removeFromTop (20);

    if (r.suggested)
    {
        const auto tag = TRANS ("Suggested");
        const auto font = juce::FontOptions (11.0f, juce::Font::bold);
        const auto tagWidth = (int) juce::GlyphArrangement::getStringWidth (juce::Font (font), tag) + 12;
        const auto tagArea = top.removeFromRight (tagWidth).withSizeKeepingCentre (tagWidth, 16);
        g.setColour (palette::connected.withAlpha (0.25f));
        g.fillRoundedRectangle (tagArea.toFloat(), 8.0f);
        g.setColour (palette::connected.brighter (0.4f));
        g.setFont (font);
        g.drawText (tag, tagArea, juce::Justification::centred);
        top.removeFromRight (6);
    }

    g.setColour (palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (r.ref.name, top, juce::Justification::centredLeft, true);

    juce::StringArray details;
    if (r.profileName.isNotEmpty()) details.add (r.profileName);
    if (r.dawType.isNotEmpty())     details.add (r.dawType);
    details.add (r.ref.filePath);

    g.setColour (palette::textTertiary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (details.joinIntoString (detailSeparator), area, juce::Justification::centredLeft, true);
}

void ProjectPicker::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    pickRow (row);
}

void ProjectPicker::returnKeyPressed (int row)
{
    pickRow (row);
}

void ProjectPicker::pickRow (int row)
{
    if (row >= 0 && row < (int) rows.size() && onPick)
        onPick (rows[(size_t) row].ref);
}

} // namespace dpm
