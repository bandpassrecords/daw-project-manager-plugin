// DAWProjectManagerBridgeDriver: one BridgeSession with no audio, GUI or host,
// driven by JSON lines on stdin, for the app's integration tests. See
// docs/development.md ("The bridge driver") for the commands.
//
// stdout carries exactly the ready line and one reply per command; anything
// else goes to stderr.

#include "core/BridgeDriver.h"

#include <iostream>
#include <string>

#if JUCE_WINDOWS
 #include <fcntl.h>
 #include <io.h>
#endif

namespace
{
    void writeLine (const juce::String& line)
    {
        std::cout << line.toRawUTF8() << '\n' << std::flush;
    }
}

int main()
{
   #if JUCE_WINDOWS
    // "\n", not "\r\n": the reader splits on newlines on every OS.
    _setmode (_fileno (stdout), _O_BINARY);
   #endif

    // Only ever a folder the caller names: never the installed app's library.
    const auto dir = juce::SystemStats::getEnvironmentVariable (dpm::bridgeDirEnvVar, {});
    if (dir.isEmpty() || ! juce::File::isAbsolutePath (dir))
    {
        writeLine (R"({"ok":false,"error":"set DPM_BRIDGE_DIR to an absolute path: the driver never uses the real library"})");
        std::cerr << "DPM_BRIDGE_DIR is not set to an absolute path\n";
        return 2;
    }

    const juce::File root (dir);
    root.createDirectory();

    const auto takes = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("DAWProjectManagerBridgeDriver")
                           .getChildFile (juce::Uuid().toString());

    {
        dpm::BridgeDriver driver (dpm::BridgeFolder { root }, takes);
        writeLine (driver.readyLine());

        std::string line;
        while (! driver.wantsToQuit() && std::getline (std::cin, line))
        {
            const auto text = juce::String::fromUTF8 (line.data(), (int) line.size()).trim();
            if (text.isNotEmpty())
                writeLine (driver.handle (text));
        }
    }

    takes.deleteRecursively();
    return 0;
}
