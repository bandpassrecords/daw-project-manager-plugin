#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace dpm
{

enum class HostOs
{
    windows,
    macOS,
    linux
};

/** Everything that decides where the bridge folder is, gathered so the
    decision can be tested on any OS rather than only on the one CI runs.

    Paths are plain strings in the target OS's form: a juce::File can only hold
    a path native to the OS it runs on.
*/
struct BridgeEnvironment
{
    HostOs os = HostOs::windows;
    std::function<juce::String (const juce::String& name)> getEnv;
    juce::String home;
    std::function<bool (const juce::String& path)> isDirectory;

    /** The environment this process is actually running in. */
    static BridgeEnvironment current();
};

/** The app's application id: its macOS bundle id, Linux application id and
    Flatpak app id are all this one string.
*/
inline constexpr const char* appId = "com.bandpassrecords.dpm";

/** Environment variable that points the plugin at another bridge folder, for
    development builds of the app (which keep their library elsewhere) and for
    tests.
*/
inline constexpr const char* bridgeDirEnvVar = "DPM_BRIDGE_DIR";

/** The folder the app and the plugin exchange files through, as a path in
    env.os's form.

    It sits inside the app's own data directory, so it goes wherever the
    installed app's library goes:
      - Windows: %LOCALAPPDATA%\daw_project_manager\plugin_bridge
      - macOS:   ~/Library/Application Support/com.bandpassrecords.dpm/plugin_bridge
      - Linux:   $XDG_DATA_HOME/com.bandpassrecords.dpm/plugin_bridge, or the
                 Flatpak's ~/.var/app/com.bandpassrecords.dpm/data/… when only
                 the Flatpak has ever run.
*/
juce::String resolveBridgeRoot (const BridgeEnvironment& env);

/** The files inside the bridge folder. See docs/PROTOCOL.md. */
struct BridgeFolder
{
    juce::File root;

    /** The bridge folder of the app installed on this machine. */
    static BridgeFolder forThisMachine();

    juce::File indexFile() const   { return root.getChildFile ("index.json"); }
    juce::File launchFile() const  { return root.getChildFile ("launch.json"); }
    juce::File outboxDir() const   { return root.getChildFile ("outbox"); }
    juce::File capturesDir() const { return root.getChildFile ("captures"); }

    /** One heartbeat file per running plugin instance, so the app notices an
        instance that vanished without a goodbye (a crashed DAW).
    */
    juce::File instancesDir() const { return root.getChildFile ("instances"); }

    /** A path inside the bridge folder written as the protocol writes paths:
        relative to the root, with forward slashes on every OS.
    */
    juce::String relativePath (const juce::File& f) const;
};

} // namespace dpm
