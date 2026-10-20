#pragma once

#include "CompanionLookAndFeel.h"
#include "ProjectPicker.h"
#include "TestPanel.h"
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
    void saveNotes();
    void showNotes (bool notes);
    void refreshNotesStatus();

    CompanionProcessor& processor;
    CompanionLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::Drawable> logo;
    juce::Label title, subtitle, appStatus;

    juce::Label projectHeading, projectName, projectPath, relinkHint;
    juce::TextButton changeProjectButton, relinkButton;
    protocol::ProjectRef relinkTarget;

    juce::Label captureHeading, modeLabel, lengthLabel, captureStatus;
    juce::ComboBox modeBox, lengthBox;
    juce::TextButton recordButton;
    LevelMeter meter;

    // The bottom card: the project's to-dos, or its notes.
    juce::TextButton todosTab, notesTab;
    bool showingNotes = false;
    TodoList todoList;
    juce::TextEditor todoInput;
    juce::TextButton addTodoButton;
    juce::ToggleButton stampToggle;

    juce::TextEditor notesEditor;
    juce::TextButton saveNotesButton, reloadNotesButton;
    juce::Label notesStatus;
    /** The text last loaded into the editor from the session: typing makes
        the editor differ from it ("dirty"), and a dirty editor is never
        overwritten by what the app sends.
    */
    juce::String notesLoaded;
    bool notesDirty() const { return notesEditor.getText() != notesLoaded; }

    juce::Label footer;
    juce::HyperlinkButton updateLink;
    ProjectPicker picker;
    std::unique_ptr<TestPanel> testPanel;   // Standalone only

    bool appConnected = false;
    bool projectLinked = false;
    bool recordingShown = false;
    juce::Rectangle<int> stripe, logoArea, statusPill, projectCard, captureCard, todoCard, tabsRow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionEditor)
};

} // namespace dpm
