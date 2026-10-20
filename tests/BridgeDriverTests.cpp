#include "TestHelpers.h"

#include "core/BridgeDriver.h"

using namespace dpm;

namespace
{
    struct Fixture
    {
        test::TempDir tmp;
        BridgeFolder bridge = tmp.bridge();
        BridgeDriver driver { bridge, tmp.dir.getChildFile ("takes") };

        /** Sends one command and returns the parsed reply. */
        juce::var send (const juce::String& line)
        {
            const auto reply = driver.handle (line);
            INFO (reply.toStdString());
            CHECK_FALSE (reply.containsAnyOf ("\r\n"));
            return juce::JSON::parse (reply);
        }

        juce::var ok (const juce::String& line)
        {
            const auto reply = send (line);
            INFO (juce::JSON::toString (reply, true).toStdString());
            REQUIRE ((bool) reply["ok"]);
            return reply;
        }

        void writeIndex (const juce::String& json)
        {
            REQUIRE (bridge.root.createDirectory());
            REQUIRE (bridge.indexFile().replaceWithText (json));
        }
    };

    juce::String library()
    {
        return test::indexJson ({ { "song", "solo", "Teardrop Remix", "C:\\Music\\Teardrop Remix.cpr", "Cubase",
                                    { { "t1", "Tighten the kick" } } },
                                  { "other", "solo", "Copper Sun", "C:\\Music\\Copper Sun.als", "Ableton Live" } });
    }
}

TEST_CASE ("The driver says it is ready, with its run id and version", "[driver]")
{
    Fixture f;
    const auto ready = juce::JSON::parse (f.driver.readyLine());
    CHECK ((bool) ready["ok"]);
    CHECK ((bool) ready["ready"]);
    CHECK (ready["runId"].toString() == f.driver.session().runId());
    CHECK (ready["version"].toString() == DPM_VERSION_STRING);
}

TEST_CASE ("Every command gets one reply line; a bad one says why", "[driver]")
{
    Fixture f;
    CHECK (f.send ("not json")["error"].toString() == "not a JSON object");
    CHECK (f.send (R"({"text":"no cmd"})")["error"].toString() == "missing \"cmd\"");
    CHECK (f.send (R"({"cmd":"fly"})")["error"].toString() == "unknown command \"fly\"");
    CHECK_FALSE ((bool) f.send (R"({"cmd":"addTodo"})")["ok"]);
    CHECK_FALSE ((bool) f.send (R"({"cmd":"clock","now":"yesterday"})")["ok"]);
    CHECK_FALSE ((bool) f.send (R"({"cmd":"capture","seconds":12,"end":"sideways"})")["ok"]);
}

TEST_CASE ("The driver links by the window title and delivers to the outbox", "[driver]")
{
    Fixture f;
    f.writeIndex (library());
    f.ok (R"({"cmd":"clock","now":"2026-10-21T10:00:00Z"})");

    const auto refresh = f.ok (R"({"cmd":"refresh"})");
    CHECK ((bool) refresh["changed"]);
    CHECK ((bool) refresh["hasIndex"]);
    CHECK (refresh["indexError"].isVoid());

    f.ok (R"({"cmd":"host","name":"Cubase","windowTitle":"Cubase Pro Project - Teardrop Remix"})");
    const auto linked = f.ok (R"({"cmd":"autoLink"})")["linked"];
    CHECK (linked["profileId"].toString() == "solo");
    CHECK (linked["projectId"].toString() == "song");
    CHECK (linked["name"].toString() == "Teardrop Remix");

    f.ok (R"({"cmd":"settings","stampTodosWithTime":true})");
    CHECK ((bool) f.ok (R"({"cmd":"addTodo","text":"Snare too loud","playheadSeconds":83.9})")["added"]);

    const auto state = f.ok (R"({"cmd":"state"})");
    REQUIRE (state["todos"].size() == 2);
    CHECK (state["todos"][1]["text"].toString() == "[01:23] Snare too loud");
    CHECK ((bool) state["todos"][1]["pending"]);
    CHECK ((int) state["undelivered"] == 1);    // the to-do; the hello is not an op

    const auto delivered = f.ok (R"({"cmd":"deliver"})");
    CHECK ((int) delivered["written"] == 2);
    CHECK ((int) delivered["undelivered"] == 0);

    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 2);
    CHECK (messages[0]["type"].toString() == "session.hello");
    CHECK (messages[1]["type"].toString() == "todo.add");
    CHECK (messages[1]["createdAt"].toString() == "2026-10-21T10:00:00.000Z");
}

