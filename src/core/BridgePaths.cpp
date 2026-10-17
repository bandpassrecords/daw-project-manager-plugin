#include "BridgePaths.h"

namespace dpm
{

BridgeEnvironment BridgeEnvironment::current()
{
    BridgeEnvironment env;

   #if JUCE_WINDOWS
    env.os = HostOs::windows;
   #elif JUCE_MAC
    env.os = HostOs::macOS;
   #else
    env.os = HostOs::linux;
   #endif

    env.getEnv = [] (const juce::String& name)
    {
        return juce::SystemStats::getEnvironmentVariable (name, {});
    };
    env.home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
    env.isDirectory = [] (const juce::String& path)
    {
        return juce::File::isAbsolutePath (path) && juce::File (path).isDirectory();
    };
    return env;
}

namespace
{
    // An absolute path for this OS. A relative override would silently land
    // in whatever directory the host happens to run in.
    bool isAbsoluteFor (HostOs os, const juce::String& path)
    {
        if (os == HostOs::windows)
            return (path.length() >= 3 && juce::CharacterFunctions::isLetter (path[0])
                      && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
                   || path.startsWith ("\\\\");

        return path.startsWithChar ('/');
    }

    juce::String join (HostOs os, juce::String base, std::initializer_list<const char*> parts)
    {
        const auto sep = os == HostOs::windows ? juce::String ("\\") : juce::String ("/");

        for (auto* p : parts)
        {
            if (! base.endsWith (sep))
                base << sep;
            base << p;
        }
        return base;
    }
}

juce::String resolveBridgeRoot (const BridgeEnvironment& env)
{
    const auto override = env.getEnv (bridgeDirEnvVar).trim();
    if (override.isNotEmpty() && isAbsoluteFor (env.os, override))
        return override;

    switch (env.os)
    {
        case HostOs::windows:
        {
            const auto localAppData = env.getEnv ("LOCALAPPDATA").trim();
            const auto base = isAbsoluteFor (env.os, localAppData)
                                  ? localAppData
                                  : join (env.os, env.home, { "AppData", "Local" });
            // The release app's library folder (app_paths.dart: defaultAppDataDirName).
            return join (env.os, base, { "daw_project_manager", "plugin_bridge" });
        }

        case HostOs::macOS:
            // Not sandboxed (Release.entitlements), so path_provider's support
            // directory is the plain one named after the bundle id.
            return join (env.os, env.home, { "Library", "Application Support", appId, "plugin_bridge" });

        case HostOs::linux:
        {
            const auto xdg = env.getEnv ("XDG_DATA_HOME").trim();
            const auto dataHome = isAbsoluteFor (env.os, xdg) ? xdg
                                                              : join (env.os, env.home, { ".local", "share" });
            const auto native = join (env.os, dataHome, { appId });

            // A Flatpak keeps its data under ~/.var/app/<id>/data, where
            // path_provider again appends the application id. Only when the
            // native app has never run here: with both installed, the native
            // app's library is the one already in use.
            const auto flatpakApp = join (env.os, env.home, { ".var", "app", appId });
            if (! env.isDirectory (native) && env.isDirectory (flatpakApp))
                return join (env.os, flatpakApp, { "data", appId, "plugin_bridge" });

            return join (env.os, native, { "plugin_bridge" });
        }
    }

    return {};
}

BridgeFolder BridgeFolder::forThisMachine()
{
    return { juce::File (resolveBridgeRoot (BridgeEnvironment::current())) };
}

juce::String BridgeFolder::relativePath (const juce::File& f) const
{
    return f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
}

} // namespace dpm
