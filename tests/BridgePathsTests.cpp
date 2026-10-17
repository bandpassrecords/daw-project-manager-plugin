#include "core/BridgePaths.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace dpm;

namespace
{
    BridgeEnvironment env (HostOs os, juce::String home, std::map<juce::String, juce::String> vars = {},
                           std::set<juce::String> dirs = {})
    {
        BridgeEnvironment e;
        e.os = os;
        e.home = std::move (home);
        e.getEnv = [vars] (const juce::String& name)
        {
            auto it = vars.find (name);
            return it != vars.end() ? it->second : juce::String();
        };
        e.isDirectory = [dirs] (const juce::String& path) { return dirs.count (path) > 0; };
        return e;
    }
}

TEST_CASE ("Windows: the bridge is inside the release app's LOCALAPPDATA library", "[paths]")
{
    const auto e = env (HostOs::windows, "C:\\Users\\ana", { { "LOCALAPPDATA", "C:\\Users\\ana\\AppData\\Local" } });
    CHECK (resolveBridgeRoot (e) == "C:\\Users\\ana\\AppData\\Local\\daw_project_manager\\plugin_bridge");
}

TEST_CASE ("Windows: without LOCALAPPDATA it is derived from the home folder", "[paths]")
{
    const auto e = env (HostOs::windows, "C:\\Users\\ana");
    CHECK (resolveBridgeRoot (e) == "C:\\Users\\ana\\AppData\\Local\\daw_project_manager\\plugin_bridge");
}

TEST_CASE ("macOS: the bridge is in Application Support under the bundle id", "[paths]")
{
    const auto e = env (HostOs::macOS, "/Users/ana");
    CHECK (resolveBridgeRoot (e) == "/Users/ana/Library/Application Support/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("Linux: XDG_DATA_HOME wins over ~/.local/share", "[paths]")
{
    CHECK (resolveBridgeRoot (env (HostOs::linux, "/home/ana"))
           == "/home/ana/.local/share/com.bandpassrecords.dpm/plugin_bridge");
    CHECK (resolveBridgeRoot (env (HostOs::linux, "/home/ana", { { "XDG_DATA_HOME", "/data/ana" } }))
           == "/data/ana/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("Linux: a relative XDG_DATA_HOME is ignored, as the XDG spec says", "[paths]")
{
    CHECK (resolveBridgeRoot (env (HostOs::linux, "/home/ana", { { "XDG_DATA_HOME", "data" } }))
           == "/home/ana/.local/share/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("Linux: only the Flatpak has run -> the bridge is in its sandbox data", "[paths]")
{
    const auto e = env (HostOs::linux, "/home/ana", {}, { "/home/ana/.var/app/com.bandpassrecords.dpm" });
    CHECK (resolveBridgeRoot (e)
           == "/home/ana/.var/app/com.bandpassrecords.dpm/data/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("Linux: with both installed, the native app's library wins", "[paths]")
{
    const auto e = env (HostOs::linux, "/home/ana", {},
                        { "/home/ana/.var/app/com.bandpassrecords.dpm", "/home/ana/.local/share/com.bandpassrecords.dpm" });
    CHECK (resolveBridgeRoot (e) == "/home/ana/.local/share/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("DPM_BRIDGE_DIR overrides the location, but only with an absolute path", "[paths]")
{
    CHECK (resolveBridgeRoot (env (HostOs::windows, "C:\\Users\\ana", { { "DPM_BRIDGE_DIR", "D:\\dev\\bridge" } }))
           == "D:\\dev\\bridge");
    CHECK (resolveBridgeRoot (env (HostOs::macOS, "/Users/ana", { { "DPM_BRIDGE_DIR", "/tmp/bridge" } }))
           == "/tmp/bridge");
    CHECK (resolveBridgeRoot (env (HostOs::macOS, "/Users/ana", { { "DPM_BRIDGE_DIR", "relative/bridge" } }))
           == "/Users/ana/Library/Application Support/com.bandpassrecords.dpm/plugin_bridge");
}

TEST_CASE ("Paths inside the bridge are written relative, with forward slashes", "[paths]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bridge");
    const BridgeFolder bridge { root };
    CHECK (bridge.relativePath (bridge.capturesDir().getChildFile ("Song_1.flac")) == "captures/Song_1.flac");
}
