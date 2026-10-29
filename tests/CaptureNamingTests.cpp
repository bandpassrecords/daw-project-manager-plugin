#include "TestHelpers.h"

#include "core/CaptureNaming.h"

using namespace dpm;

TEST_CASE ("A capture is named after its project and the local time", "[naming]")
{
    const juce::Time at (2026, 9, 18, 14, 2, 33, 0, true);
    CHECK (captureFileName ("My Song", at) == "My Song_20261018-140233.flac");
}

TEST_CASE ("Characters no file system allows are removed", "[naming]")
{
    const juce::Time at (2026, 9, 18, 14, 2, 33, 0, true);
    const auto name = captureFileName ("AC/DC: Back in <Black>?*|\"", at);
    for (auto c : juce::String ("\\/:*?\"<>|"))
        CHECK_FALSE (name.containsChar (c));
    CHECK (name.endsWith ("_20261018-140233.flac"));
}

TEST_CASE ("An unnamed project is 'Unlinked', and long names are cut", "[naming]")
{
    const juce::Time at (2026, 9, 18, 14, 2, 33, 0, true);
    CHECK (captureFileName ("", at).startsWith ("Unlinked_"));
    CHECK (captureFileName ("  ...  ", at).startsWith ("Unlinked_"));

    const auto longName = captureFileName (juce::String::repeatedString ("x", 300), at);
    CHECK (longName.length() <= 60 + juce::String ("_20261018-140233.flac").length());
}

TEST_CASE ("Takes being written are recognizable so the app skips them", "[naming]")
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory);
    const auto partial = dir.getChildFile (partialCaptureName ("1234-5678-abcd", 3));
    CHECK (isPartialCapture (partial));
    CHECK (isPartialCapture (dir.getChildFile (".Song.flac.part")));
    CHECK_FALSE (isPartialCapture (dir.getChildFile ("Song_20261018-140233.flac")));
}

TEST_CASE ("Takes left behind by a crash are deleted once they are a day old", "[naming]")
{
    test::TempDir tmp;
    const auto now = juce::Time::getCurrentTime();
    auto takeFile = [&tmp] (const juce::String& name, juce::Time modified)
    {
        auto f = tmp.dir.getChildFile (name);
        REQUIRE (f.replaceWithText ("audio"));
        REQUIRE (f.setLastModificationTime (modified));
        return f;
    };

    const auto crashed = takeFile (partialCaptureName ("a1b2c3", 1), now - juce::RelativeTime::days (2));
    const auto recording = takeFile (partialCaptureName ("d4e5f6", 7), now - juce::RelativeTime::minutes (3));
    const auto notATake = takeFile ("notes.txt", now - juce::RelativeTime::days (30));

    CHECK (deleteStaleTakes (tmp.dir, now) == 1);
    CHECK_FALSE (crashed.exists());
    CHECK (recording.existsAsFile());   // another DAW may still be writing it
    CHECK (notATake.existsAsFile());    // never anything but a take

    CHECK (deleteStaleTakes (tmp.dir.getChildFile ("missing"), now) == 0);
}
