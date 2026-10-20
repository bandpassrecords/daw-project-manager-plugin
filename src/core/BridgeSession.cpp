#include "BridgeSession.h"

#include "CaptureNaming.h"
#include "Version.h"

#include <algorithm>
#include <cmath>

namespace dpm
{

BridgeSession::BridgeSession (BridgeFolder folder, Clock clock)
    : bridge (std::move (folder)),
      now (std::move (clock)),
      outbox (bridge.outboxDir()),
      current (PluginState::fresh())
{
    startedAt = now();
}

void BridgeSession::restore (PluginState saved)
{
    // The host swapped this instance's state for another project's: the app
    // must hear that it left the one it was in.
    if (presentIn.isLinked() && ! presentIn.sameProject (saved.project))
        goodbyeTo (presentIn);

    current = std::move (saved);
    if (current.instanceId.isEmpty())
        current.instanceId = protocol::newId();
    helloPending = current.project.isLinked();
    if (loadedIndex)
        adoptIndexDetails();
}

void BridgeSession::setCaptureMaxSeconds (int seconds)
{
    const auto& choices = CaptureSettings::lengthChoices();
    if (std::find (choices.begin(), choices.end(), seconds) != choices.end())
        current.capture.maxSeconds = seconds;
}

// --- Reading the app's files -------------------------------------------------

namespace
{
    // Re-read only when the file changed, by modification time and size: the
    // index of a big library is worth not parsing every two seconds.
    bool changedSince (const juce::File& f, juce::Time& modified, juce::int64& size)
    {
        const auto exists = f.existsAsFile();
        const auto m = exists ? f.getLastModificationTime() : juce::Time();
        const auto s = exists ? f.getSize() : (juce::int64) -1;

        if (m == modified && s == size)
            return false;

        modified = m;
        size = s;
        return true;
    }
}

bool BridgeSession::refresh()
{
    bool changed = false;

    if (changedSince (bridge.indexFile(), indexModified, indexSize))
    {
        changed = true;

        if (indexSize < 0)
        {
            loadedIndex.reset();
            lastIndexError.clear();
        }
        else
        {
            juce::String error;
            if (auto parsed = ProjectIndex::parse (bridge.indexFile().loadFileAsString(), error))
            {
                loadedIndex = std::move (parsed);
                lastIndexError.clear();
                pruneAcknowledged (current.pendingTodos, *loadedIndex);
                pruneAcknowledgedNotes (current.pendingNotes, *loadedIndex);
                adoptIndexDetails();
            }
            else
            {
                // Keep the last good index: the app may be halfway through
                // replacing it (it shouldn't — it renames into place — but a
                // sync tool copying the folder might).
                lastIndexError = error;
            }
        }
    }

    if (changedSince (bridge.launchFile(), launchModified, launchSize))
    {
        changed = true;
        launch = launchSize < 0 ? std::nullopt : LaunchHint::parse (bridge.launchFile().loadFileAsString());
    }

    return changed;
}

void BridgeSession::adoptIndexDetails()
{
    // A project renamed or moved in the app keeps its link; take the new
    // name and path so messages carry them.
    if (auto* p = linkedProject())
    {
        current.project.name = p->name;
        current.project.filePath = p->filePath;
    }
}

// --- Linking ------------------------------------------------------------------

void BridgeSession::setHostContext (const protocol::HostContext& context)
{
    host = context;
}

std::vector<MatchCandidate> BridgeSession::suggestions() const
{
    if (! loadedIndex)
        return {};

    DetectionContext ctx;
    ctx.hostName = host.hostName;
    ctx.windowTitle = host.windowTitle;
    ctx.launch = launch;
    ctx.now = now();
    return rankProjects (*loadedIndex, ctx);
}

bool BridgeSession::autoLink()
{
    if (current.linkedByUser)
        return false;

    auto* match = confidentMatch (suggestions());
    if (match == nullptr || current.project.sameProject (match->ref()))
        return false;

    linkTo (*match, false);
    return true;
}

const IndexedProject* BridgeSession::relinkSuggestion() const
{
    if (! current.linkedByUser)
        return nullptr;

    auto* match = confidentMatch (suggestions());
    return match != nullptr && ! current.project.sameProject (match->ref()) ? match : nullptr;
}

void BridgeSession::linkTo (const IndexedProject& project, bool byUser)
{
    const auto changed = ! current.project.sameProject (project.ref());

    current.project = project.ref();
    current.linkedByUser = byUser;

    if (changed)
    {
        // The last capture was another project's: comparing a new take
        // against it would keep this project from getting one.
        current.lastCapture = {};
        helloPending = true;

        if (presentIn.isLinked())
            goodbyeTo (presentIn);
    }
}

void BridgeSession::unlink()
{
    current.project = {};
    current.linkedByUser = false;
    current.lastCapture = {};
    helloPending = false;

    if (presentIn.isLinked())
        goodbyeTo (presentIn);
}

const IndexedProject* BridgeSession::linkedProject() const
{
    return loadedIndex ? loadedIndex->find (current.project) : nullptr;
}

// --- Updates --------------------------------------------------------------------

std::optional<PluginRelease> BridgeSession::availableUpdate (const juce::String& installedVersion) const
{
    if (! loadedIndex || ! loadedIndex->latestRelease)
        return std::nullopt;

    const auto& release = *loadedIndex->latestRelease;
    if (! isNewerVersion (release.version, installedVersion))
        return std::nullopt;
    if (! isReleasePageUrl (release.url))
        return std::nullopt;
    return release;
}

// --- To-dos ---------------------------------------------------------------------

std::vector<TodoView> BridgeSession::todos() const
{
    return buildTodoList (linkedProject(), current.pendingTodos, current.project);
}

bool BridgeSession::addTodo (const juce::String& text, std::optional<double> playheadSeconds)
{
    auto body = text.trim();
    if (body.isEmpty())
        return false;

    if (current.stampTodosWithTime && playheadSeconds && std::isfinite (*playheadSeconds) && *playheadSeconds >= 0)
        body = stampWithTime (body, *playheadSeconds);

    auto op = makeAddOp (current.project, body, now());
    if (! op)
        return false;

    current.pendingTodos.push_back (*op);
    return true;
}

void BridgeSession::setTodoCompleted (const juce::String& todoId, bool completed)
{
    addSetCompletedOp (current.pendingTodos, current.project, todoId, completed, now());
}

// --- Delivery -------------------------------------------------------------------

protocol::Envelope BridgeSession::envelope (const char* type, const protocol::ProjectRef& project, juce::Time at,
                                            const juce::String& messageId) const
{
    protocol::Envelope e;
    e.messageId = messageId.isNotEmpty() ? messageId : protocol::newId();
    e.type = type;
    e.createdAt = at;
    e.instanceId = current.instanceId;
    e.runId = run;
    e.project = project;
    e.host = host;
    return e;
}

bool BridgeSession::send (const protocol::Envelope& e, const juce::var& payload)
{
    return outbox.write (protocol::toJson (e, payload), e.createdAt, e.messageId) != juce::File();
}

int BridgeSession::deliver()
{
    int written = 0;

    if (helloPending && current.project.isLinked())
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("linkedByUser", current.linkedByUser);
        if (! send (envelope (protocol::type::hello, current.project, now()), juce::var (obj)))
            return written;
        helloPending = false;
        presentIn = current.project;
        heartbeat();
        ++written;
    }

