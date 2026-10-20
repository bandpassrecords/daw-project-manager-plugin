#include "BridgeDriver.h"

#include "CaptureNaming.h"
#include "CaptureWriter.h"

namespace dpm
{

namespace
{
    /** A command that can't be carried out: becomes {"ok":false,"error":…}. */
    struct CommandError
    {
        juce::String message;
    };

    juce::var object (std::initializer_list<std::pair<const char*, juce::var>> properties = {})
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [name, value] : properties)
            o->setProperty (name, value);
        return juce::var (o);
    }

    juce::String requireString (const juce::var& args, const char* name)
    {
        const auto& v = args[name];
        if (! v.isString())
            throw CommandError { juce::String ("\"") + name + "\" must be a string" };
        return v.toString();
    }

    bool requireBool (const juce::var& args, const char* name)
    {
        const auto& v = args[name];
        if (! v.isBool())
            throw CommandError { juce::String ("\"") + name + "\" must be true or false" };
        return (bool) v;
    }

    std::optional<double> optionalNumber (const juce::var& args, const char* name)
    {
        const auto& v = args[name];
        if (v.isVoid() || v.isUndefined())
            return std::nullopt;
        if (! (v.isDouble() || v.isInt() || v.isInt64()))
            throw CommandError { juce::String ("\"") + name + "\" must be a number" };
        return (double) v;
    }

    bool optionalBool (const juce::var& args, const char* name, bool fallback)
    {
        return args.hasProperty (name) ? requireBool (args, name) : fallback;
    }

    /** REF: {"profileId","projectId","name"}, or null when nothing is linked. */
    juce::var refVar (const protocol::ProjectRef& ref)
    {
        if (! ref.isLinked())
            return {};
        return object ({ { "profileId", ref.profileId }, { "projectId", ref.id }, { "name", ref.name } });
    }

    juce::var refVar (const IndexedProject& p)
    {
        return object ({ { "profileId", p.profileId }, { "projectId", p.id }, { "name", p.name } });
    }

    juce::var nullIfEmpty (const juce::String& s)
    {
        return s.isEmpty() ? juce::var() : juce::var (s);
    }

    const char* verdictName (TakeVerdict v)
    {
        switch (v)
        {
            case TakeVerdict::keep:              return "keep";
            case TakeVerdict::discardEnded:      return "discardEnded";
            case TakeVerdict::discardIncomplete: return "discardIncomplete";
            case TakeVerdict::discardTooShort:   return "discardTooShort";
            case TakeVerdict::discardSilent:     return "discardSilent";
            case TakeVerdict::discardNotBetter:  return "discardNotBetter";
        }
        return "unknown";
    }

    const std::pair<const char*, TakeEnd> takeEnds[] {
        { "none", TakeEnd::none },           { "stopped", TakeEnd::stopped },
        { "reachedMax", TakeEnd::reachedMax }, { "silence", TakeEnd::silence },
        { "looped", TakeEnd::looped },       { "offline", TakeEnd::offline },
        { "manualStop", TakeEnd::manualStop }, { "modeChanged", TakeEnd::modeChanged },
    };

    const std::pair<const char*, CaptureMode> captureModes[] {
        { "off", CaptureMode::off }, { "automatic", CaptureMode::automatic }, { "manual", CaptureMode::manual },
    };

    template <typename Enum, size_t N>
    Enum parseName (const std::pair<const char*, Enum> (&names)[N], const juce::String& text, const char* what)
    {
        for (const auto& [name, value] : names)
            if (text == name)
                return value;

        juce::StringArray known;
        for (const auto& [name, value] : names)
            known.add (name);
        throw CommandError { juce::String ("unknown ") + what + " \"" + text + "\" (" + known.joinIntoString ("|") + ")" };
    }

    template <typename Enum, size_t N>
    const char* nameOf (const std::pair<const char*, Enum> (&names)[N], Enum value)
    {
        for (const auto& [name, v] : names)
            if (v == value)
                return name;
        return "unknown";
    }
}

BridgeDriver::BridgeDriver (BridgeFolder folder, juce::File takeFolder)
    : takes (std::move (takeFolder))
{
    // Real time until a "clock" command fixes it.
    bridgeSession = std::make_unique<BridgeSession> (std::move (folder), [this]
    {
        return fixedTime ? *fixedTime : juce::Time::getCurrentTime();
    });
}

juce::String BridgeDriver::readyLine() const
{
    return juce::JSON::toString (object ({ { "ok", true },
                                           { "ready", true },
                                           { "runId", bridgeSession->runId() },
                                           { "version", DPM_VERSION_STRING },
                                           { "bridgeDir", bridgeSession->folder().root.getFullPathName() } }),
                                 true);
}

