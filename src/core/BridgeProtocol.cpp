#include "BridgeProtocol.h"

namespace dpm::protocol
{

namespace
{
    juce::var object (std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* obj = new juce::DynamicObject();
        for (auto& [key, value] : props)
            obj->setProperty (key, value);
        return juce::var (obj);
    }

    juce::var nullable (const juce::String& s)
    {
        return s.isEmpty() ? juce::var() : juce::var (s);
    }
}

juce::var ProjectRef::toVar() const
{
    return object ({ { "id", id },
                     { "profileId", profileId },
                     { "name", name },
                     { "filePath", filePath } });
}

ProjectRef ProjectRef::fromVar (const juce::var& v)
{
    return { v["id"].toString(), v["profileId"].toString(), v["name"].toString(), v["filePath"].toString() };
}

juce::var HostContext::toVar() const
{
    return object ({ { "name", hostName }, { "windowTitle", windowTitle } });
}

juce::String toJson (const Envelope& e, const juce::var& payload)
{
    const auto message = object ({
        { "schema", schemaVersion },
        { "id", e.messageId },
        { "type", e.type },
        { "createdAt", formatTime (e.createdAt) },
        { "plugin", object ({ { "version", DPM_VERSION_STRING }, { "instanceId", e.instanceId }, { "runId", e.runId } }) },
        { "host", e.host.toVar() },
        { "project", e.project.isLinked() ? e.project.toVar() : juce::var() },
        { "payload", payload },
    });

    return juce::JSON::toString (message, juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::multiLine));
}

juce::var todoAddPayload (const juce::String& todoId, const juce::String& text, juce::Time createdAt)
{
    return object ({ { "todoId", todoId }, { "text", text }, { "createdAt", formatTime (createdAt) } });
}

juce::var todoSetCompletedPayload (const juce::String& todoId, bool completed, juce::Time changedAt)
{
    return object ({ { "todoId", todoId }, { "completed", completed }, { "changedAt", formatTime (changedAt) } });
}

juce::var notesSetPayload (const juce::String& text, const juce::String& baseText, juce::Time changedAt)
{
    return object ({ { "text", text }, { "baseText", baseText }, { "changedAt", formatTime (changedAt) } });
}

juce::var captureReadyPayload (const CaptureInfo& info)
{
    return object ({ { "file", info.file },
                     { "supersedes", nullable (info.supersedes) },
                     { "durationSeconds", info.durationSeconds },
                     { "sampleRate", info.sampleRate },
                     { "channels", info.channels },
                     { "peakDbfs", (double) info.peakDbfs },
                     { "trigger", info.manual ? "manual" : "auto" },
                     { "capturedAt", formatTime (info.capturedAt) } });
}

namespace
{
    juce::int64 floorDiv (juce::int64 a, juce::int64 b)
    {
        return a / b - ((a % b != 0) && ((a < 0) != (b < 0)));
    }

    // Inverse of daysFromCivil (Howard Hinnant's algorithm).
    void civilFromDays (juce::int64 z, int& y, int& m, int& d)
    {
        z += 719468;
        const auto era = (z >= 0 ? z : z - 146096) / 146097;
        const auto doe = (unsigned) (z - era * 146097);
        const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const auto mp = (5 * doy + 2) / 153;
        d = (int) (doy - (153 * mp + 2) / 5 + 1);
        m = (int) (mp < 10 ? mp + 3 : mp - 9);
        y = (int) ((juce::int64) yoe + era * 400 + (m <= 2));
    }
}

juce::String heartbeatJson (const Envelope& e, juce::Time lastSeen, juce::Time startedAt)
{
    const auto beat = object ({
        { "schema", schemaVersion },
        { "runId", e.runId },
        { "instanceId", e.instanceId },
        { "pluginVersion", DPM_VERSION_STRING },
        { "startedAt", formatTime (startedAt) },
        { "lastSeen", formatTime (lastSeen) },
        { "host", e.host.toVar() },
        { "project", e.project.isLinked() ? e.project.toVar() : juce::var() },
    });
    return juce::JSON::toString (beat, juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::multiLine));
}

