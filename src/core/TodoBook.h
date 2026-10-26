#pragma once

#include "PluginState.h"
#include "ProjectIndex.h"

#include <optional>
#include <vector>

namespace dpm
{

/** One row of the plugin's to-do list. */
struct TodoView
{
    juce::String todoId;
    juce::String text;
    bool completed = false;

    /** Made or changed in the plugin and not yet in the app's index. */
    bool pending = false;
};

/** The project's to-dos as the user should see them: the app's list with the
    plugin's unconfirmed changes laid over it, and to-dos added in the plugin
    after the app's ones.
*/
std::vector<TodoView> buildTodoList (const IndexedProject* indexed,
                                     const std::vector<PendingTodoOp>& pending,
                                     const protocol::ProjectRef& project);

/** Drops the delivered ops the app has dealt with:
      - any op the index lists in `applied`: the app handled it, whether it
        applied it or kept a newer change of its own;
      - an add, once its to-do is in the index;
      - a completion change, once the index (written after the change) agrees,
        or once the to-do is gone from a project the index still lists —
        deleted in the app, which beats a tick made in the plugin;
      - any op an index written `unconfirmedChangeTimeout` after it still
        doesn't confirm (sent by 0.1.0, under an id `applied` can't name).
    Ops not yet delivered are never dropped, whatever the index says.
*/
void pruneAcknowledged (std::vector<PendingTodoOp>& pending, const ProjectIndex& index);

/** A new to-do. Returns nothing for blank text. */
std::optional<PendingTodoOp> makeAddOp (const protocol::ProjectRef& project, const juce::String& text, juce::Time now);

/** Records a completion change, replacing an undelivered change to the same
    to-do rather than queueing both.
*/
void addSetCompletedOp (std::vector<PendingTodoOp>& pending, const protocol::ProjectRef& project,
                        const juce::String& todoId, bool completed, juce::Time now);

/** "[02:31] text": the playhead time in front of a to-do, so it points at the
    spot in the song it is about. The same text the app's own "Add to-do at
    timestamp" writes (playback_todo_utils.dart: buildTimestampedTodoText),
    so the two read alike in the list.
*/
juce::String stampWithTime (const juce::String& text, double playheadSeconds);

/** "02:31", or "01:02:31" past an hour (formatPlaybackTimestamp in the app). */
juce::String formatClock (double seconds);

/** A to-do's text with its playhead stamp taken off, so the list can show
    the time apart. A text without the app's "[MM:SS] " or "[HH:MM:SS] "
    in front has no time and comes back whole.
*/
struct StampedText
{
    juce::String time;
    juce::String text;
};

StampedText splitTimeStamp (const juce::String& text);

/** How long after a delivered change an index that neither shows it nor
    lists it as applied stops it waiting. The app lists handled messages for
    30 minutes; past this, it has had the message for longer than that.
*/
inline const juce::RelativeTime unconfirmedChangeTimeout = juce::RelativeTime::hours (1);

/** Longest to-do text the plugin sends. */
inline constexpr int maxTodoLength = 500;

} // namespace dpm