juce::String BridgeDriver::handle (const juce::String& line)
{
    juce::var reply;

    try
    {
        juce::var command;
        if (juce::JSON::parse (line.trim(), command).failed() || ! command.isObject())
            throw CommandError { "not a JSON object" };
        if (! command["cmd"].isString())
            throw CommandError { "missing \"cmd\"" };

        const auto result = run (command["cmd"].toString(), command);

        // "ok" first, then whatever the command answered.
        reply = object ({ { "ok", true } });
        if (auto* fields = result.getDynamicObject())
            for (const auto& p : fields->getProperties())
                reply.getDynamicObject()->setProperty (p.name, p.value);
    }
    catch (const CommandError& e)
    {
        reply = object ({ { "ok", false }, { "error", e.message } });
    }

    return juce::JSON::toString (reply, true);
}

juce::var BridgeDriver::run (const juce::String& cmd, const juce::var& args)
{
    auto& s = *bridgeSession;

    if (cmd == "clock")
    {
        if (args.hasProperty ("now"))
        {
            const auto t = protocol::parseTime (requireString (args, "now"));
            if (! t)
                throw CommandError { "\"now\" is not an ISO 8601 time" };
            fixedTime = *t;
        }
        else if (const auto seconds = optionalNumber (args, "advanceSeconds"))
        {
            fixedTime = fixedTime.value_or (juce::Time::getCurrentTime()) + juce::RelativeTime::seconds (*seconds);
        }
        else
        {
            throw CommandError { "clock needs \"now\" or \"advanceSeconds\"" };
        }
        return object ({ { "now", protocol::formatTime (*fixedTime) } });
    }

    if (cmd == "host")
    {
        protocol::HostContext context;
        context.hostName = requireString (args, "name");
        context.windowTitle = args.hasProperty ("windowTitle") ? requireString (args, "windowTitle") : juce::String();
        s.setHostContext (context);
        return object();
    }

    if (cmd == "settings")
    {
        if (args.hasProperty ("stampTodosWithTime"))
            s.setStampTodosWithTime (requireBool (args, "stampTodosWithTime"));
        if (args.hasProperty ("captureMode"))
            s.setCaptureMode (parseName (captureModes, requireString (args, "captureMode"), "captureMode"));
        if (const auto seconds = optionalNumber (args, "captureMaxSeconds"))
            s.setCaptureMaxSeconds ((int) *seconds);

        const auto& st = s.state();
        return object ({ { "stampTodosWithTime", st.stampTodosWithTime },
                         { "captureMode", nameOf (captureModes, st.capture.mode) },
                         { "captureMaxSeconds", st.capture.maxSeconds } });
    }

    if (cmd == "refresh")
    {
        const auto changed = s.refresh();
        return object ({ { "changed", changed },
                         { "hasIndex", s.index() != nullptr },
                         { "indexError", nullIfEmpty (s.indexError()) } });
    }

    if (cmd == "autoLink")
    {
        const auto changed = s.autoLink();
        return object ({ { "changed", changed }, { "linked", refVar (s.state().project) } });
    }

    if (cmd == "linkTo")
    {
        const auto profileId = requireString (args, "profileId");
        const auto projectId = requireString (args, "projectId");
        const auto* project = s.index() != nullptr ? s.index()->find (profileId, projectId) : nullptr;
        if (project == nullptr)
            throw CommandError { "no project " + profileId + "/" + projectId + " in the index (refresh first?)" };

        s.linkTo (*project, optionalBool (args, "byUser", true));
        return object ({ { "linked", refVar (s.state().project) } });
    }

    if (cmd == "unlink")
    {
        s.unlink();
        return object ({ { "linked", juce::var() } });
    }

    if (cmd == "addTodo")
    {
        const auto added = s.addTodo (requireString (args, "text"), optionalNumber (args, "playheadSeconds"));
        return object ({ { "added", added } });
    }

    if (cmd == "setTodoCompleted")
    {
        s.setTodoCompleted (requireString (args, "todoId"), requireBool (args, "completed"));
        return object();
    }

    if (cmd == "setNotes")
        return object ({ { "changed", s.setNotes (requireString (args, "text")) } });

    if (cmd == "deliver")
    {
        const auto written = s.deliver();
        return object ({ { "written", written }, { "undelivered", s.undeliveredCount() } });
    }

    if (cmd == "heartbeat")
    {
        s.heartbeat();
        return object ({ { "heartbeatFile", s.heartbeatFile().getFullPathName() },
                         { "present", s.heartbeatFile().existsAsFile() } });
    }

    if (cmd == "goodbye")
    {
        s.sayGoodbye();
        return object();
    }

    if (cmd == "announceAgain")
    {
        s.announceAgain();
        return object();
    }

    if (cmd == "capture")
        return capture (args);

    if (cmd == "state")
        return stateReply();

    if (cmd == "saveState")
        return object ({ { "state", s.state().toValueTree().toXmlString() } });

    if (cmd == "restore")
    {
        const auto tree = juce::ValueTree::fromXml (requireString (args, "state"));
        if (! tree.isValid())
            throw CommandError { "\"state\" is not a saved state" };
        s.restore (PluginState::fromValueTree (tree));
        return object ({ { "linked", refVar (s.state().project) } });
    }

    if (cmd == "quit")
    {
        quit = true;
        return object();
    }

    throw CommandError { "unknown command \"" + cmd + "\"" };
}

