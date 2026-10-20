#include "TestHelpers.h"

#include "core/PluginState.h"

using namespace dpm;

TEST_CASE ("The saved state survives a round trip through XML, as the DAW stores it", "[state]")
{
    auto s = PluginState::fresh();
    s.project = { "p1", "prof1", "My Song", "C:\\Music\\My Song.cpr" };
    s.linkedByUser = true;
    s.capture.mode = CaptureMode::manual;
    s.capture.maxSeconds = 120;
    s.stampTodosWithTime = true;
    s.lastCapture = { "captures/My Song_1.flac", 59.5, false, test::fixedTime() };

    PendingTodoOp add;
    add.opId = "op1";
    add.kind = PendingTodoOp::Kind::add;
    add.project = s.project;
    add.todoId = "t1";
    add.text = "Fix \"the\" <kick> & snare";
    add.at = test::fixedTime (5);
    add.delivered = true;

    PendingTodoOp tick = add;
    tick.opId = "op2";
    tick.kind = PendingTodoOp::Kind::setCompleted;
    tick.text = {};
    tick.completed = true;
    tick.delivered = false;

    s.pendingTodos = { add, tick };

    const auto xml = s.toValueTree().createXml();
    REQUIRE (xml != nullptr);
    const auto back = PluginState::fromValueTree (juce::ValueTree::fromXml (*xml));

    CHECK (back.instanceId == s.instanceId);
    CHECK (back.project.sameProject (s.project));
    CHECK (back.project.filePath == s.project.filePath);
    CHECK (back.linkedByUser);
    CHECK (back.capture.mode == CaptureMode::manual);
    CHECK (back.capture.maxSeconds == 120);
    CHECK (back.stampTodosWithTime);
    CHECK (back.lastCapture.file == "captures/My Song_1.flac");
    CHECK (back.lastCapture.seconds == 59.5);
    CHECK (back.lastCapture.capturedAt == test::fixedTime());

    REQUIRE (back.pendingTodos.size() == 2);
    CHECK (back.pendingTodos[0].text == add.text);
    CHECK (back.pendingTodos[0].delivered);
    CHECK (back.pendingTodos[0].at == add.at);
    CHECK (back.pendingTodos[1].kind == PendingTodoOp::Kind::setCompleted);
    CHECK (back.pendingTodos[1].completed);
    CHECK_FALSE (back.pendingTodos[1].delivered);
    CHECK (back.pendingTodos[1].project.sameProject (s.project));
}

TEST_CASE ("A state from nowhere, or another plugin, falls back to defaults with a new instance id", "[state]")
{
    const auto a = PluginState::fromValueTree (juce::ValueTree ("SomethingElse"));
    const auto b = PluginState::fromValueTree ({});
    CHECK (a.instanceId.isNotEmpty());
    CHECK (a.instanceId != b.instanceId);
    CHECK_FALSE (a.project.isLinked());
    CHECK (a.capture.mode == CaptureMode::automatic);
    CHECK (a.capture.maxSeconds == 60);
    CHECK (a.pendingTodos.empty());
}

TEST_CASE ("Values the plugin doesn't offer are not restored", "[state]")
{
    auto tree = PluginState::fresh().toValueTree();
    tree.getChildWithName ("Capture").setProperty ("maxSeconds", 7, nullptr);
    tree.getChildWithName ("Capture").setProperty ("mode", "sideways", nullptr);
    tree.setProperty ("linkedByUser", true, nullptr);   // with no project linked

    const auto s = PluginState::fromValueTree (tree);
    CHECK (s.capture.maxSeconds == 60);
    CHECK (s.capture.mode == CaptureMode::automatic);
    CHECK_FALSE (s.linkedByUser);
}

TEST_CASE ("Pending ops missing their ids or project are dropped on load", "[state]")
{
    auto s = PluginState::fresh();
    PendingTodoOp op;
    op.opId = "op1";
    op.todoId = "t1";
    op.text = "no project";
    s.pendingTodos = { op };

    CHECK (PluginState::fromValueTree (s.toValueTree()).pendingTodos.empty());
}
