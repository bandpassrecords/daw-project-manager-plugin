#include "TestHelpers.h"

#include "core/CaptureNaming.h"
#include "core/BridgeSession.h"

using namespace dpm;

namespace
{
    /** A session against a temp bridge folder with a clock the test moves. */
    struct Fixture
    {
        test::TempDir tmp;
        BridgeFolder bridge = tmp.bridge();
        juce::Time clock = test::fixedTime();
        BridgeSession session { bridge, [this] { return clock; } };

        void writeIndex (const juce::String& json)
        {
            REQUIRE (bridge.root.createDirectory());
            REQUIRE (bridge.indexFile().replaceWithText (json));
            // A later write in the same second must still look changed.
            bridge.indexFile().setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (++writes));
        }

        void writeLaunch (const char* projectId, const char* profileId, juce::Time at)
        {
            REQUIRE (bridge.root.createDirectory());
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("projectId", projectId);
            obj->setProperty ("profileId", profileId);
            obj->setProperty ("launchedAt", protocol::formatTime (at));
            REQUIRE (bridge.launchFile().replaceWithText (juce::JSON::toString (juce::var (obj))));
        }

        /** A finished take as the engine hands it over: a real file in a take folder. */
        FinishedTakeFile take (double seconds, TakeEnd end = TakeEnd::stopped, bool manual = false)
        {
            const auto file = tmp.dir.getChildFile ("takes").getChildFile (partialCaptureName ("x", ++takes));
            file.getParentDirectory().createDirectory();
            file.replaceWithText ("pretend FLAC");
            return { file, { seconds, end, manual, 0.5f, false }, 48000.0, 2 };
        }

        int writes = 0, takes = 0;
    };

    juce::String library (std::vector<test::TodoSpec> songTodos = {}, juce::Time at = test::fixedTime())
    {
        return test::indexJson ({
            { "song", "solo", "Teardrop Remix", "C:\\Music\\Teardrop Remix.cpr", "Cubase", songTodos },
            { "other", "solo", "Night Drive", "C:\\Music\\Night Drive.als", "Ableton Live" },
        }, at);
    }
}

TEST_CASE ("Without an app index there is nothing to link to, and that is not an error", "[session]")
{
    Fixture f;
    CHECK_FALSE (f.session.refresh());
    CHECK (f.session.index() == nullptr);
    CHECK (f.session.indexError().isEmpty());
    CHECK (f.session.suggestions().empty());
    CHECK_FALSE (f.session.autoLink());
}

TEST_CASE ("The index is read when it appears and re-read only when it changes", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    CHECK (f.session.refresh());
    REQUIRE (f.session.index() != nullptr);
    CHECK (f.session.index()->projects.size() == 2);
    CHECK_FALSE (f.session.refresh());

    f.writeIndex (test::indexJson ({ { "song", "solo", "Teardrop Remix", "x.cpr" } }));
    CHECK (f.session.refresh());
    CHECK (f.session.index()->projects.size() == 1);

    f.bridge.indexFile().deleteFile();
    CHECK (f.session.refresh());
    CHECK (f.session.index() == nullptr);
}

TEST_CASE ("A broken index keeps the last good one and reports why", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.writeIndex ("{ broken");
    f.session.refresh();
    REQUIRE (f.session.index() != nullptr);
    CHECK (f.session.index()->projects.size() == 2);
    CHECK (f.session.indexError().isNotEmpty());
}

TEST_CASE ("The window title links the session to its project and says hello", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.setHostContext ({ "Steinberg Cubase 14", "Cubase Pro Project - Teardrop Remix" });

    CHECK (f.session.autoLink());
    CHECK (f.session.state().project.id == "song");
    CHECK_FALSE (f.session.state().linkedByUser);
    CHECK_FALSE (f.session.autoLink());     // already linked: no change

    CHECK (f.session.deliver() == 1);
    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 1);
    CHECK (messages[0]["type"].toString() == "session.hello");
    CHECK (messages[0]["project"]["id"].toString() == "song");
    CHECK (messages[0]["project"]["profileId"].toString() == "solo");
    CHECK (messages[0]["host"]["windowTitle"].toString() == "Cubase Pro Project - Teardrop Remix");
    CHECK (f.session.deliver() == 0);       // said once
}

TEST_CASE ("The app's launch hint links when the title can't be read", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.writeLaunch ("other", "solo", f.clock - juce::RelativeTime::minutes (2));
    f.session.refresh();
    f.session.setHostContext ({ "Bitwig Studio", "" });

    CHECK (f.session.autoLink());
    CHECK (f.session.state().project.id == "other");
}

