#include "NotesBook.h"

#include <algorithm>

namespace dpm
{

juce::String currentNotes (const IndexedProject* indexed, const std::vector<PendingNotesOp>& pending,
                           const protocol::ProjectRef& project)
{
    for (auto it = pending.rbegin(); it != pending.rend(); ++it)
        if (it->project.sameProject (project))
            return it->text;
    return indexed != nullptr ? indexed->notes : juce::String();
}

bool notesPending (const std::vector<PendingNotesOp>& pending, const protocol::ProjectRef& project)
{
    return std::any_of (pending.begin(), pending.end(),
                        [&project] (const PendingNotesOp& op) { return op.project.sameProject (project); });
}

bool addNotesEdit (std::vector<PendingNotesOp>& pending, const protocol::ProjectRef& project,
                   const juce::String& newText, const juce::String& shownText, juce::Time now)
{
    const auto text = newText.substring (0, maxNotesLength);
    if (! project.isLinked() || text == shownText)
        return false;

    for (auto& op : pending)
    {
        if (op.project.sameProject (project) && ! op.delivered)
        {
            op.text = text;
            op.at = now;
            return true;
        }
    }

    PendingNotesOp op;
    op.opId = protocol::newId();
    op.project = project;
    op.text = text;
    op.baseText = shownText;
    op.at = now;
    pending.push_back (op);
    return true;
}

void pruneAcknowledgedNotes (std::vector<PendingNotesOp>& pending, const ProjectIndex& index)
{
    pending.erase (std::remove_if (pending.begin(), pending.end(), [&index] (const PendingNotesOp& op)
                   {
                       if (! op.delivered)
                           return false;
                       if (index.applied.contains (op.opId))
                           return true;
                       auto* project = index.find (op.project);
                       return project != nullptr && project->notes == op.text;
                   }),
                   pending.end());
}

} // namespace dpm
