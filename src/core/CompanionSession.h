#pragma once

#include "BridgePaths.h"
#include "CaptureEngine.h"
#include "CompanionState.h"
#include "Outbox.h"
#include "ProjectMatcher.h"
#include "TodoBook.h"

#include <functional>
#include <optional>

namespace dpm
{

/** Everything one plugin instance does with the bridge folder: reading the
    app's index, linking to a project, keeping to-dos, delivering messages and
    handing over captures.

    It touches files and nothing else — no timers, no GUI, no host — so the
    whole conversation with the app can be tested against a temp folder. The
    plugin drives it from the message thread (BridgeClient).
*/
class CompanionSession
{
public:
    using Clock = std::function<juce::Time()>;

    explicit CompanionSession (BridgeFolder folder, Clock clock = [] { return juce::Time::getCurrentTime(); });

    // --- State --------------------------------------------------------------

    const CompanionState& state() const { return current; }

    /** Restores a saved state (the DAW project being opened). A restored link
        says hello again: the app learns the project is open.
    */
    void restore (CompanionState saved);

    void setCaptureMode (CaptureMode mode)  { current.capture.mode = mode; }
    void setCaptureMaxSeconds (int seconds);
    void setStampTodosWithTime (bool on)    { current.stampTodosWithTime = on; }

    // --- Reading the app's files ----------------------------------------------

    /** Re-reads index.json and launch.json if they changed. Returns true when
        anything the UI shows may have changed.
    */
    bool refresh();

    /** Null until the app has written a readable index. */
    const ProjectIndex* index() const { return loadedIndex ? &*loadedIndex : nullptr; }
    const juce::String& indexError() const { return lastIndexError; }

    // --- Linking --------------------------------------------------------------

    void setHostContext (const protocol::HostContext& context);
    const protocol::HostContext& hostContext() const { return host; }

    /** Projects the host context points at, best first. */
    std::vector<MatchCandidate> suggestions() const;

    /** Links to the confident match unless the user picked a project by hand.
        Follows the detection when it changes (a "save as" to a project the
        app knows). Returns true when the link changed.
    */
    bool autoLink();

    /** When the user picked by hand and detection confidently says another
        project, that project — for the UI to offer, never to act on.
    */
    const IndexedProject* relinkSuggestion() const;

    void linkTo (const IndexedProject& project, bool byUser);
    void unlink();

    /** The linked project's entry in the index, or null. */
    const IndexedProject* linkedProject() const;

    // --- To-dos -----------------------------------------------------------------

    std::vector<TodoView> todos() const;

    /** Adds a to-do to the linked project, stamped with the playhead time when
        that option is on and a time is known. False when nothing was added.
    */
    bool addTodo (const juce::String& text, std::optional<double> playheadSeconds);
    void setTodoCompleted (const juce::String& todoId, bool completed);

    // --- Delivery ---------------------------------------------------------------

    /** Writes everything not yet delivered to the outbox, oldest first,
        stopping at the first failure so the app never sees them out of order.
        Returns the number of messages written.
    */
    int deliver();

    /** Ops waiting to be written to the outbox (the app may be uninstalled
        or the folder unwritable).
    */
    int undeliveredCount() const;

    // --- Captures ---------------------------------------------------------------

    struct CaptureOutcome
    {
        TakeVerdict verdict = TakeVerdict::discardEnded;
        juce::File file;
    };

    /** Judges a finished take; a kept one moves into the captures folder,
        replaces this instance's previous capture and is announced to the app.
    */
    CaptureOutcome handleFinishedTake (const FinishedTakeFile& take);

    const BridgeFolder& folder() const { return bridge; }

private:
    protocol::Envelope envelope (const char* type, const protocol::ProjectRef& project, juce::Time at) const;
    bool send (const protocol::Envelope& envelope, const juce::var& payload);
    void adoptIndexDetails();

    BridgeFolder bridge;
    Clock now;
    Outbox outbox;
    CompanionState current;
    protocol::HostContext host;

    std::optional<ProjectIndex> loadedIndex;
    juce::String lastIndexError;
    juce::Time indexModified, launchModified;
    juce::int64 indexSize = -1, launchSize = -1;
    std::optional<LaunchHint> launch;

    bool helloPending = false;
};

/** Moves a file, copying across volumes when a rename can't. The target
    appears complete or not at all.
*/
bool moveIntoPlace (const juce::File& source, const juce::File& target);

} // namespace dpm
