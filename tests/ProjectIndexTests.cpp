#include "TestHelpers.h"

using namespace dpm;

TEST_CASE ("The index lists projects with their to-dos", "[index]")
{
    const auto index = test::parseIndex (test::indexJson ({
        { "p1", "prof1", "My Song", "C:\\Music\\My Song.cpr", "Cubase", { { "t1", "Fix kick" }, { "t2", "Vocals", true } } },
        { "p2", "prof1", "Other", "/music/other.als", "Ableton Live" },
    }));

    CHECK (index.schema == 1);
    CHECK (index.appVersion == "2.10.0");
    CHECK (index.generatedAt == test::fixedTime());
    REQUIRE (index.projects.size() == 2);

    const auto& p = index.projects[0];
    CHECK (p.name == "My Song");
    CHECK (p.profileName == "PROF1");
    CHECK (p.dawType == "Cubase");
    REQUIRE (p.todos.size() == 2);
    CHECK (p.todos[0].text == "Fix kick");
    CHECK_FALSE (p.todos[0].completed);
    CHECK (p.todos[1].completed);
    CHECK (p.findTodo ("t2") != nullptr);
    CHECK (p.findTodo ("nope") == nullptr);
}

TEST_CASE ("Projects are found by profile and id, never id alone", "[index]")
{
    // Old restores copied one project into every profile: same id, two rows.
    const auto index = test::parseIndex (test::indexJson ({
        { "p1", "solo", "Song (solo)", "C:\\a.cpr" },
        { "p1", "duo", "Song (duo)", "C:\\b.cpr" },
    }));

    REQUIRE (index.find ("duo", "p1") != nullptr);
    CHECK (index.find ("duo", "p1")->name == "Song (duo)");
    CHECK (index.find ("solo", "p1")->name == "Song (solo)");
    CHECK (index.find ("nobody", "p1") == nullptr);
    CHECK (index.find ("solo", "") == nullptr);
}

TEST_CASE ("File names are read with either separator, whatever the OS", "[index]")
{
    IndexedProject p;
    p.filePath = "C:\\Music\\Album\\My Song v2.cpr";
    CHECK (p.fileName() == "My Song v2.cpr");
    CHECK (p.fileStem() == "My Song v2");

    p.filePath = "/Users/ana/Music/Track.logicx/";
    CHECK (p.fileName() == "Track.logicx");
    CHECK (p.fileStem() == "Track");

    p.filePath = "/music/.hidden";
    CHECK (p.fileStem() == ".hidden");

    p.filePath = "no-extension";
    CHECK (p.fileStem() == "no-extension");
}

TEST_CASE ("A project without a name takes its file's", "[index]")
{
    const auto index = test::parseIndex (test::indexJson ({ { "p1", "prof1", "", "C:\\Music\\Untitled 3.flp" } }));
    CHECK (index.projects[0].name == "Untitled 3");
}

TEST_CASE ("Unknown fields are ignored and rows without an id are skipped", "[index]")
{
    const auto json = R"({
        "schema": 1,
        "generatedAt": "2026-10-18T12:00:00Z",
        "somethingNew": { "the app": "added this later" },
        "projects": [
            { "id": "p1", "profileId": "a", "name": "Kept", "filePath": "x.cpr", "color": "#ff0000",
              "todos": [ { "id": "t1", "text": "ok" }, { "text": "no id" }, "not an object" ] },
            { "profileId": "a", "name": "No id" },
            42
        ]
    })";

    const auto index = test::parseIndex (json);
    REQUIRE (index.projects.size() == 1);
    CHECK (index.projects[0].todos.size() == 1);
}

TEST_CASE ("A broken or schema-less index is refused with a reason", "[index]")
{
    juce::String error;
    CHECK_FALSE (ProjectIndex::parse ("{ not json", error).has_value());
    CHECK (error.isNotEmpty());

    error.clear();
    CHECK_FALSE (ProjectIndex::parse ("[1, 2, 3]", error).has_value());
    CHECK (error.isNotEmpty());

    error.clear();
    CHECK_FALSE (ProjectIndex::parse (R"({ "projects": [] })", error).has_value());
    CHECK (error.contains ("schema"));
}

TEST_CASE ("The launch hint names a project and when it was opened", "[index]")
{
    auto hint = LaunchHint::parse (R"({ "projectId": "p1", "profileId": "a", "filePath": "C:\\x.cpr",
                                        "launchedAt": "2026-10-18T12:00:00.000Z" })");
    REQUIRE (hint.has_value());
    CHECK (hint->projectId == "p1");
    CHECK (hint->launchedAt == test::fixedTime());

    CHECK_FALSE (LaunchHint::parse (R"({ "projectId": "p1" })").has_value());
    CHECK_FALSE (LaunchHint::parse (R"({ "launchedAt": "2026-10-18T12:00:00Z" })").has_value());
    CHECK_FALSE (LaunchHint::parse ("garbage").has_value());
}