juce::var BridgeDriver::capture (const juce::var& args)
{
    const auto seconds = optionalNumber (args, "seconds");
    if (! seconds || *seconds <= 0 || *seconds > 600)
        throw CommandError { "\"seconds\" must be a number of seconds, up to 600" };

    const auto sampleRate = optionalNumber (args, "sampleRate").value_or (48000.0);
    const auto channels = (int) optionalNumber (args, "channels").value_or (2.0);
    if (sampleRate < 8000 || sampleRate > 192000 || channels < 1 || channels > 2)
        throw CommandError { "\"sampleRate\" must be 8000-192000 and \"channels\" 1 or 2" };

    const auto silent = optionalBool (args, "silent", false);
    TakeSummary summary;
    summary.end = parseName (takeEnds, args.hasProperty ("end") ? requireString (args, "end") : "stopped", "end");
    summary.manual = optionalBool (args, "manual", false);
    summary.overflowed = optionalBool (args, "overflowed", false);

    // A take as the capture engine writes it: FLAC, outside the bridge
    // folder, under the name of a take still being recorded.
    takes.createDirectory();
    const auto file = takes.getChildFile (partialCaptureName (bridgeSession->runId(), ++takeNumber));

    CaptureWriter writer;
    if (! writer.open (file, sampleRate, channels))
        throw CommandError { "could not write " + file.getFullPathName() };

    constexpr float level = 0.5f;
    constexpr int block = 4096;
    juce::AudioBuffer<float> buffer (channels, block);
    const auto total = (juce::int64) std::llround (*seconds * sampleRate);
    for (juce::int64 done = 0; done < total; done += block)
    {
        const auto n = (int) std::min<juce::int64> (block, total - done);
        for (int i = 0; i < n; ++i)
        {
            const auto t = (double) (done + i) / sampleRate;
            const auto sample = silent ? 0.0f
                                       : level * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t);
            for (int c = 0; c < channels; ++c)
                buffer.setSample (c, i, sample);
        }
        writer.write (buffer.getArrayOfReadPointers(), n);
    }

    summary.seconds = writer.close();
    summary.peak = silent ? 0.0f : level;

    const auto outcome = bridgeSession->handleFinishedTake ({ file, summary, sampleRate, channels });
    const auto kept = outcome.verdict == TakeVerdict::keep && outcome.file.existsAsFile();
    return object ({ { "verdict", verdictName (outcome.verdict) },
                     { "seconds", summary.seconds },
                     { "file", kept ? juce::var (outcome.file.getFullPathName()) : juce::var() },
                     { "relativeFile", kept ? juce::var (bridgeSession->folder().relativePath (outcome.file)) : juce::var() } });
}

juce::var BridgeDriver::stateReply() const
{
    const auto& s = *bridgeSession;
    const auto& st = s.state();

    juce::Array<juce::var> todos;
    for (const auto& t : s.todos())
        todos.add (object ({ { "id", t.todoId }, { "text", t.text }, { "completed", t.completed }, { "pending", t.pending } }));

    juce::Array<juce::var> suggestions;
    for (const auto& c : s.suggestions())
        if (c.project != nullptr)
            suggestions.add (refVar (*c.project));

    juce::var update;
    if (const auto release = s.availableUpdate())
        update = object ({ { "version", release->version }, { "url", release->url } });

    juce::var lastCapture;
    if (st.lastCapture.exists())
        lastCapture = object ({ { "file", st.lastCapture.file },
                                { "seconds", st.lastCapture.seconds },
                                { "reachedMax", st.lastCapture.reachedMax },
                                { "capturedAt", protocol::formatTime (st.lastCapture.capturedAt) } });

    return object ({ { "runId", s.runId() },
                     { "instanceId", st.instanceId },
                     { "linked", refVar (st.project) },
                     { "linkedByUser", st.linkedByUser },
                     { "hostName", s.hostContext().hostName },
                     { "todos", todos },
                     { "notes", s.notes() },
                     { "appNotes", s.appNotes() },
                     { "notesPending", s.notesPending() },
                     { "undelivered", s.undeliveredCount() },
                     { "availableUpdate", update },
                     { "suggestions", suggestions },
                     { "lastCapture", lastCapture },
                     { "stampTodosWithTime", st.stampTodosWithTime },
                     { "captureMode", nameOf (captureModes, st.capture.mode) },
                     { "captureMaxSeconds", st.capture.maxSeconds } });
}

} // namespace dpm
