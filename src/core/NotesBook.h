#pragma once

#include "CompanionState.h"
#include "ProjectIndex.h"

#include <vector>

namespace dpm
{

/** The project's notes as the plugin should show them: its own newest edit
    the app hasn't confirmed yet, else the app's.
*/
juce::String currentNotes (const IndexedProject* indexed, const std::vector<PendingNotesOp>& pending,
                           const protocol::ProjectRef& project);

/** Whether an edit of this project's notes is still waiting for the app. */
bool notesPending (const std::vector<PendingNotesOp>& pending, const protocol::ProjectRef& project);

/** Records an edit of the notes from `shownText` to `newText`. An edit not
    yet delivered is replaced (keeping the text it began from), so typing and
    saving again sends one message, not one per save. Returns false when
    nothing changed or the project isn't linked.
*/
bool addNotesEdit (std::vector<PendingNotesOp>& pending, const protocol::ProjectRef& project,
                   const juce::String& newText, const juce::String& shownText, juce::Time now);

/** Drops the delivered edits the app has applied: their id is in the
    index's `applied` list, or the app's notes now read exactly as edited.
    Edits not yet delivered are never dropped.
*/
void pruneAcknowledgedNotes (std::vector<PendingNotesOp>& pending, const ProjectIndex& index);

/** Longest notes text the plugin sends. */
inline constexpr int maxNotesLength = 20000;

} // namespace dpm