TEST_CASE ("An automatic link follows the project; a manual one never moves", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();

    f.session.setHostContext ({ "Cubase", "Cubase Pro Project - Teardrop Remix" });
    f.session.autoLink();
    f.session.setHostContext ({ "Cubase", "Cubase Pro Project - Night Drive" });
    CHECK (f.session.autoLink());
    CHECK (f.session.state().project.id == "other");

    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);
    CHECK_FALSE (f.session.autoLink());
    CHECK (f.session.state().project.id == "song");

    REQUIRE (f.session.relinkSuggestion() != nullptr);
    CHECK (f.session.relinkSuggestion()->id == "other");
}

TEST_CASE ("A project renamed in the app keeps its link and takes the new name", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);

    f.writeIndex (test::indexJson ({ { "song", "solo", "Teardrop (Final)", "D:\\New\\Teardrop (Final).cpr" } }));
    f.session.refresh();
    CHECK (f.session.state().project.id == "song");
    CHECK (f.session.state().project.name == "Teardrop (Final)");
    CHECK (f.session.state().project.filePath == "D:\\New\\Teardrop (Final).cpr");
}

TEST_CASE ("To-dos: shown from the index, added and ticked in the plugin, delivered in order", "[session]")
{
    Fixture f;
    f.writeIndex (library ({ { "t1", "Fix the kick" }, { "t2", "Record vocals", true } }));
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), false);
    f.session.deliver();   // hello

    auto todos = f.session.todos();
    REQUIRE (todos.size() == 2);
    CHECK (todos[0].text == "Fix the kick");

    CHECK (f.session.addTodo ("Automate the filter", std::nullopt));
    f.session.setTodoCompleted ("t1", true);
    CHECK (f.session.undeliveredCount() == 2);

    todos = f.session.todos();
    REQUIRE (todos.size() == 3);
    CHECK (todos[0].completed);
    CHECK (todos[0].pending);
    CHECK (todos[2].text == "Automate the filter");
    CHECK (todos[2].pending);

    CHECK (f.session.deliver() == 2);
    CHECK (f.session.undeliveredCount() == 0);

    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 3);
    CHECK (messages[1]["type"].toString() == "todo.add");
    CHECK (messages[1]["payload"]["text"].toString() == "Automate the filter");
    const auto newId = messages[1]["payload"]["todoId"].toString();
    CHECK (messages[2]["type"].toString() == "todo.setCompleted");
    CHECK (messages[2]["payload"]["todoId"].toString() == "t1");
    CHECK ((bool) messages[2]["payload"]["completed"]);

    // The app applies them and rewrites the index: nothing is pending any more.
    f.clock = f.clock + juce::RelativeTime::seconds (5);
    f.writeIndex (library ({ { "t1", "Fix the kick", true },
                             { "t2", "Record vocals", true },
                             { newId.toRawUTF8(), "Automate the filter" } },
                           f.clock));
    f.session.refresh();
    CHECK (f.session.state().pendingTodos.empty());
    todos = f.session.todos();
    REQUIRE (todos.size() == 3);
    for (auto& t : todos)
        CHECK_FALSE (t.pending);
}

TEST_CASE ("The playhead stamp is added only when switched on", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), false);

    f.session.addTodo ("Plain", 83.0);
    f.session.setStampTodosWithTime (true);
    f.session.addTodo ("Stamped", 83.0);
    f.session.addTodo ("No playhead", std::nullopt);

    const auto todos = f.session.todos();
    REQUIRE (todos.size() == 3);
    CHECK (todos[0].text == "Plain");
    CHECK (todos[1].text == "[01:23] Stamped");
    CHECK (todos[2].text == "No playhead");
}

TEST_CASE ("To-dos can't be added without a link", "[session]")
{
    Fixture f;
    CHECK_FALSE (f.session.addTodo ("Nowhere to go", std::nullopt));
    CHECK (f.session.todos().empty());
}

TEST_CASE ("Without an app, to-dos wait in the saved state and are delivered later", "[session]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);

    // The bridge folder becomes unwritable (here: a file where the outbox goes).
    REQUIRE (f.bridge.outboxDir().replaceWithText ("blocked"));
    f.session.addTodo ("Survives", std::nullopt);
    CHECK (f.session.deliver() == 0);
    CHECK (f.session.undeliveredCount() == 1);

    // The DAW saves the project, closes, and opens it again later.
    const auto saved = f.session.state().toValueTree().createXml();
    BridgeSession reopened (f.bridge, [&f] { return f.clock; });
    reopened.restore (PluginState::fromValueTree (juce::ValueTree::fromXml (*saved)));
    reopened.refresh();
    REQUIRE (reopened.todos().size() == 1);
    CHECK (reopened.todos()[0].text == "Survives");

    REQUIRE (f.bridge.outboxDir().deleteFile());
    CHECK (reopened.deliver() == 2);    // hello again, then the to-do
    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 2);
    CHECK (messages[0]["type"].toString() == "session.hello");
    CHECK (messages[1]["payload"]["text"].toString() == "Survives");
}