    for (auto& op : current.pendingTodos)
    {
        if (op.delivered)
            continue;

        const auto e = envelope (op.kind == PendingTodoOp::Kind::add ? protocol::type::todoAdd
                                                                      : protocol::type::todoSetCompleted,
                                 op.project, op.at);
        const auto payload = op.kind == PendingTodoOp::Kind::add
                                 ? protocol::todoAddPayload (op.todoId, op.text, op.at)
                                 : protocol::todoSetCompletedPayload (op.todoId, op.completed, op.at);

        if (! send (e, payload))
            return written;

        op.delivered = true;
        ++written;
    }

    for (auto& op : current.pendingNotes)
    {
        if (op.delivered)
            continue;

        // The edit's own id is the message id: the app lists it in the
        // index's "applied" once done, which is how the edit is confirmed.
        if (! send (envelope (protocol::type::notesSet, op.project, op.at, op.opId),
                    protocol::notesSetPayload (op.text, op.baseText, op.at)))
            return written;

        op.delivered = true;
        ++written;
    }

    return written;
}

int BridgeSession::undeliveredCount() const
{
    return (int) std::count_if (current.pendingTodos.begin(), current.pendingTodos.end(),
                                [] (const PendingTodoOp& op) { return ! op.delivered; })
         + (int) std::count_if (current.pendingNotes.begin(), current.pendingNotes.end(),
                                [] (const PendingNotesOp& op) { return ! op.delivered; });
}

