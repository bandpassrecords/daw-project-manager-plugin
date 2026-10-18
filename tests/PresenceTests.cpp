#include "TestHelpers.h"

#include "core/CompanionSession.h"

using namespace dpm;

namespace
{
    struct Fixture
    {
        test::TempDir tmp;
        BridgeFolder bridge = tmp.bridge();
        juce::Time clock = test::fixedTime();
        CompanionSession session { bridge, [this] { return clock; } };

        Fixture()
        {
            REQUIRE (bridge.root.createDirectory());
            REQUIRE (bridge.indexFile().replaceWithText (test::indexJson ({
                { "song", "solo", "Teardrop", "C:\\Music\\Teardrop.cpr" },
                { "other", "solo", "Night Drive", "C:\\Music\\Night Drive.als" },
            })));
            session.refresh();
        }

        void link (const char* id) { session.linkTo (*session.index()->find ("solo", id), true); }

        juce::var heartbeat() const { return juce::JSON::parse (session.heartbeatFile().loadFileAsString()); }

        std::vector<juce::String> types() const
        {
            std::vector<juce::String> out;
            for (auto& m : test::outboxMessages (bridge))
                out.push_back (m["type"].toString() + ":" + m["project"]["id"].toString());
            return out;
        }
    };
}

TEST_CASE ("Saying hello starts the heartbeat, naming the project and this run", "[presence]")
{
    Fixture f;
    f.link ("song");
    CHECK_FALSE (f.session.heartbeatFile().exists());   // nothing until the app has been told

    f.session.deliver();
    REQUIRE (f.session.heartbeatFile().existsAsFile());
    CHECK (f.session.heartbeatFile().getParentDirectory() == f.bridge.instancesDir());

    const auto beat = f.heartbeat();
    CHECK (beat["runId"].toString() == f.session.runId());
    CHECK (beat["instanceId"].toString() == f.session.state().instanceId);
    CHECK (beat["project"]["id"].toString() == "song");
    CHECK (beat["project"]["profileId"].toString() == "solo");
    CHECK (beat["lastSeen"].toString() == "2026-10-18T12:00:00.000Z");

    const auto hello = test::outboxMessages (f.bridge).front();
    CHECK (hello["plugin"]["runId"].toString() == f.session.runId());
}

TEST_CASE ("The heartbeat moves on with the clock", "[presence]")
{
    Fixture f;
    f.link ("song");
    f.session.deliver();

    f.clock = f.clock + juce::RelativeTime::seconds (protocol::heartbeatSeconds);
    f.session.heartbeat();
    CHECK (f.heartbeat()["lastSeen"].toString() == "2026-10-18T12:00:10.000Z");
    CHECK (f.heartbeat()["startedAt"].toString() == "2026-10-18T12:00:00.000Z");
}

TEST_CASE ("Being destroyed says goodbye to the project and stops the heartbeat", "[presence]")
{
    Fixture f;
    f.link ("song");
    f.session.deliver();
    f.clock = f.clock + juce::RelativeTime::minutes (42);

    f.session.sayGoodbye();
    CHECK_FALSE (f.session.heartbeatFile().exists());

    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 2);
    CHECK (messages[1]["type"].toString() == "session.goodbye");
    CHECK (messages[1]["project"]["id"].toString() == "song");
    CHECK (messages[1]["createdAt"].toString() == "2026-10-18T12:42:00.000Z");
    CHECK (messages[1]["plugin"]["runId"].toString() == f.session.runId());

    f.session.sayGoodbye();     // once is enough
    CHECK (test::outboxMessages (f.bridge).size() == 2);
}

TEST_CASE ("An instance that never said hello leaves without a word", "[presence]")
{
    // A host's plug-in scan: created, auto-linked from the window title,
    // destroyed before the first timer tick delivered anything.
    Fixture f;
    f.link ("song");
    f.session.sayGoodbye();
    CHECK (test::outboxMessages (f.bridge).empty());
    CHECK_FALSE (f.session.heartbeatFile().exists());
}

TEST_CASE ("Linking another project says goodbye to the old one first", "[presence]")
{
    Fixture f;
    f.link ("song");
    f.session.deliver();
    f.link ("other");
    CHECK_FALSE (f.session.heartbeatFile().exists());
    f.session.deliver();

    CHECK (f.types() == std::vector<juce::String> { "session.hello:song", "session.goodbye:song", "session.hello:other" });
    CHECK (f.heartbeat()["project"]["id"].toString() == "other");
}

TEST_CASE ("Unlinking says goodbye", "[presence]")
{
    Fixture f;
    f.link ("song");
    f.session.deliver();
    f.session.unlink();
    CHECK (f.types() == std::vector<juce::String> { "session.hello:song", "session.goodbye:song" });
    CHECK_FALSE (f.session.heartbeatFile().exists());
}

TEST_CASE ("A state for another project, loaded after hello, says goodbye to the first", "[presence]")
{
    Fixture f;
    f.link ("song");
    f.session.deliver();

    auto state = CompanionState::fresh();
    state.project = { "other", "solo", "Night Drive", "" };
    f.session.restore (state);
    f.session.deliver();
    CHECK (f.types() == std::vector<juce::String> { "session.hello:song", "session.goodbye:song", "session.hello:other" });
}

TEST_CASE ("After a goodbye, announcing again says hello and restarts the heartbeat", "[presence]")
{
    // The Standalone's "Close project" then "Reopen project".
    Fixture f;
    f.link ("song");
    f.session.deliver();
    f.session.sayGoodbye();
    CHECK (f.session.deliver() == 0);     // closed stays closed

    f.session.announceAgain();
    CHECK (f.session.deliver() == 1);
    CHECK (f.types() == std::vector<juce::String> { "session.hello:song", "session.goodbye:song", "session.hello:song" });
    CHECK (f.session.heartbeatFile().existsAsFile());
}

TEST_CASE ("Announcing again without a link says nothing", "[presence]")
{
    Fixture f;
    f.session.announceAgain();
    CHECK (f.session.deliver() == 0);
}

TEST_CASE ("Two running copies of one project are two instances", "[presence]")
{
    // A project saved under a new name keeps the saved instance id; opened
    // side by side, each copy must still come and go on its own.
    Fixture f;
    const auto saved = f.session.state();
    CompanionSession copy (f.bridge, [&f] { return f.clock; });
    copy.restore (saved);

    CHECK (copy.state().instanceId == f.session.state().instanceId);
    CHECK (copy.runId() != f.session.runId());
    CHECK (copy.heartbeatFile() != f.session.heartbeatFile());
}
