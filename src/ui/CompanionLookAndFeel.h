#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dpm
{

/** The app's Classic Dark palette (theme_provider.dart: classicDarkSpec), so
    the plugin reads as part of DAW Project Manager.
*/
namespace palette
{
    inline const juce::Colour background { 0xff1e1f22 };
    inline const juce::Colour card       { 0xff2b2d31 };
    inline const juce::Colour divider    { 0xff3c3f43 };
    inline const juce::Colour primary    { 0xff5a6b7a };
    inline const juce::Colour text       { 0xffffffff };
    inline const juce::Colour textSecondary = juce::Colours::white.withAlpha (0.70f);
    inline const juce::Colour textTertiary  = juce::Colours::white.withAlpha (0.60f);
    inline const juce::Colour recording  { 0xffe5484d };
    inline const juce::Colour connected  { 0xff46a758 };
    inline const juce::Colour warning    { 0xfff5a524 };
    inline constexpr float radius = 8.0f;
}

/** A middle dot between details. Built from UTF-8 bytes: a non-ASCII
    character in a plain string literal is read as Latin-1 (showing as "A^.")
    and asserts in debug builds. LocalisationTests keeps every literal in
    src/ ASCII.
*/
inline const juce::String detailSeparator = juce::String::fromUTF8 ("  \xc2\xb7  ");

class CompanionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CompanionLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked,
                      bool isEnabled, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

/** A rounded card behind a section, as the app draws its cards. */
void drawCard (juce::Graphics& g, juce::Rectangle<int> area);

} // namespace dpm
