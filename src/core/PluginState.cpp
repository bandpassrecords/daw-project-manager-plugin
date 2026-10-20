#include "PluginState.h"

#include <algorithm>

namespace dpm
{

namespace ids
{
    static const juce::Identifier state ("DAWProjectManagerState");
    static const juce::Identifier stateVersion ("stateVersion");
    static const juce::Identifier instanceId ("instanceId");
    static const juce::Identifier project ("Project");
    static const juce::Identifier id ("id");
    static const juce::Identifier profileId ("profileId");
    static const juce::Identifier name ("name");
    static const juce::Identifier filePath ("filePath");
    static const juce::Identifier linkedByUser ("linkedByUser");
    static const juce::Identifier capture ("Capture");
    static const juce::Identifier mode ("mode");
    static const juce::Identifier maxSeconds ("maxSeconds");
    static const juce::Identifier lastCapture ("LastCapture");
    static const juce::Identifier file ("file");
    static const juce::Identifier seconds ("seconds");
    static const juce::Identifier reachedMax ("reachedMax");
    static const juce::Identifier capturedAt ("capturedAt");
    static const juce::Identifier stampTodos ("stampTodosWithTime");
    static const juce::Identifier pending ("PendingTodos");
    static const juce::Identifier op ("Op");
    static const juce::Identifier opId ("opId");
    static const juce::Identifier kind ("kind");
    static const juce::Identifier todoId ("todoId");
    static const juce::Identifier text ("text");
    static const juce::Identifier completed ("completed");
    static const juce::Identifier at ("at");
    static const juce::Identifier delivered ("delivered");
    static const juce::Identifier pendingNotes ("PendingNotes");
    static const juce::Identifier notesOp ("NotesOp");
    static const juce::Identifier baseText ("baseText");
}

const std::vector<int>& CaptureSettings::lengthChoices()
{
    static const std::vector<int> choices { 30, 60, 120, 300 };
    return choices;
}

namespace
{
    constexpr int currentStateVersion = 1;

    juce::ValueTree projectTree (const protocol::ProjectRef& p)
    {
        return juce::ValueTree (ids::project, { { ids::id, p.id },
                                                { ids::profileId, p.profileId },
                                                { ids::name, p.name },
                                                { ids::filePath, p.filePath } });
    }

    protocol::ProjectRef projectFrom (const juce::ValueTree& t)
    {
        return { t[ids::id].toString(), t[ids::profileId].toString(), t[ids::name].toString(),
                 t[ids::filePath].toString() };
    }

    juce::String modeName (CaptureMode m)
    {
        switch (m)
        {
            case CaptureMode::off:       return "off";
            case CaptureMode::manual:    return "manual";
            case CaptureMode::automatic: break;
        }
        return "auto";
    }

    CaptureMode modeFrom (const juce::String& s)
    {
        if (s == "off")    return CaptureMode::off;
        if (s == "manual") return CaptureMode::manual;
        return CaptureMode::automatic;
    }

