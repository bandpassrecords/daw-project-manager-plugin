#pragma once

#include "core/TodoBook.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace dpm
{

/** The linked project's to-dos: open ones first, a click ticks one off. */
class TodoList : public juce::Component,
                 private juce::ListBoxModel
{
public:
    TodoList();

    std::function<void (const juce::String& todoId, bool completed)> onToggle;

    void setTodos (std::vector<TodoView> newTodos);
    void setEmptyText (const juce::String& text);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::String getTooltipForRow (int row) override;

    juce::ListBox list;
    std::vector<TodoView> todos;
    juce::String emptyText;
};

} // namespace dpm
