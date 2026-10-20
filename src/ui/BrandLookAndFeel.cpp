#include "BrandLookAndFeel.h"

#include <DPMBinaryData.h>

namespace dpm
{

BrandLookAndFeel::BrandLookAndFeel()
{
    using namespace juce;

    setColour (ResizableWindow::backgroundColourId, palette::background);
    setColour (Label::textColourId, palette::text);

    setColour (TextButton::buttonColourId, palette::control);
    setColour (TextButton::buttonOnColourId, palette::recording);
    setColour (TextButton::textColourOffId, palette::text);
    setColour (TextButton::textColourOnId, palette::text);

    setColour (ComboBox::backgroundColourId, palette::background);
    setColour (ComboBox::outlineColourId, palette::divider);
    setColour (ComboBox::textColourId, palette::text);
    setColour (ComboBox::arrowColourId, palette::textSecondary);

    setColour (PopupMenu::backgroundColourId, palette::card);
    setColour (PopupMenu::textColourId, palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, palette::selection);
    setColour (PopupMenu::highlightedTextColourId, palette::text);

    setColour (TextEditor::backgroundColourId, palette::background);
    setColour (TextEditor::textColourId, palette::text);
    setColour (TextEditor::outlineColourId, palette::divider);
    setColour (TextEditor::focusedOutlineColourId, palette::accent);
    setColour (TextEditor::highlightColourId, palette::selection.brighter (0.3f));
    setColour (CaretComponent::caretColourId, palette::text);

    setColour (ToggleButton::textColourId, palette::textSecondary);
    setColour (ToggleButton::tickColourId, palette::text);
    setColour (ToggleButton::tickDisabledColourId, palette::textTertiary);

    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (ScrollBar::thumbColourId, palette::divider);
    setColour (TooltipWindow::backgroundColourId, palette::card.brighter (0.15f));
    setColour (TooltipWindow::textColourId, palette::text);
    setColour (TooltipWindow::outlineColourId, palette::divider);
}

void BrandLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId)
                                          : backgroundColour;

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.4f);
    else if (isButtonDown)
        colour = colour.darker (0.2f);
    else if (isMouseOverButton)
        colour = colour.brighter (0.15f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, palette::radius);
}

void BrandLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                     juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, palette::radius);
    g.setColour (box.hasKeyboardFocus (true) ? palette::accent : palette::divider);
    g.drawRoundedRectangle (bounds, palette::radius, 1.0f);

    juce::Path arrow;
    const auto cx = (float) width - 14.0f, cy = (float) height * 0.5f;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (arrow);
}

void BrandLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f), palette::radius);
}

void BrandLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (editor.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                            : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f),
                            palette::radius, focused ? 1.5f : 1.0f);
}

void BrandLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& component, float x, float y, float w,
                                    float h, bool ticked, bool isEnabled, bool highlighted, bool)
{
    const juce::Rectangle<float> box (x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);
    const auto alpha = isEnabled ? 1.0f : 0.4f;

    if (ticked)
    {
        g.setColour (palette::accent.brighter (highlighted ? 0.15f : 0.0f).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (box, 4.0f);

        juce::Path tick;
        tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
        tick.lineTo (box.getX() + box.getWidth() * 0.42f, box.getBottom() - box.getHeight() * 0.25f);
        tick.lineTo (box.getRight() - box.getWidth() * 0.2f, box.getY() + box.getHeight() * 0.25f);
        g.setColour (component.findColour (juce::ToggleButton::tickColourId).withMultipliedAlpha (alpha));
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour ((highlighted ? palette::textSecondary : palette::textTertiary).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (box, 4.0f, 1.5f);
    }
}

void drawCard (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (palette::card);
    g.fillRoundedRectangle (area.toFloat(), palette::radius);
}

void drawBrandStripe (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Where each band starts along the strip, as the icon's top edge has
    // them: mostly orange, then grey, then navy, with a darker seam between.
    struct Band { float from; juce::Colour colour; };
    const Band bands[] { { 0.0f, brand::orange },  { 0.50f, brand::orangeDeep }, { 0.53f, brand::grey },
                         { 0.76f, brand::navyDeep }, { 0.79f, brand::navy } };
    const auto slant = area.getHeight();

    for (size_t i = 0; i < std::size (bands); ++i)
    {
        const auto x0 = area.getX() + area.getWidth() * bands[i].from;
        const auto x1 = i + 1 < std::size (bands) ? area.getX() + area.getWidth() * bands[i + 1].from
                                                  : area.getRight() + slant;
        juce::Path band;
        band.addQuadrilateral (x0 + slant, area.getY(), x1 + slant, area.getY(),
                               x1, area.getBottom(), x0, area.getBottom());
        g.setColour (bands[i].colour);
        g.fillPath (band);
    }
}

std::unique_ptr<juce::Drawable> createLogo()
{
    return juce::Drawable::createFromImageData (DPMBinaryData::logo_svg, (size_t) DPMBinaryData::logo_svgSize);
}

void makePrimary (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, palette::accent);
    button.setColour (juce::TextButton::textColourOffId, palette::text);
}

} // namespace dpm
