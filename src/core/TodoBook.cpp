#include "TodoBook.h"

#include <algorithm>
#include <cmath>

namespace dpm
{

std::vector<TodoView> buildTodoList (const IndexedProject* indexed,
                                     const std::vector<PendingTodoOp>& pending,
                                     const protocol::ProjectRef& project)
{
    std::vector<TodoView> list;

    if (indexed != nullptr)
        for (auto& t : indexed->todos)
            list.push_back ({ t.id, t.text, t.completed, false });

    auto find = [&list] (const juce::String& todoId) -> TodoView*
    {
        for (auto& v : list)
            if (v.todoId == todoId)
                return &v;
        return nullptr;
    };

    // In the order they were made, so a tick on a to-do added in the plugin
    // lands on it.
    for (auto& op : pending)
    {
        if (! op.project.sameProject (project))
            continue;

        if (op.kind == PendingTodoOp::Kind::add)
        {
            if (find (op.todoId) == nullptr)
                list.push_back ({ op.todoId, op.text, false, true });
        }
        else if (auto* v = find (op.todoId))
        {
            if (v->completed != op.completed)
            {
                v->completed = op.completed;
                v->pending = true;
            }
        }
    }

    return list;
}

void pruneAcknowledged (std::vector<PendingTodoOp>& pending, const ProjectIndex& index)
{
    auto addStillPending = [&pending] (const PendingTodoOp& op)
    {
        return std::any_of (pending.begin(), pending.end(), [&op] (const PendingTodoOp& other)
        {
            return other.kind == PendingTodoOp::Kind::add && other.todoId == op.todoId
                   && other.project.sameProject (op.project);
        });
    };

    std::vector<PendingTodoOp> kept;

    for (auto& op : pending)
    {
        bool acknowledged = false;

        if (op.delivered)
        {
            if (auto* project = index.find (op.project))
            {
                auto* todo = project->findTodo (op.todoId);

                if (op.kind == PendingTodoOp::Kind::add)
                    acknowledged = todo != nullptr;
                else if (todo != nullptr)
                    acknowledged = todo->completed == op.completed && index.generatedAt >= op.at;
                else
                    acknowledged = ! addStillPending (op) && index.generatedAt >= op.at;
            }
        }

        if (! acknowledged)
            kept.push_back (op);
    }

    pending = std::move (kept);
}

std::optional<PendingTodoOp> makeAddOp (const protocol::ProjectRef& project, const juce::String& text, juce::Time now)
{
    // One line: the app's to-do rows are single-line.
    auto clean = text.replaceCharacters ("\r\n\t", "   ").trim();
    while (clean.contains ("  "))
        clean = clean.replace ("  ", " ");

    if (clean.isEmpty() || ! project.isLinked())
        return std::nullopt;

    PendingTodoOp op;
    op.opId = protocol::newId();
    op.kind = PendingTodoOp::Kind::add;
    op.project = project;
    op.todoId = protocol::newId();
    op.text = clean.substring (0, maxTodoLength);
    op.at = now;
    return op;
}

void addSetCompletedOp (std::vector<PendingTodoOp>& pending, const protocol::ProjectRef& project,
                        const juce::String& todoId, bool completed, juce::Time now)
{
    if (! project.isLinked() || todoId.isEmpty())
        return;

    pending.erase (std::remove_if (pending.begin(), pending.end(), [&] (const PendingTodoOp& op)
                   {
                       return op.kind == PendingTodoOp::Kind::setCompleted && ! op.delivered
                              && op.todoId == todoId && op.project.sameProject (project);
                   }),
                   pending.end());

    PendingTodoOp op;
    op.opId = protocol::newId();
    op.kind = PendingTodoOp::Kind::setCompleted;
    op.project = project;
    op.todoId = todoId;
    op.completed = completed;
    op.at = now;
    pending.push_back (op);
}

juce::String formatClock (double seconds)
{
    const auto total = (int) std::floor (juce::jmax (0.0, seconds));
    const auto h = total / 3600, m = (total / 60) % 60, s = total % 60;

    if (h > 0)
        return juce::String::formatted ("%02d:%02d:%02d", h, m, s);
    return juce::String::formatted ("%02d:%02d", m, s);
}

juce::String stampWithTime (const juce::String& text, double playheadSeconds)
{
    return "[" + formatClock (playheadSeconds) + "] " + text;
}

} // namespace dpm
