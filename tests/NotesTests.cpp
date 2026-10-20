#include "TestHelpers.h"

#include "core/BridgeSession.h"
#include "core/NotesBook.h"

using namespace dpm;

namespace
{
    const protocol::ProjectRef song { "song", "solo", "Teardrop", "C:\\Music\\Teardrop.cpr" };

    /** index.json with notes for "song" and, optionally, applied message ids. */
    juce::String indexWithNotes (const juce::String& notes, juce::StringArray applied = {},
                                 juce::Time at = test::fixedTime())
    {
        auto json = juce::JSON::parse (test::indexJson ({ { "song", "solo", "Teardrop", "C:\\Music\\Teardrop.cpr" } }, at));
        json["projects"][0].getDynamicObject()->setProperty ("notes", notes);
        juce::Array<juce::var> ids;
        for (auto& id : applied)
            ids.add (id);
        json.getDynamicObject()->setProperty ("applied", ids);
        return juce::JSON::toString (json);
    }

    ProjectIndex parse (const juce::String& json) { return test::parseIndex (json); }
}

TEST_CASE ("The editor shows the plugin's unconfirmed edit, else the app's notes", "[notes]")
{
    const auto index = parse (indexWithNotes ("Kick too loud in the drop"));
    std::vector<PendingNotesOp> pending;
    CHECK (currentNotes (index.find (song), pending, song) == "Kick too loud in the drop");
    CHECK_FALSE (notesPending (pending, song));

    REQUIRE (addNotesEdit (pending, song, "Kick fixed", "Kick too loud in the drop", test::fixedTime()));
    CHECK (currentNotes (index.find (song), pending, song) == "Kick fixed");
    CHECK (notesPending (pending, song));
    CHECK (currentNotes (nullptr, {}, song).isEmpty());
}

TEST_CASE ("Saving again before delivery sends one edit, from the original text", "[notes]")
{
    std::vector<PendingNotesOp> pending;
    addNotesEdit (pending, song, "draft 1", "original", test::fixedTime());
    addNotesEdit (pending, song, "draft 2", "draft 1", test::fixedTime (5));
    REQUIRE (pending.size() == 1);
    CHECK (pending[0].text == "draft 2");
    CHECK (pending[0].baseText == "original");
    CHECK (pending[0].at == test::fixedTime (5));

    // Once delivered, the next edit is a new message, based on the delivered text.
    pending[0].delivered = true;
    addNotesEdit (pending, song, "draft 3", "draft 2", test::fixedTime (9));
    REQUIRE (pending.size() == 2);
    CHECK (pending[1].baseText == "draft 2");
}

TEST_CASE ("An edit that changes nothing, or has no project, is not an edit", "[notes]")
{
    std::vector<PendingNotesOp> pending;
    CHECK_FALSE (addNotesEdit (pending, song, "same", "same", test::fixedTime()));
    CHECK_FALSE (addNotesEdit (pending, {}, "new", "old", test::fixedTime()));
    CHECK (pending.empty());

    const auto huge = juce::String::repeatedString ("a", maxNotesLength + 100);
    REQUIRE (addNotesEdit (pending, song, huge, "", test::fixedTime()));
    CHECK (pending[0].text.length() == maxNotesLength);
}

TEST_CASE ("A delivered edit is confirmed by its id in 'applied', whatever the app made of it", "[notes]")
{
    std::vector<PendingNotesOp> pending;
    addNotesEdit (pending, song, "mine", "base", test::fixedTime());
    pending[0].delivered = true;
    const auto id = pending[0].opId;

    pruneAcknowledgedNotes (pending, parse (indexWithNotes ("base")));
    CHECK (pending.size() == 1);    // not applied yet

    // The app merged it with its own change: the text differs, the id says done.
    pruneAcknowledgedNotes (pending, parse (indexWithNotes ("theirs\n\n----------\nmine", { id })));
    CHECK (pending.empty());
}

TEST_CASE ("A delivered edit is also confirmed when the app's notes read exactly as edited", "[notes]")
{
    std::vector<PendingNotesOp> pending;
    addNotesEdit (pending, song, "mine", "base", test::fixedTime());
    pending[0].delivered = true;
    pruneAcknowledgedNotes (pending, parse (indexWithNotes ("mine")));
    CHECK (pending.empty());
}

TEST_CASE ("An undelivered edit is never dropped", "[notes]")
{
    std::vector<PendingNotesOp> pending;
    addNotesEdit (pending, song, "mine", "base", test::fixedTime());
    pruneAcknowledgedNotes (pending, parse (indexWithNotes ("mine", { pending[0].opId })));
    CHECK (pending.size() == 1);
}

TEST_CASE ("Notes edits survive saving the DAW project", "[notes][state]")
{
    auto s = PluginState::fresh();
    s.project = song;
    addNotesEdit (s.pendingNotes, song, "Line 1\nLine \"2\" <&>", "old", test::fixedTime (3));
    s.pendingNotes[0].delivered = true;

    const auto back = PluginState::fromValueTree (juce::ValueTree::fromXml (*s.toValueTree().createXml()));
    REQUIRE (back.pendingNotes.size() == 1);
    CHECK (back.pendingNotes[0].text == "Line 1\nLine \"2\" <&>");
    CHECK (back.pendingNotes[0].baseText == "old");
    CHECK (back.pendingNotes[0].delivered);
    CHECK (back.pendingNotes[0].at == test::fixedTime (3));
    CHECK (back.pendingNotes[0].project.sameProject (song));
    CHECK (back.pendingNotes[0].opId == s.pendingNotes[0].opId);
}

TEST_CASE ("Notes travel to the app and back through the session", "[notes][session]")
{
    test::TempDir tmp;
    const auto bridge = tmp.bridge();
    juce::Time clock = test::fixedTime();
    BridgeSession session (bridge, [&clock] { return clock; });

    REQUIRE (bridge.root.createDirectory());
    REQUIRE (bridge.indexFile().replaceWithText (indexWithNotes ("Vocals: comp verse 2")));
    session.refresh();
    session.linkTo (*session.index()->find ("solo", "song"), true);
    session.deliver();  // hello

    CHECK (session.notes() == "Vocals: comp verse 2");
    CHECK (session.setNotes ("Vocals: comp verse 2\nBass: sidechain"));
    CHECK_FALSE (session.setNotes ("Vocals: comp verse 2\nBass: sidechain"));     // unchanged
    CHECK (session.notesPending());
    CHECK (session.undeliveredCount() == 1);
    CHECK (session.deliver() == 1);

    const auto messages = test::outboxMessages (bridge);
    const auto& m = messages.back();
    CHECK (m["type"].toString() == "notes.set");
    CHECK (m["payload"]["text"].toString() == "Vocals: comp verse 2\nBass: sidechain");
    CHECK (m["payload"]["baseText"].toString() == "Vocals: comp verse 2");
    CHECK (m["id"].toString() == session.state().pendingNotes[0].opId);

    // The app applies it and lists the message as applied.
    clock = clock + juce::RelativeTime::seconds (3);
    REQUIRE (bridge.indexFile().replaceWithText (indexWithNotes ("Vocals: comp verse 2\nBass: sidechain",
                                                                 { m["id"].toString() }, clock)));
    bridge.indexFile().setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (5));
    session.refresh();
    CHECK_FALSE (session.notesPending());
    CHECK (session.notes() == "Vocals: comp verse 2\nBass: sidechain");
    CHECK (session.appNotes() == session.notes());
}