juce::String formatTime (juce::Time t)
{
    // Plain arithmetic on the UTC millisecond count: Time's own getters are
    // local, and shifting by an offset goes wrong across a DST change.
    const auto ms = t.toMilliseconds();
    const auto days = floorDiv (ms, 86400000);
    auto msOfDay = (int) (ms - days * 86400000);

    int y, m, d;
    civilFromDays (days, y, m, d);

    const auto hours = msOfDay / 3600000;   msOfDay %= 3600000;
    const auto minutes = msOfDay / 60000;   msOfDay %= 60000;
    const auto seconds = msOfDay / 1000;
    const auto millis = msOfDay % 1000;

    return juce::String::formatted ("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", y, m, d, hours, minutes, seconds, millis);
}

namespace
{
    struct Cursor
    {
        juce::String::CharPointerType p;

        bool digits (int count, int& out)
        {
            out = 0;
            for (int i = 0; i < count; ++i)
            {
                const auto c = *p;
                if (c < '0' || c > '9')
                    return false;
                out = out * 10 + (int) (c - '0');
                ++p;
            }
            return true;
        }

        bool skip (juce::juce_wchar c)
        {
            if (*p != c)
                return false;
            ++p;
            return true;
        }
    };

    bool isLeap (int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

    int daysInMonth (int y, int m)
    {
        static constexpr int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        return m == 2 && isLeap (y) ? 29 : days[m - 1];
    }

    // Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
    juce::int64 daysFromCivil (int y, int m, int d)
    {
        y -= m <= 2;
        const auto era = (y >= 0 ? y : y - 399) / 400;
        const auto yoe = (unsigned) (y - era * 400);
        const auto doy = (unsigned) ((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
        const auto doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return (juce::int64) era * 146097 + (juce::int64) doe - 719468;
    }
}

std::optional<juce::Time> parseTime (const juce::String& text)
{
    Cursor c { text.trim().getCharPointer() };
    int year, month, day, hour = 0, minute = 0, second = 0, millis = 0;

    if (! (c.digits (4, year) && c.skip ('-') && c.digits (2, month) && c.skip ('-') && c.digits (2, day)))
        return std::nullopt;
    if (month < 1 || month > 12 || day < 1 || day > daysInMonth (year, month))
        return std::nullopt;

    if (c.skip ('T') || c.skip (' '))
    {
        if (! (c.digits (2, hour) && c.skip (':') && c.digits (2, minute)))
            return std::nullopt;
        if (c.skip (':') && ! c.digits (2, second))
            return std::nullopt;
        if (hour > 23 || minute > 59 || second > 59)
            return std::nullopt;

        if (c.skip ('.') || c.skip (','))
        {
            // Any number of fraction digits; keep milliseconds.
            int place = 100, seen = 0;
            while (*c.p >= '0' && *c.p <= '9')
            {
                millis += (int) (*c.p - '0') * place;
                place /= 10;
                ++c.p;
                ++seen;
            }
            if (seen == 0)
                return std::nullopt;
        }
    }

    const auto wallClockMs = ((daysFromCivil (year, month, day) * 24 + hour) * 60 + minute) * (juce::int64) 60000
                             + (juce::int64) second * 1000 + millis;

    if (c.skip ('Z') || c.skip ('z'))
        return *c.p == 0 ? std::optional (juce::Time (wallClockMs)) : std::nullopt;

    const auto sign = *c.p;
    if (sign == '+' || sign == '-')
    {
        ++c.p;
        int oh, om = 0;
        if (! c.digits (2, oh))
            return std::nullopt;
        c.skip (':');
        if (*c.p != 0 && ! c.digits (2, om))
            return std::nullopt;
        if (*c.p != 0)
            return std::nullopt;

        const auto offsetMs = ((juce::int64) oh * 60 + om) * 60000;
        return juce::Time (sign == '+' ? wallClockMs - offsetMs : wallClockMs + offsetMs);
    }

    if (*c.p != 0)
        return std::nullopt;

    // No offset: local wall-clock time, as ISO 8601 (and Dart) mean it.
    return juce::Time (year, month - 1, day, hour, minute, second, millis, true);
}

juce::String newId()
{
    return juce::Uuid().toDashedString();
}

} // namespace dpm::protocol
