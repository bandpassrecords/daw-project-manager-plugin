#pragma once

#include "core/ProjectMatcher.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace dpm
{

/** The panel that links the plugin to a project: a search field over the
    app's projects, suggestions first.
*/
class ProjectPicker : public juce::Component,
                      private juce::ListBoxModel
{
public:
    ProjectPicker();

    std::function<void (const protocol::ProjectRef&)> onPick;
    std::function<void()> onUnlink;
    std::function<void()> onClose;

    /** Called with the query whenever it changes; returns the rows to show. */
    std::function<std::vector<PickerRow> (const juce::String& query)> rowsFor;

    void refresh (bool appHasSharedProjects, bool isLinked);

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void pickRow (int row);

    juce::Label heading;
    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton pickButton, unlinkButton, closeButton;
    std::vector<PickerRow> rows;
    bool hasIndex = false;
};

} // namespace dpm
