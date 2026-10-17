#include "TestHelpers.h"

#include "core/TodoBook.h"

using namespace dpm;

namespace
{
    const protocol::ProjectRef song { "p1", "a", "Song", "C:\\Song.cpr" };
    const protocol::ProjectRef otherSong { "p2", "a", "Other", "C:\\Other.cpr" };

    PendingTodoOp added (const char* todoId, const char* text, bool delivered = false, int at = 0)
    {
        auto op = *makeAddOp (song, text, test::fixedTime (at));
        op.todoId = todoId;
        op.delivered = delivered;
        return op;
    }
}

TEST_CASE ("The list is the app's to-dos with the plugin's additions after them", "[todos]")
{
    const auto index = test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr", "", { { "t1", "Mix" } } } }));
    const std::vector<PendingTodoOp> pending { added ("t9", "Master") };

    const auto list = buildTodoList (index.find (song), pending, song);
    REQUIRE (list.size() == 2);
    CHECK (list[0].text == "Mix");
    CHECK_FALSE (list[0].pending);
    CHECK (list[1].text == "Master");
    CHECK (list[1].pending);
}

TEST_CASE ("A tick made in the plugin shows before the app confirms it", "[todos]")
{
    const auto index = test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr", "", { { "t1", "Mix" } } } }));
    std::vector<PendingTodoOp> pending;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime());

    const auto list = buildTodoList (index.find (song), pending, song);
    REQUIRE (list.size() == 1);
    CHECK (list[0].completed);
    CHECK (list[0].pending);
}

TEST_CASE ("A tick on a to-do added in the plugin lands on it", "[todos]")
{
    std::vector<PendingTodoOp> pending { added ("t9", "Master") };
    addSetCompletedOp (pending, song, "t9", true, test::fixedTime (1));

    const auto list = buildTodoList (nullptr, pending, song);
    REQUIRE (list.size() == 1);
    CHECK (list[0].completed);
}

TEST_CASE ("Another project's pending ops don't show", "[todos]")
{
    auto op = added ("t9", "For the other song");
    op.project = otherSong;
    CHECK (buildTodoList (nullptr, { op }, song).empty());
}

TEST_CASE ("A tick that matches the app's state isn't marked pending", "[todos]")
{
    const auto index = test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr", "", { { "t1", "Mix", true } } } }));
    std::vector<PendingTodoOp> pending;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime());
    CHECK_FALSE (buildTodoList (index.find (song), pending, song)[0].pending);
}

TEST_CASE ("A delivered add is dropped once the index has it", "[todos]")
{
    std::vector<PendingTodoOp> pending { added ("t9", "Master", true) };

    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr" } })));
    CHECK (pending.size() == 1);

    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr", "", { { "t9", "Master" } } } })));
    CHECK (pending.empty());
}

TEST_CASE ("An undelivered op is never dropped, whatever the index says", "[todos]")
{
    std::vector<PendingTodoOp> pending { added ("t9", "Master", false) };
    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr", "", { { "t9", "Master" } } } })));
    CHECK (pending.size() == 1);
}

TEST_CASE ("A delivered tick is dropped once a newer index agrees", "[todos]")
{
    std::vector<PendingTodoOp> pending;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime (10));
    pending[0].delivered = true;

    // Written before the tick: says nothing about it, even if it happens to agree.
    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "x", "", { { "t1", "Mix", true } } } }, test::fixedTime (5))));
    CHECK (pending.size() == 1);

    // Written after but disagreeing: the app hasn't applied it yet.
    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "x", "", { { "t1", "Mix", false } } } }, test::fixedTime (20))));
    CHECK (pending.size() == 1);

    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "x", "", { { "t1", "Mix", true } } } }, test::fixedTime (20))));
    CHECK (pending.empty());
}

TEST_CASE ("A tick on a to-do deleted in the app is dropped", "[todos]")
{
    std::vector<PendingTodoOp> pending;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime (10));
    pending[0].delivered = true;

    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "x" } }, test::fixedTime (20))));
    CHECK (pending.empty());
}

TEST_CASE ("A tick waits while the add it belongs to is still pending", "[todos]")
{
    std::vector<PendingTodoOp> pending { added ("t9", "Master", false) };
    addSetCompletedOp (pending, song, "t9", true, test::fixedTime (10));
    pending[1].delivered = true;

    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "p1", "a", "Song", "x" } }, test::fixedTime (20))));
    CHECK (pending.size() == 2);
}

TEST_CASE ("Ops for a project missing from the index are kept", "[todos]")
{
    std::vector<PendingTodoOp> pending { added ("t9", "Master", true) };
    pruneAcknowledged (pending, test::parseIndex (test::indexJson ({ { "zz", "a", "Elsewhere", "x" } })));
    CHECK (pending.size() == 1);
}

TEST_CASE ("Ticking twice before delivery leaves one op, the latest", "[todos]")
{
    std::vector<PendingTodoOp> pending;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime (1));
    addSetCompletedOp (pending, song, "t1", false, test::fixedTime (2));
    REQUIRE (pending.size() == 1);
    CHECK_FALSE (pending[0].completed);

    // A delivered tick is history the app has seen: a new one is queued after it.
    pending[0].delivered = true;
    addSetCompletedOp (pending, song, "t1", true, test::fixedTime (3));
    CHECK (pending.size() == 2);
}

TEST_CASE ("A to-do is one trimmed line, never blank, never unlinked", "[todos]")
{
    auto op = makeAddOp (song, "  Fix\tthe\r\nkick   drum  ", test::fixedTime());
    REQUIRE (op.has_value());
    CHECK (op->text == "Fix the kick drum");
    CHECK (op->todoId.isNotEmpty());
    CHECK (op->opId != op->todoId);

    CHECK_FALSE (makeAddOp (song, "   \n ", test::fixedTime()).has_value());
    CHECK_FALSE (makeAddOp ({}, "text", test::fixedTime()).has_value());

    const auto longText = juce::String::repeatedString ("a", maxTodoLength + 50);
    CHECK (makeAddOp (song, longText, test::fixedTime())->text.length() == maxTodoLength);
}

TEST_CASE ("The playhead stamp matches the app's timestamped to-dos", "[todos]")
{
    // playback_todo_utils.dart: "[${formatPlaybackTimestamp(position)}] ${note.trim()}"
    CHECK (stampWithTime ("Fix the kick", 83.9) == "[01:23] Fix the kick");
    CHECK (stampWithTime ("Intro", 0.0) == "[00:00] Intro");
    CHECK (formatClock (3723.0) == "01:02:03");
    CHECK (formatClock (-5.0) == "00:00");
    CHECK (formatClock (59.999) == "00:59");
}
