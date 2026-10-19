#pragma once

#include "BridgeProtocol.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace dpm
{

enum class CaptureMode
{
    off,
    automatic,
    manual
};

struct CaptureSettings
{
    CaptureMode mode = CaptureMode::automatic;
    int maxSeconds = 60;

    /** The length choices the UI offers. */
    static const std::vector<int>& lengthChoices();
};

/** The capture this instance last handed to the app, for "keep the better
    take" and for replacing its file when a better one comes along.
*/
struct LastCapture
{
    juce::String file;      // relative to the bridge root
    double seconds = 0;
    bool reachedMax = false;
    juce::Time capturedAt;

    bool exists() const { return file.isNotEmpty(); }
};

/** A to-do change made in the plugin that the app hasn't confirmed yet.

    Kept in the plugin's saved state — so in the DAW project — until the
    app's index shows it, which is what makes a to-do typed while the app is
    closed (or not even installed) survive until the app picks it up.
*/
struct PendingTodoOp
{
    enum class Kind
    {
        add,
        setCompleted
    };

    juce::String opId;
    Kind kind = Kind::add;
    protocol::ProjectRef project;   // the op stays with this project even if the instance is relinked
    juce::String todoId;
    juce::String text;              // add only
    bool completed = false;         // setCompleted only
    juce::Time at;
    bool delivered = false;         // written to the outbox
};

/** A notes edit made in the plugin that the app hasn't confirmed yet. Like
    to-do ops, kept in the saved state until the app's index says it was
    applied.
*/
struct PendingNotesOp
{
    juce::String opId;              // also the message id the app reports back
    protocol::ProjectRef project;
    juce::String text;
    juce::String baseText;          // what the notes said when this edit began
    juce::Time at;
    bool delivered = false;
};

/** One plugin instance's saved state. */
struct CompanionState
{
    juce::String instanceId;
    protocol::ProjectRef project;

    /** Picked by hand: automatic detection never replaces a manual choice. */
    bool linkedByUser = false;

    CaptureSettings capture;
    LastCapture lastCapture;
    bool stampTodosWithTime = false;
    std::vector<PendingTodoOp> pendingTodos;
    std::vector<PendingNotesOp> pendingNotes;

    juce::ValueTree toValueTree() const;

    /** Missing or unreadable values fall back to the defaults; an empty
        instance id gets a fresh one.
    */
    static CompanionState fromValueTree (const juce::ValueTree& tree);

    static CompanionState fresh();
};

} // namespace dpm
