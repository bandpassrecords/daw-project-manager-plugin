#pragma once

#include "plugin/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace dpm
{

/** The Standalone build's pretend DAW, for testing without one: a transport
    (play, loop, a test tone to capture), the DAW's name and window title (to
    try auto-linking), and its lifecycle (close or reopen the project, crash,
    recover).

    A developer tool: English only and never part of the VST3/AU — the
    editor creates it only when the processor runs as the Standalone.
*/
class TestPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit TestPanel (CompanionProcessor& processor);

    static constexpr int preferredHeight = 150;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyHost();
    void refresh();

    CompanionProcessor& processor;

    juce::Label heading, position, hostLabel, titleLabel, status;
    juce::TextButton playButton, rewindButton;
    juce::ToggleButton loopToggle { "Loop" }, toneToggle { "Test tone" };
    juce::TextEditor hostEditor, titleEditor;
    juce::TextButton closeButton { "Close project" }, reopenButton { "Reopen project" },
                     crashButton { "Crash DAW" }, recoverButton { "Recover" };
};

} // namespace dpm
