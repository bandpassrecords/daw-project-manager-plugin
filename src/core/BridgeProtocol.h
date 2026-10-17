#pragma once

#include <juce_core/juce_core.h>

#include <optional>

/** The messages the plugin leaves for the app. docs/PROTOCOL.md is the
    contract; this file and that one change together.
*/
namespace dpm::protocol
{

inline constexpr int schemaVersion = 1;

namespace type
{
    /** A plugin instance is running inside a project (sent on link and on load). */
    inline constexpr const char* hello = "session.hello";
    inline constexpr const char* todoAdd = "todo.add";
    inline constexpr const char* todoSetCompleted = "todo.setCompleted";
    inline constexpr const char* captureReady = "capture.ready";
}

/** Which project a message is about. Matched by profile *and* id, never id
    alone: one id can sit in two profiles.
*/
struct ProjectRef
{
    juce::String id;
    juce::String profileId;
    juce::String name;
    juce::String filePath;

    bool isLinked() const { return id.isNotEmpty(); }
    bool sameProject (const ProjectRef& other) const
    {
        return isLinked() && id == other.id && profileId == other.profileId;
    }

    juce::var toVar() const;
    static ProjectRef fromVar (const juce::var& v);
};

/** What the plugin can tell about where it runs, sent with every message so
    the app can link a message even when the plugin was never linked.
*/
struct HostContext
{
    juce::String hostName;
    juce::String windowTitle;

    juce::var toVar() const;
};

struct Envelope
{
    juce::String messageId;
    juce::String type;
    juce::Time createdAt;
    juce::String instanceId;
    ProjectRef project;
    HostContext host;
};

/** The full JSON text of one outbox message. */
juce::String toJson (const Envelope& envelope, const juce::var& payload);

juce::var todoAddPayload (const juce::String& todoId, const juce::String& text, juce::Time createdAt);
juce::var todoSetCompletedPayload (const juce::String& todoId, bool completed, juce::Time changedAt);

struct CaptureInfo
{
    juce::String file;          // relative to the bridge root, forward slashes
    juce::String supersedes;    // the capture this one replaces, or empty
    double durationSeconds = 0;
    double sampleRate = 0;
    int channels = 0;
    float peakDbfs = -100.0f;
    bool manual = false;
    juce::Time capturedAt;
};

juce::var captureReadyPayload (const CaptureInfo& info);

/** UTC, millisecond precision, "Z" suffix — what Dart's DateTime.parse reads. */
juce::String formatTime (juce::Time t);

/** Reads what Dart's toIso8601String writes, including microsecond fractions
    and the offset-less local form. Never asserts on bad input (JUCE's own
    parser does, and this text comes from another program).
*/
std::optional<juce::Time> parseTime (const juce::String& text);

juce::String newId();

} // namespace dpm::protocol