TEST_CASE ("A kept take moves into the captures folder and is announced", "[session][capture]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);
    f.session.deliver();

    const auto take = f.take (60.0, TakeEnd::reachedMax);
    const auto outcome = f.session.handleFinishedTake (take);

    CHECK (outcome.verdict == TakeVerdict::keep);
    CHECK_FALSE (take.file.exists());
    REQUIRE (outcome.file.existsAsFile());
    CHECK (outcome.file.getParentDirectory() == f.bridge.capturesDir());
    CHECK (outcome.file.getFileName().startsWith ("Teardrop Remix_"));
    CHECK (f.session.state().lastCapture.file == f.bridge.relativePath (outcome.file));
    CHECK (f.session.state().lastCapture.reachedMax);

    const auto messages = test::outboxMessages (f.bridge);
    REQUIRE (messages.size() == 2);
    const auto& m = messages[1];
    CHECK (m["type"].toString() == "capture.ready");
    CHECK (m["project"]["id"].toString() == "song");
    CHECK (m["payload"]["file"].toString() == f.session.state().lastCapture.file);
    CHECK (m["payload"]["supersedes"].isVoid());
    CHECK ((double) m["payload"]["durationSeconds"] == 60.0);
    CHECK ((int) m["payload"]["channels"] == 2);
}

TEST_CASE ("A better take replaces the previous capture's file", "[session][capture]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);

    const auto first = f.session.handleFinishedTake (f.take (30.0)).file;
    REQUIRE (first.existsAsFile());
    const auto firstRelative = f.session.state().lastCapture.file;

    f.clock = f.clock + juce::RelativeTime::minutes (10);
    const auto second = f.session.handleFinishedTake (f.take (45.0));
    CHECK (second.verdict == TakeVerdict::keep);
    CHECK_FALSE (first.exists());
    CHECK (second.file.existsAsFile());

    const auto messages = test::outboxMessages (f.bridge);
    CHECK (messages.back()["payload"]["supersedes"].toString() == firstRelative);
}

TEST_CASE ("A worse take is deleted and the capture stays", "[session][capture]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);

    const auto kept = f.session.handleFinishedTake (f.take (50.0)).file;
    const auto shortTake = f.take (15.0);
    CHECK (f.session.handleFinishedTake (shortTake).verdict == TakeVerdict::discardNotBetter);
    CHECK_FALSE (shortTake.file.exists());
    CHECK (kept.existsAsFile());
    CHECK (f.session.state().lastCapture.seconds == 50.0);

    const auto looped = f.take (60.0, TakeEnd::looped);
    CHECK (f.session.handleFinishedTake (looped).verdict == TakeVerdict::discardEnded);
    CHECK_FALSE (looped.file.exists());
}

TEST_CASE ("Linking another project starts its captures afresh", "[session][capture]")
{
    Fixture f;
    f.writeIndex (library());
    f.session.refresh();
    f.session.linkTo (*f.session.index()->find ("solo", "song"), true);
    const auto songCapture = f.session.handleFinishedTake (f.take (60.0, TakeEnd::reachedMax)).file;

    f.session.linkTo (*f.session.index()->find ("solo", "other"), true);
    CHECK_FALSE (f.session.state().lastCapture.exists());

    // A short first take for the new project is kept; the old project's file stays.
    f.clock = f.clock + juce::RelativeTime::minutes (1);
    CHECK (f.session.handleFinishedTake (f.take (12.0)).verdict == TakeVerdict::keep);
    CHECK (songCapture.existsAsFile());
}

TEST_CASE ("Captures taken twice in the same second don't overwrite each other", "[session][capture]")
{
    Fixture f;
    const auto a = f.session.handleFinishedTake (f.take (20.0, TakeEnd::manualStop, true)).file;
    REQUIRE (a.existsAsFile());
    // Same clock, same (unlinked) name; the first is superseded and removed,
    // but the second must not have been written over it first.
    const auto b = f.session.handleFinishedTake (f.take (20.0, TakeEnd::manualStop, true)).file;
    CHECK (b.existsAsFile());
    CHECK (a != b);
}

TEST_CASE ("moveIntoPlace moves a file and leaves no partial copy", "[session]")
{
    test::TempDir tmp;
    const auto src = tmp.dir.getChildFile ("a/take.flac");
    src.getParentDirectory().createDirectory();
    src.replaceWithText ("data");
    const auto dst = tmp.dir.getChildFile ("b/c/Song.flac");

    CHECK (moveIntoPlace (src, dst));
    CHECK_FALSE (src.exists());
    CHECK (dst.loadFileAsString() == "data");
    CHECK (dst.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*.part").isEmpty());
}
