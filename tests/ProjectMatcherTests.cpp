#include "TestHelpers.h"

#include "core/ProjectMatcher.h"

#include <set>

using namespace dpm;

namespace
{
    ProjectIndex library()
    {
        return test::parseIndex (test::indexJson ({
            { "cub", "a", "Teardrop Remix", "C:\\Music\\Teardrop Remix\\Teardrop Remix.cpr", "Cubase" },
            { "cub2", "a", "Teardrop Remix v2", "C:\\Music\\Teardrop Remix\\Teardrop Remix v2.cpr", "Cubase" },
            { "abl", "a", "Night Drive", "C:\\Music\\Night Drive Project\\Night Drive.als", "Ableton Live" },
            { "flp", "a", "Bounce", "C:\\Music\\FL\\Bounce.flp", "FL Studio" },
            { "rpp", "b", "Demo", "D:\\Reaper\\Demo\\Demo.rpp", "Reaper" },
            { "s1", "a", "Song", "C:\\Music\\Song.song", "Studio One" },
        }));
    }

    DetectionContext titled (const juce::String& host, const juce::String& title)
    {
        DetectionContext c;
        c.hostName = host;
        c.windowTitle = title;
        c.now = test::fixedTime();
        return c;
    }

    juce::String bestId (const ProjectIndex& index, const DetectionContext& ctx)
    {
        auto* p = confidentMatch (rankProjects (index, ctx));
        return p != nullptr ? p->id : juce::String ("<none>");
    }
}

TEST_CASE ("Each DAW's window title finds its project", "[matcher]")
{
    const auto index = library();
    CHECK (bestId (index, titled ("Steinberg Cubase 14", "Cubase Pro Project - Teardrop Remix")) == "cub");
    CHECK (bestId (index, titled ("Ableton Live 12 Suite", "Night Drive [Night Drive Project] - Ableton Live 12 Suite")) == "abl");
    CHECK (bestId (index, titled ("FL Studio", "Bounce.flp - FL Studio 21")) == "flp");
    CHECK (bestId (index, titled ("REAPER", "Demo [D:\\Reaper\\Demo\\Demo.rpp] - REAPER v7.22 - Registered to Ana")) == "rpp");
    CHECK (bestId (index, titled ("Studio One", "Studio One - Song")) == "s1");
}

TEST_CASE ("The longer, more specific name wins over a name it contains", "[matcher]")
{
    const auto index = library();
    CHECK (bestId (index, titled ("Cubase", "Cubase Pro Project - Teardrop Remix v2")) == "cub2");
}

TEST_CASE ("A name that is only part of the title's project name is a suggestion, not a link", "[matcher]")
{
    // "Demo Remix" is a project the app hasn't scanned yet: "Demo" must not
    // take its to-dos and work sessions.
    const auto index = library();
    const auto ranked = rankProjects (index, titled ("Steinberg Cubase 14", "Cubase Pro Project - Demo Remix"));
    REQUIRE (ranked.size() == 1);
    CHECK (ranked[0].project->id == "rpp");
    CHECK (ranked[0].score < confidentScore);
    CHECK (confidentMatch (ranked) == nullptr);

    // A launch from the app still makes it confident.
    auto launched = titled ("Steinberg Cubase 14", "Cubase Pro Project - Demo Remix");
    launched.launch = LaunchHint { "rpp", "b", "D:\\Reaper\\Demo\\Demo.rpp", test::fixedTime (-60) };
    CHECK (bestId (index, launched) == "rpp");
}

TEST_CASE ("The project's own part of the title links, however the DAW decorates it", "[matcher]")
{
    const auto index = library();
    CHECK (bestId (index, titled ("Studio One", "Studio One - Song*")) == "s1");               // unsaved
    CHECK (bestId (index, titled ("Pro Tools", "Edit: Song")) == "s1");
    CHECK (bestId (index, titled ("Bitwig Studio", "Bitwig Studio | Song")) == "s1");
    CHECK (bestId (index, titled ("Ableton Live 12", "Song (Song) - Ableton Live 12")) == "s1");
    CHECK (bestId (index, titled ("Studio One", juce::String::fromUTF8 ("Studio One \xe2\x80\x93 Song"))) == "s1");
}

TEST_CASE ("A name only counts as a whole word", "[matcher]")
{
    const auto index = library();
    // "Song" inside "Songwriting" and "Demo" inside "Demolition" are not matches.
    CHECK (rankProjects (index, titled ("Studio One", "Studio One - Songwriting Session")).empty());
    CHECK (rankProjects (index, titled ("REAPER", "Demolition - REAPER")).empty());
    CHECK (containsAsWord ("Studio One - Song*", "song"));
    CHECK_FALSE (containsAsWord ("Teardrop Remix v12", "Teardrop Remix v1"));
}

TEST_CASE ("Path matching ignores slash direction and case", "[matcher]")
{
    const auto index = library();
    const auto ranked = rankProjects (index, titled ("REAPER", "demo [d:/reaper/demo/DEMO.RPP] - REAPER"));
    REQUIRE_FALSE (ranked.empty());
    CHECK (ranked[0].project->id == "rpp");
    CHECK (ranked[0].reason == MatchReason::titleHasPath);
}

TEST_CASE ("A window title with no project in it matches nothing", "[matcher]")
{
    const auto index = library();
    CHECK (rankProjects (index, titled ("Bitwig Studio", "Bitwig Studio 5.2")).empty());
    CHECK (rankProjects (index, titled ("", "")).empty());
}

