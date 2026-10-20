#include "TestHelpers.h"

#include "core/BridgeSession.h"
#include "core/Version.h"

using namespace dpm;

TEST_CASE ("Versions compare by number, not as text", "[version]")
{
    CHECK (isNewerVersion ("0.2.0", "0.1.0"));
    CHECK (isNewerVersion ("0.10.0", "0.9.9"));
    CHECK (isNewerVersion ("1.0.0", "0.99.99"));
    CHECK (isNewerVersion ("v0.1.1", "0.1.0"));
    CHECK_FALSE (isNewerVersion ("0.1.0", "0.1.0"));
    CHECK_FALSE (isNewerVersion ("0.1.0", "0.2.0"));
}

TEST_CASE ("Suffixes are ignored and malformed versions are never newer", "[version]")
{
    CHECK (isNewerVersion ("0.2.0-beta", "0.1.0"));
    CHECK_FALSE (isNewerVersion ("0.1.0+5", "0.1.0"));
    for (auto* bad : { "", "latest", "1.2", "1.2.3.4", "1.x.0", "-1.0.0" })
    {
        INFO (bad);
        CHECK_FALSE (isNewerVersion (bad, "0.0.1"));
    }
    CHECK_FALSE (isNewerVersion ("9.9.9", "not a version"));
}

namespace
{
    juce::String indexWithRelease (const char* version, const char* url)
    {
        auto json = juce::JSON::parse (test::indexJson ({ { "p1", "a", "Song", "C:\\Song.cpr" } }));
        auto* plugin = new juce::DynamicObject();
        plugin->setProperty ("latestVersion", version);
        plugin->setProperty ("releaseUrl", url);
        json.getDynamicObject()->setProperty ("companion", juce::var (plugin));
        return juce::JSON::toString (json);
    }

    std::optional<PluginRelease> updateFor (const juce::String& indexText, const char* installed)
    {
        test::TempDir tmp;
        const auto bridge = tmp.bridge();
        bridge.root.createDirectory();
        bridge.indexFile().replaceWithText (indexText);
        BridgeSession session (bridge);
        session.refresh();
        return session.availableUpdate (installed);
    }

    constexpr const char* releaseUrl = "https://github.com/bandpassrecords/daw-project-manager-plugin/releases/tag/v0.2.0";
}

TEST_CASE ("The index can name the newest plugin release", "[version]")
{
    juce::String error;
    auto index = ProjectIndex::parse (indexWithRelease ("0.2.0", releaseUrl), error);
    REQUIRE (index.has_value());
    REQUIRE (index->latestRelease.has_value());
    CHECK (index->latestRelease->version == "0.2.0");
    CHECK (index->latestRelease->url == releaseUrl);

    auto without = ProjectIndex::parse (test::indexJson ({}), error);
    CHECK_FALSE (without->latestRelease.has_value());
}

TEST_CASE ("An update is offered only when it is newer than what is installed", "[version]")
{
    const auto index = indexWithRelease ("0.2.0", releaseUrl);
    REQUIRE (updateFor (index, "0.1.0").has_value());
    CHECK (updateFor (index, "0.1.0")->version == "0.2.0");
    CHECK_FALSE (updateFor (index, "0.2.0").has_value());
    CHECK_FALSE (updateFor (index, "0.3.0").has_value());
}

TEST_CASE ("An update link anywhere but the plugin's release page is ignored", "[version]")
{
    CHECK_FALSE (updateFor (indexWithRelease ("0.2.0", "https://example.com/dpm-companion.exe"), "0.1.0").has_value());
    CHECK_FALSE (updateFor (indexWithRelease ("0.2.0", "https://github.com/someone-else/dpm-companion/releases"), "0.1.0").has_value());
    CHECK_FALSE (updateFor (indexWithRelease ("0.2.0", ""), "0.1.0").has_value());
}

TEST_CASE ("An app from before the repository's rename still offers the update", "[version]")
{
    const auto oldUrl = "https://github.com/bandpassrecords/dpm-companion/releases/tag/v0.2.0";
    REQUIRE (updateFor (indexWithRelease ("0.2.0", oldUrl), "0.1.0").has_value());
    CHECK (updateFor (indexWithRelease ("0.2.0", oldUrl), "0.1.0")->url == oldUrl);

    CHECK (isReleasePageUrl ("https://github.com/bandpassrecords/daw-project-manager-plugin/releases"));
    CHECK_FALSE (isReleasePageUrl ("https://github.com/bandpassrecords/daw-project-manager-plugin/releases-elsewhere"));
    CHECK_FALSE (isReleasePageUrl ("https://github.com/bandpassrecords/daw-project-manager/releases/tag/v2.10.0"));
}

TEST_CASE ("No index, no update", "[version]")
{
    test::TempDir tmp;
    BridgeSession session (tmp.bridge());
    session.refresh();
    CHECK_FALSE (session.availableUpdate ("0.0.1").has_value());
}