TEST_CASE ("The driver's clock is fixed once set, and moves only when told", "[driver]")
{
    Fixture f;
    CHECK (f.ok (R"({"cmd":"clock","now":"2026-10-21T10:00:00Z"})")["now"].toString() == "2026-10-21T10:00:00.000Z");
    CHECK (f.ok (R"({"cmd":"clock","advanceSeconds":95})")["now"].toString() == "2026-10-21T10:01:35.000Z");
}

TEST_CASE ("Linking by hand, unlinking, and a project the index doesn't have", "[driver]")
{
    Fixture f;
    f.writeIndex (library());
    f.ok (R"({"cmd":"refresh"})");

    CHECK_FALSE ((bool) f.send (R"({"cmd":"linkTo","profileId":"solo","projectId":"nope"})")["ok"]);

    const auto linked = f.ok (R"({"cmd":"linkTo","profileId":"solo","projectId":"other"})")["linked"];
    CHECK (linked["name"].toString() == "Copper Sun");
    CHECK ((bool) f.ok (R"({"cmd":"state"})")["linkedByUser"]);

    CHECK (f.ok (R"({"cmd":"unlink"})")["linked"].isVoid());
    CHECK (f.ok (R"({"cmd":"state"})")["linked"].isVoid());
}

TEST_CASE ("A capture is written as the engine writes it and judged by the session", "[driver][capture]")
{
    Fixture f;
    f.writeIndex (library());
    f.ok (R"({"cmd":"refresh"})");
    f.ok (R"({"cmd":"linkTo","profileId":"solo","projectId":"song"})");

    const auto kept = f.ok (R"({"cmd":"capture","seconds":12,"sampleRate":8000,"channels":1})");
    CHECK (kept["verdict"].toString() == "keep");
    CHECK ((double) kept["seconds"] == Catch::Approx (12.0).margin (0.01));
    const juce::File file (kept["file"].toString());
    CHECK (file.existsAsFile());
    CHECK (file.getParentDirectory() == f.bridge.capturesDir());
    CHECK (kept["relativeFile"].toString().startsWith ("captures/"));
    CHECK (f.ok (R"({"cmd":"state"})")["lastCapture"]["file"].toString() == kept["relativeFile"].toString());

    const auto shortTake = f.ok (R"({"cmd":"capture","seconds":2,"sampleRate":8000})");
    CHECK (shortTake["verdict"].toString() == "discardTooShort");
    CHECK (shortTake["file"].isVoid());

    CHECK (f.ok (R"({"cmd":"capture","seconds":12,"sampleRate":8000,"silent":true})")["verdict"].toString() == "discardSilent");
    CHECK (f.ok (R"({"cmd":"capture","seconds":12,"sampleRate":8000,"end":"looped"})")["verdict"].toString() == "discardEnded");
    CHECK (f.ok (R"({"cmd":"capture","seconds":4,"sampleRate":8000,"manual":true,"end":"manualStop"})")["verdict"].toString() == "keep");
}

TEST_CASE ("Notes, presence and a saved state survive a round trip", "[driver]")
{
    Fixture f;
    f.writeIndex (library());
    f.ok (R"({"cmd":"refresh"})");
    f.ok (R"({"cmd":"linkTo","profileId":"solo","projectId":"song"})");

    CHECK ((bool) f.ok (R"({"cmd":"setNotes","text":"Vocal take 3"})")["changed"]);
    CHECK ((bool) f.ok (R"({"cmd":"state"})")["notesPending"]);

    f.ok (R"({"cmd":"deliver"})");
    CHECK ((bool) f.ok (R"({"cmd":"heartbeat"})")["present"]);

    const auto saved = f.ok (R"({"cmd":"saveState"})")["state"].toString();
    f.ok (R"({"cmd":"goodbye"})");
    CHECK_FALSE ((bool) f.ok (R"({"cmd":"heartbeat"})")["present"]);

    auto* restore = new juce::DynamicObject();
    restore->setProperty ("cmd", "restore");
    restore->setProperty ("state", saved);
    const auto restored = f.ok (juce::JSON::toString (juce::var (restore), true));
    CHECK (restored["linked"]["projectId"].toString() == "song");
    CHECK (f.ok (R"({"cmd":"state"})")["notes"].toString() == "Vocal take 3");

    f.ok (R"({"cmd":"quit"})");
    CHECK (f.driver.wantsToQuit());
}