    juce::Time timeFrom (const juce::var& v)
    {
        if (auto t = protocol::parseTime (v.toString()))
            return *t;
        return {};
    }
}

juce::ValueTree PluginState::toValueTree() const
{
    juce::ValueTree tree (ids::state);
    tree.setProperty (ids::stateVersion, currentStateVersion, nullptr);
    tree.setProperty (ids::instanceId, instanceId, nullptr);
    tree.setProperty (ids::linkedByUser, linkedByUser, nullptr);
    tree.setProperty (ids::stampTodos, stampTodosWithTime, nullptr);
    tree.appendChild (projectTree (project), nullptr);

    tree.appendChild (juce::ValueTree (ids::capture, { { ids::mode, modeName (capture.mode) },
                                                       { ids::maxSeconds, capture.maxSeconds } }),
                      nullptr);

    tree.appendChild (juce::ValueTree (ids::lastCapture, { { ids::file, lastCapture.file },
                                                           { ids::seconds, lastCapture.seconds },
                                                           { ids::reachedMax, lastCapture.reachedMax },
                                                           { ids::capturedAt, lastCapture.exists()
                                                                                  ? protocol::formatTime (lastCapture.capturedAt)
                                                                                  : juce::String() } }),
                      nullptr);

    juce::ValueTree pendingTree (ids::pending);
    for (auto& op : pendingTodos)
    {
        juce::ValueTree t (ids::op, { { ids::opId, op.opId },
                                      { ids::kind, op.kind == PendingTodoOp::Kind::add ? "add" : "setCompleted" },
                                      { ids::todoId, op.todoId },
                                      { ids::text, op.text },
                                      { ids::completed, op.completed },
                                      { ids::at, protocol::formatTime (op.at) },
                                      { ids::delivered, op.delivered } });
        t.appendChild (projectTree (op.project), nullptr);
        pendingTree.appendChild (t, nullptr);
    }
    tree.appendChild (pendingTree, nullptr);

    juce::ValueTree notesTree (ids::pendingNotes);
    for (auto& op : pendingNotes)
    {
        juce::ValueTree t (ids::notesOp, { { ids::opId, op.opId },
                                           { ids::text, op.text },
                                           { ids::baseText, op.baseText },
                                           { ids::at, protocol::formatTime (op.at) },
                                           { ids::delivered, op.delivered } });
        t.appendChild (projectTree (op.project), nullptr);
        notesTree.appendChild (t, nullptr);
    }
    tree.appendChild (notesTree, nullptr);

    return tree;
}

PluginState PluginState::fromValueTree (const juce::ValueTree& tree)
{
    auto s = fresh();
    if (! tree.hasType (ids::state))
        return s;

    if (auto id = tree[ids::instanceId].toString(); id.isNotEmpty())
        s.instanceId = id;

    s.project = projectFrom (tree.getChildWithName (ids::project));
    s.linkedByUser = s.project.isLinked() && (bool) tree[ids::linkedByUser];
    s.stampTodosWithTime = (bool) tree[ids::stampTodos];

    const auto capture = tree.getChildWithName (ids::capture);
    s.capture.mode = modeFrom (capture[ids::mode].toString());
    const auto& choices = CaptureSettings::lengthChoices();
    const auto max = (int) capture.getProperty (ids::maxSeconds, s.capture.maxSeconds);
    if (std::find (choices.begin(), choices.end(), max) != choices.end())
        s.capture.maxSeconds = max;

    const auto last = tree.getChildWithName (ids::lastCapture);
    s.lastCapture.file = last[ids::file].toString();
    s.lastCapture.seconds = (double) last[ids::seconds];
    s.lastCapture.reachedMax = (bool) last[ids::reachedMax];
    s.lastCapture.capturedAt = timeFrom (last[ids::capturedAt]);

    for (auto t : tree.getChildWithName (ids::pending))
    {
        PendingTodoOp op;
        op.opId = t[ids::opId].toString();
        op.kind = t[ids::kind].toString() == "setCompleted" ? PendingTodoOp::Kind::setCompleted
                                                             : PendingTodoOp::Kind::add;
        op.todoId = t[ids::todoId].toString();
        op.text = t[ids::text].toString();
        op.completed = (bool) t[ids::completed];
        op.at = timeFrom (t[ids::at]);
        op.delivered = (bool) t[ids::delivered];
        op.project = projectFrom (t.getChildWithName (ids::project));

        if (op.opId.isNotEmpty() && op.todoId.isNotEmpty() && op.project.isLinked())
            s.pendingTodos.push_back (op);
    }

    for (auto t : tree.getChildWithName (ids::pendingNotes))
    {
        PendingNotesOp op;
        op.opId = t[ids::opId].toString();
        op.text = t[ids::text].toString();
        op.baseText = t[ids::baseText].toString();
        op.at = timeFrom (t[ids::at]);
        op.delivered = (bool) t[ids::delivered];
        op.project = projectFrom (t.getChildWithName (ids::project));

        if (op.opId.isNotEmpty() && op.project.isLinked())
            s.pendingNotes.push_back (op);
    }

    return s;
}

PluginState PluginState::fresh()
{
    PluginState s;
    s.instanceId = protocol::newId();
    return s;
}

} // namespace dpm
