#include "TestHelpers.h"

using namespace dpm;

TEST_CASE ("Times are written in UTC with a Z, millisecond precision", "[protocol]")
{
    CHECK (protocol::formatTime (test::fixedTime()) == "2026-10-18T12:00:00.000Z");
    CHECK (protocol::formatTime (juce::Time (0)) == "1970-01-01T00:00:00.000Z");
    CHECK (protocol::formatTime (juce::Time ((juce::int64) 951782400123)) == "2000-02-29T00:00:00.123Z");
}

TEST_CASE ("Times written by the plugin read back identically", "[protocol]")
{
    for (auto ms : { (juce::int64) 0, (juce::int64) 1792324800000, (juce::int64) 1792324801999, (juce::int64) 951782400123 })
    {
        const juce::Time t (ms);
        auto parsed = protocol::parseTime (protocol::formatTime (t));
        REQUIRE (parsed.has_value());
        CHECK (parsed->toMilliseconds() == ms);
    }
}

TEST_CASE ("Dart's toIso8601String forms are read", "[protocol]")
{
    // UTC with microseconds: what DateTime.now().toUtc().toIso8601String() writes.
    auto micro = protocol::parseTime ("2026-10-18T12:00:00.123456Z");
    REQUIRE (micro.has_value());
    CHECK (micro->toMilliseconds() == test::fixedTime().toMilliseconds() + 123);

    // An explicit offset.
    auto offset = protocol::parseTime ("2026-10-18T14:00:00+02:00");
    REQUIRE (offset.has_value());
    CHECK (*offset == test::fixedTime());

    auto compactOffset = protocol::parseTime ("2026-10-18T07:30:00-0430");
    REQUIRE (compactOffset.has_value());
    CHECK (*compactOffset == test::fixedTime());

    // No offset: local time, as Dart writes a local DateTime.
    auto local = protocol::parseTime ("2026-10-18T12:00:00.000");
    REQUIRE (local.has_value());
    CHECK (*local == juce::Time (2026, 9, 18, 12, 0, 0, 0, true));

    // Date only.
    auto date = protocol::parseTime ("2026-10-18");
    REQUIRE (date.has_value());
    CHECK (*date == juce::Time (2026, 9, 18, 0, 0, 0, 0, true));
}

TEST_CASE ("Malformed times are rejected without asserting", "[protocol]")
{
    for (auto* bad : { "", "yesterday", "2026-13-01T00:00:00Z", "2026-02-30", "2026-10-18T25:00:00Z",
                       "2026-10-18T12:00:00Zjunk", "2026-10-18T12:00:00.Z", "2026-10-18T12:00:00+2" })
    {
        INFO (bad);
        CHECK_FALSE (protocol::parseTime (bad).has_value());
    }
}

TEST_CASE ("A message carries schema, type, plugin, host, project and payload", "[protocol]")
{
    protocol::Envelope e;
    e.messageId = "m-1";
    e.type = protocol::type::todoAdd;
    e.createdAt = test::fixedTime();
    e.instanceId = "inst-1";
    e.project = { "p1", "prof1", "My Song", "C:\\Music\\My Song.cpr" };
    e.host = { "Steinberg Cubase 14", "Cubase Pro Project - My Song" };

    const auto json = juce::JSON::parse (protocol::toJson (e, protocol::todoAddPayload ("t1", "Fix the kick", test::fixedTime())));

    CHECK ((int) json["schema"] == 1);
    CHECK (json["id"].toString() == "m-1");
    CHECK (json["type"].toString() == "todo.add");
    CHECK (json["createdAt"].toString() == "2026-10-18T12:00:00.000Z");
    CHECK (json["plugin"]["instanceId"].toString() == "inst-1");
    CHECK (json["plugin"]["version"].toString() == DPM_VERSION_STRING);
    CHECK (json["host"]["name"].toString() == "Steinberg Cubase 14");
    CHECK (json["host"]["windowTitle"].toString() == "Cubase Pro Project - My Song");
    CHECK (json["project"]["id"].toString() == "p1");
    CHECK (json["project"]["profileId"].toString() == "prof1");
    CHECK (json["project"]["filePath"].toString() == "C:\\Music\\My Song.cpr");
    CHECK (json["payload"]["todoId"].toString() == "t1");
    CHECK (json["payload"]["text"].toString() == "Fix the kick");
}

TEST_CASE ("An unlinked message has a null project, so the app can match it by its host context", "[protocol]")
{
    protocol::Envelope e;
    e.messageId = "m-2";
    e.type = protocol::type::captureReady;
    e.createdAt = test::fixedTime();
    e.host = { "REAPER", "Song [C:\\Song.rpp] - REAPER v7" };

    const auto json = juce::JSON::parse (protocol::toJson (e, {}));
    CHECK (json.hasProperty ("project"));
    CHECK (json["project"].isVoid());
    CHECK (json["host"]["windowTitle"].toString().contains ("Song.rpp"));
}

TEST_CASE ("A capture payload names the file relative to the bridge and what it supersedes", "[protocol]")
{
    protocol::CaptureInfo info;
    info.file = "captures/Song_20261018-140000.flac";
    info.supersedes = "captures/Song_20261017-090000.flac";
    info.durationSeconds = 60.0;
    info.sampleRate = 48000;
    info.channels = 2;
    info.peakDbfs = -3.5f;
    info.capturedAt = test::fixedTime();

    const auto p = protocol::captureReadyPayload (info);
    CHECK (p["file"].toString() == info.file);
    CHECK (p["supersedes"].toString() == info.supersedes);
    CHECK ((double) p["durationSeconds"] == 60.0);
    CHECK ((int) p["channels"] == 2);
    CHECK (p["trigger"].toString() == "auto");

    info.supersedes = {};
    info.manual = true;
    const auto first = protocol::captureReadyPayload (info);
    CHECK (first["supersedes"].isVoid());
    CHECK (first["trigger"].toString() == "manual");
}

TEST_CASE ("Projects are the same only when profile and id both match", "[protocol]")
{
    const protocol::ProjectRef a { "p1", "prof1", "Song", "" };
    CHECK (a.sameProject ({ "p1", "prof1", "Renamed", "elsewhere" }));
    CHECK_FALSE (a.sameProject ({ "p1", "prof2", "Song", "" }));
    CHECK_FALSE (a.sameProject ({ "p2", "prof1", "Song", "" }));
    CHECK_FALSE (protocol::ProjectRef {}.sameProject ({}));
}
