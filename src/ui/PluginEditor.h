#pragma once

#include "CompanionLookAndFeel.h"
#include "ProjectPicker.h"
#include "TodoList.h"
#include "plugin/PluginProcessor.h"

namespace dpm
{

/** A horizontal level meter with a decaying peak. */
class LevelMeter : public juce::Component
{
public:
    void push (float peak);
    void setRecording (bool isRecording);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
    bool recording = false;
};

class CompanionEditor : public juce::AudioProcessorEditor,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit CompanionEditor (CompanionProcessor&);
    ~CompanionEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void refreshFromSession();
    void refreshCaptureStatus();
    void showPicker (bool show);
    void addTodo();

    CompanionProcessor& processor;
    CompanionLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Label title, subtitle, appStatus;

    juce::Label projectHeading, projectName, projectPath, relinkHint;
    juce::TextButton changeProjectButton, relinkButton;
    protocol::ProjectRef relinkTarget;

    juce::Label captureHeading, modeLabel, lengthLabel, captureStatus;
    juce::ComboBox modeBox, lengthBox;
    juce::TextButton recordButton;
    LevelMeter meter;

    juce::Label todoHeading;
    TodoList todoList;
    juce::TextEditor todoInput;
    juce::TextButton addTodoButton;
    juce::ToggleButton stampToggle;

    juce::Label footer;
    ProjectPicker picker;

    bool appConnected = false;
    juce::Rectangle<int> projectCard, captureCard, todoCard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionEditor)
};

} // namespace dpm
