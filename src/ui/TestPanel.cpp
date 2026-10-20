#include "TestPanel.h"

#include "BrandLookAndFeel.h"
#include "core/TodoBook.h"

namespace dpm
{

namespace
{
    const juce::Colour panelColor = palette::warning.withAlpha (0.12f);

    void small (juce::Label& l, const juce::String& text, juce::Colour color = palette::textSecondary)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (12.0f));
        l.setColour (juce::Label::textColourId, color);
    }
}

TestPanel::TestPanel (PluginProcessor& p) : processor (p)
{
    heading.setText ("STANDALONE TEST PANEL" + detailSeparator + "a pretend DAW", juce::dontSendNotification);
    heading.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    heading.setColour (juce::Label::textColourId, palette::warning);
    addAndMakeVisible (heading);

    auto& transport = processor.testTransportForStandalone();

    playButton.onClick = [this, &transport]
    {
        transport.setPlaying (! transport.isPlaying());
        refresh();
    };
    rewindButton.setButtonText ("Rewind");
    rewindButton.onClick = [&transport] { transport.rewind(); };
    loopToggle.setTooltip ("Pretend the DAW cycles a loop: an automatic capture is not kept.");
    loopToggle.onClick = [this, &transport] { transport.setLooping (loopToggle.getToggleState()); };
    toneToggle.setTooltip ("Plays a quiet 220 Hz tone while the transport runs, for the capture to record.");
    toneToggle.setToggleState (transport.isToneOn(), juce::dontSendNotification);
    toneToggle.onClick = [this, &transport] { transport.setToneOn (toneToggle.getToggleState()); };

    small (position, "00:00");
    position.setJustificationType (juce::Justification::centredRight);

    small (hostLabel, "DAW");
    small (titleLabel, "Window title");
    hostEditor.setText ("Steinberg Cubase 13", false);
    titleEditor.setTextToShowWhenEmpty ("e.g. Cubase Pro Project - <project name>", palette::textTertiary);
    for (auto* e : { &hostEditor, &titleEditor })
    {
        e->setIndents (6, 3);
        e->onReturnKey = [this] { applyHost(); };
        e->onFocusLost = [this] { applyHost(); };
    }

    closeButton.setTooltip ("The DAW closes the project: the plugin says goodbye. The app ends the session at once.");
    closeButton.onClick = [this] { processor.bridge().simulateProjectClosed(); refresh(); };
    reopenButton.setTooltip ("The project opens again: the plugin says hello.");
    reopenButton.onClick = [this] { processor.bridge().simulateProjectReopened(); refresh(); };
    crashButton.setTooltip ("The DAW crashes: no goodbye, the heartbeat stops. The app ends the session about 90 s later.");
    crashButton.setColour (juce::TextButton::buttonColourId, palette::recording.darker (0.3f));
    crashButton.onClick = [this] { processor.bridge().simulateCrash(); refresh(); };
    recoverButton.setTooltip ("Back from the crash, as a freshly started DAW.");
    recoverButton.onClick = [this] { processor.bridge().simulateRecovery(); refresh(); };

    small (status, {}, palette::textTertiary);

    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &rewindButton, &loopToggle, &toneToggle,
                                                            &position, &hostLabel, &hostEditor, &titleLabel,
                                                            &titleEditor, &closeButton, &reopenButton, &crashButton,
                                                            &recoverButton, &status })
        addAndMakeVisible (c);

    applyHost();
    refresh();
    startTimerHz (10);
}

void TestPanel::applyHost()
{
    processor.bridge().setTestHost (hostEditor.getText(), titleEditor.getText());
}

void TestPanel::refresh()
{
    auto& transport = processor.testTransportForStandalone();
    playButton.setButtonText (transport.isPlaying() ? "Stop" : "Play");
    playButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
    position.setText (formatClock (transport.positionSeconds (processor.getSampleRate())), juce::dontSendNotification);

    const auto crashed = processor.bridge().isCrashed();
    crashButton.setEnabled (! crashed);
    recoverButton.setEnabled (crashed);
    closeButton.setEnabled (! crashed);
    reopenButton.setEnabled (! crashed);
    status.setText (crashed ? "Crashed: nothing is sent; the heartbeat goes stale in 90 s." : juce::String(),
                    juce::dontSendNotification);
}

void TestPanel::timerCallback()
{
    refresh();
}

void TestPanel::paint (juce::Graphics& g)
{
    g.setColour (panelColor);
    g.fillRect (getLocalBounds());
    g.setColour (palette::warning.withAlpha (0.5f));
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void TestPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 6);
    heading.setBounds (area.removeFromTop (16));
    area.removeFromTop (4);

    auto row = area.removeFromTop (26);
    playButton.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (6);
    rewindButton.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (10);
    loopToggle.setBounds (row.removeFromLeft (64));
    toneToggle.setBounds (row.removeFromLeft (96));
    position.setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (24);
    hostLabel.setBounds (row.removeFromLeft (34));
    hostEditor.setBounds (row.removeFromLeft (juce::jmin (150, row.getWidth() / 3)));
    row.removeFromLeft (8);
    titleLabel.setBounds (row.removeFromLeft (80));
    titleEditor.setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (26);
    const auto w = (row.getWidth() - 18) / 4;
    for (auto* b : { &closeButton, &reopenButton, &crashButton, &recoverButton })
    {
        b->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (6);
    }
    status.setBounds (area);
}

} // namespace dpm