TEST_CASE ("A fresh launch from the app links when the title says nothing", "[matcher]")
{
    const auto index = library();
    auto ctx = titled ("Bitwig Studio", "Bitwig Studio 5.2");
    ctx.launch = LaunchHint { "abl", "a", {}, test::fixedTime (-120) };
    CHECK (bestId (index, ctx) == "abl");
}

TEST_CASE ("An old launch is only a suggestion", "[matcher]")
{
    const auto index = library();
    auto ctx = titled ("Bitwig Studio", "");
    ctx.launch = LaunchHint { "abl", "a", {}, test::fixedTime (-3 * 3600) };

    const auto ranked = rankProjects (index, ctx);
    REQUIRE (ranked.size() == 1);
    CHECK (ranked[0].reason == MatchReason::launchedFromApp);
    CHECK (confidentMatch (ranked) == nullptr);

    ctx.launch->launchedAt = test::fixedTime (-13 * 3600);
    CHECK (rankProjects (index, ctx).empty());
}

TEST_CASE ("The window title beats a launch of another project", "[matcher]")
{
    // Launched "Night Drive" from the app, then opened "Bounce" from inside the DAW.
    const auto index = library();
    auto ctx = titled ("FL Studio", "Bounce.flp - FL Studio 21");
    ctx.launch = LaunchHint { "abl", "a", {}, test::fixedTime (-60) };
    CHECK (bestId (index, ctx) == "flp");
}

TEST_CASE ("A launch hint without an id matches by file path", "[matcher]")
{
    const auto index = library();
    auto ctx = titled ("", "");
    ctx.launch = LaunchHint { {}, {}, "c:/music/fl/bounce.flp", test::fixedTime (-5) };
    CHECK (bestId (index, ctx) == "flp");
}

TEST_CASE ("Two projects scoring the same is not confident", "[matcher]")
{
    const auto index = test::parseIndex (test::indexJson ({
        { "x", "a", "Intro", "C:\\One\\Intro.cpr", "Cubase" },
        { "y", "b", "Intro", "C:\\Two\\Intro.cpr", "Cubase" },
    }));
    const auto ranked = rankProjects (index, titled ("Cubase", "Cubase Pro Project - Intro"));
    CHECK (ranked.size() == 2);
    CHECK (confidentMatch (ranked) == nullptr);
}

TEST_CASE ("The host's DAW breaks a tie between same-named projects of different DAWs", "[matcher]")
{
    const auto index = test::parseIndex (test::indexJson ({
        { "x", "a", "Intro", "C:\\One\\Intro.als", "Ableton Live" },
        { "y", "a", "Intro", "C:\\Two\\Intro.cpr", "Cubase" },
    }));
    CHECK (bestId (index, titled ("Steinberg Cubase 14", "Cubase Pro Project - Intro")) == "y");
}

TEST_CASE ("Host descriptions map to the app's DAW type names", "[matcher]")
{
    CHECK (dawTypeForHost ("Steinberg Cubase 14") == "Cubase");
    CHECK (dawTypeForHost ("Ableton Live 12") == "Ableton Live");
    CHECK (dawTypeForHost ("FL Studio") == "FL Studio");
    CHECK (dawTypeForHost ("REAPER") == "Reaper");
    CHECK (dawTypeForHost ("PreSonus Studio One") == "Studio One");
    CHECK (dawTypeForHost ("Bitwig Studio") == "Bitwig Studio");
    CHECK (dawTypeForHost ("Steinberg Nuendo") == "Nuendo");
    CHECK (dawTypeForHost ("Some Unknown Host") == "");
}

TEST_CASE ("The picker lists suggestions first, then everything by name", "[picker]")
{
    const auto index = library();
    const auto ranked = rankProjects (index, titled ("Cubase", "Cubase Pro Project - Teardrop Remix v2"));
    const auto rows = buildPickerRows (index, ranked, "");

    REQUIRE (rows.size() == index.projects.size());
    CHECK (rows[0].ref.id == "cub2");
    CHECK (rows[0].suggested);
    CHECK (rows[1].ref.id == "cub");     // "Teardrop Remix" also appears in the title
    CHECK (rows[1].suggested);
    CHECK_FALSE (rows[2].suggested);
    CHECK (rows[2].ref.name == "Bounce");
    CHECK (rows.back().ref.name == "Song");

    // No project twice.
    std::set<juce::String> ids;
    for (auto& r : rows)
        ids.insert (r.ref.id);
    CHECK (ids.size() == rows.size());
}

TEST_CASE ("A picker query keeps projects containing every word", "[picker]")
{
    const auto index = library();
    auto rows = buildPickerRows (index, {}, "tear V2");
    REQUIRE (rows.size() == 1);
    CHECK (rows[0].ref.id == "cub2");

    rows = buildPickerRows (index, {}, "b");   // profile "B" and names with a "b"
    CHECK (rows.size() == 2);

    CHECK (buildPickerRows (index, {}, "zzz").empty());
}

TEST_CASE ("Names sort naturally in the picker", "[picker]")
{
    const auto index = test::parseIndex (test::indexJson ({
        { "10", "a", "Track 10", "10.cpr" },
        { "2", "a", "Track 2", "2.cpr" },
        { "1", "a", "track 1", "1.cpr" },
    }));
    const auto rows = buildPickerRows (index, {}, "");
    REQUIRE (rows.size() == 3);
    CHECK (rows[0].ref.id == "1");
    CHECK (rows[1].ref.id == "2");
    CHECK (rows[2].ref.id == "10");
}
