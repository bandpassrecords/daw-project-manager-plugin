#include "TodoList.h"

#include "CompanionLookAndFeel.h"

#include <algorithm>

namespace dpm
{

TodoList::TodoList()
{
    list.setModel (this);
    list.setRowHeight (30);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
}

void TodoList::setTodos (std::vector<TodoView> newTodos)
{
    std::stable_partition (newTodos.begin(), newTodos.end(), [] (const TodoView& t) { return ! t.completed; });

    const auto sameRows = newTodos.size() == todos.size()
                          && std::equal (newTodos.begin(), newTodos.end(), todos.begin(),
                                         [] (const TodoView& a, const TodoView& b)
                                         {
                                             return a.todoId == b.todoId && a.text == b.text
                                                    && a.completed == b.completed && a.pending == b.pending;
                                         });
    if (sameRows)
        return;

    todos = std::move (newTodos);
    list.updateContent();
    list.repaint();
    repaint();
}

void TodoList::setEmptyText (const juce::String& text)
{
    if (emptyText != text)
    {
        emptyText = text;
        repaint();
    }
}

void TodoList::paint (juce::Graphics& g)
{
    if (todos.empty())
    {
        g.setColour (palette::textTertiary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText (emptyText, getLocalBounds().reduced (8), juce::Justification::centred, 3);
    }
}

void TodoList::resized()
{
    list.setBounds (getLocalBounds());
}

int TodoList::getNumRows()
{
    return (int) todos.size();
}

void TodoList::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    if (row < 0 || row >= (int) todos.size())
        return;

    const auto& todo = todos[(size_t) row];

    if (row > 0)
    {
        g.setColour (palette::divider.withAlpha (0.5f));
        g.fillRect (0, 0, width, 1);
    }

    const auto boxSize = 16;
    const juce::Rectangle<float> box (4.0f, (float) (height - boxSize) * 0.5f, (float) boxSize, (float) boxSize);

    if (todo.completed)
    {
        g.setColour (palette::primary.brighter (0.1f));
        g.fillRoundedRectangle (box, 4.0f);
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
        tick.lineTo (box.getX() + 7.0f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 4.0f);
        g.setColour (palette::text);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (palette::textSecondary);
        g.drawRoundedRectangle (box, 4.0f, 1.5f);
    }

    auto textArea = juce::Rectangle<int> (boxSize + 14, 0, width - boxSize - 18, height);

    if (todo.pending)
    {
        // A small clock: made here, not yet confirmed by the app.
        const auto clock = textArea.removeFromRight (16).toFloat().withSizeKeepingCentre (11.0f, 11.0f);
        g.setColour (palette::warning);
        g.drawEllipse (clock, 1.2f);
        g.drawLine (clock.getCentreX(), clock.getCentreY(), clock.getCentreX(), clock.getY() + 2.5f, 1.2f);
        g.drawLine (clock.getCentreX(), clock.getCentreY(), clock.getRight() - 2.5f, clock.getCentreY(), 1.2f);
        textArea.removeFromRight (4);
    }

    auto font = juce::FontOptions (14.0f);
    if (todo.pending)
        font = font.withStyle ("Italic");

    g.setFont (font);
    g.setColour (todo.completed ? palette::textTertiary : palette::text);
    g.drawText (todo.text, textArea, juce::Justification::centredLeft, true);

    if (todo.completed)
    {
        const auto textWidth = juce::jmin ((float) textArea.getWidth(),
                                           juce::GlyphArrangement::getStringWidth (juce::Font (font), todo.text));
        g.drawLine ((float) textArea.getX(), (float) height * 0.5f, (float) textArea.getX() + textWidth,
                    (float) height * 0.5f, 1.0f);
    }
}

void TodoList::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < (int) todos.size() && onToggle)
        onToggle (todos[(size_t) row].todoId, ! todos[(size_t) row].completed);
}

juce::String TodoList::getTooltipForRow (int row)
{
    if (row < 0 || row >= (int) todos.size())
        return {};

    const auto& todo = todos[(size_t) row];
    return todo.pending ? todo.text + "\n" + TRANS ("Waiting for the app") : todo.text;
}

} // namespace dpm