// --- Notes ----------------------------------------------------------------------

juce::String BridgeSession::notes() const
{
    return currentNotes (linkedProject(), current.pendingNotes, current.project);
}

juce::String BridgeSession::appNotes() const
{
    auto* p = linkedProject();
    return p != nullptr ? p->notes : juce::String();
}

bool BridgeSession::notesPending() const
{
    return dpm::notesPending (current.pendingNotes, current.project);
}

bool BridgeSession::setNotes (const juce::String& text)
{
    return addNotesEdit (current.pendingNotes, current.project, text, notes(), now());
}

// --- Presence -------------------------------------------------------------------

void BridgeSession::heartbeat()
{
    if (! presentIn.isLinked())
    {
        heartbeatFile().deleteFile();
        return;
    }

    auto e = envelope (protocol::type::hello, presentIn, now());
    writeFileAtomically (heartbeatFile(), protocol::heartbeatJson (e, now(), startedAt));
}

void BridgeSession::sayGoodbye()
{
    if (presentIn.isLinked())
        goodbyeTo (presentIn);
    heartbeatFile().deleteFile();
}

void BridgeSession::goodbyeTo (const protocol::ProjectRef& project)
{
    // Best effort and not queued: a goodbye that can't be written is covered
    // by the heartbeat going stale, which the app notices on its own.
    send (envelope (protocol::type::goodbye, project, now()), juce::var (new juce::DynamicObject()));
    presentIn = {};
    heartbeatFile().deleteFile();
}

// --- Captures -------------------------------------------------------------------

bool moveIntoPlace (const juce::File& source, const juce::File& target)
{
    if (! target.getParentDirectory().createDirectory())
        return false;

    if (source.moveFileTo (target))
        return true;

    // Another volume: copy under a name the app skips, then rename, so the
    // app never sees a half-copied file under the real name.
    const auto partial = target.getSiblingFile ("." + target.getFileName() + ".part");
    if (source.copyFileTo (partial) && partial.moveFileTo (target))
    {
        source.deleteFile();
        return true;
    }

    partial.deleteFile();
    return false;
}

BridgeSession::CaptureOutcome BridgeSession::handleFinishedTake (const FinishedTakeFile& take)
{
    CaptureOutcome outcome;
    outcome.verdict = judgeTake (take.summary, current.lastCapture);

    if (outcome.verdict != TakeVerdict::keep)
    {
        take.file.deleteFile();
        return outcome;
    }

    const auto at = now();
    auto target = bridge.capturesDir().getChildFile (captureFileName (current.project.name, at));
    if (target.exists())
        target = target.getNonexistentSibling (false);

    if (! moveIntoPlace (take.file, target))
    {
        take.file.deleteFile();
        outcome.verdict = TakeVerdict::discardIncomplete;
        return outcome;
    }

    // One capture per instance: the one it replaces goes, so a folder nobody
    // empties (no app installed) holds one file per project, not one per pass.
    const auto previous = current.lastCapture;
    if (previous.exists())
    {
        const auto old = bridge.root.getChildFile (previous.file);
        if (old.isAChildOf (bridge.capturesDir()) && old != target)
            old.deleteFile();
    }

    current.lastCapture.file = bridge.relativePath (target);
    current.lastCapture.seconds = take.summary.seconds;
    current.lastCapture.reachedMax = take.summary.end == TakeEnd::reachedMax;
    current.lastCapture.capturedAt = at;

    protocol::CaptureInfo info;
    info.file = current.lastCapture.file;
    info.supersedes = previous.exists() ? previous.file : juce::String();
    info.durationSeconds = take.summary.seconds;
    info.sampleRate = take.sampleRate;
    info.channels = take.channels;
    info.peakDbfs = juce::Decibels::gainToDecibels (take.summary.peak, -100.0f);
    info.manual = take.summary.manual;
    info.capturedAt = at;

    // Not queued for a retry: the outbox and the captures folder share the
    // bridge root, so the move above succeeding means the folder is writable.
    // Only a disk filling up between the two lines loses the announcement.
    send (envelope (protocol::type::captureReady, current.project, at), protocol::captureReadyPayload (info));

    outcome.file = target;
    return outcome;
}

} // namespace dpm
