#pragma once

#include "BridgeSession.h"

#include <memory>
#include <optional>

namespace dpm
{

/** One BridgeSession driven by JSON commands, one per line, for the app's
    integration tests: the app runs DAWProjectManagerBridgeDriver against a
    temp library and talks to it over stdin/stdout. Each command gets exactly
    one reply line, {"ok":true,…} or {"ok":false,"error":"…"}.

    Here so the commands are tested with the rest of src/core; the console
    app around it (tools/BridgeDriverMain.cpp) only reads and writes lines.
*/
class BridgeDriver
{
public:
    /** `takeFolder` is where captures are written before the session judges
        them, as the plugin writes them outside the bridge folder.
    */
    BridgeDriver (BridgeFolder folder, juce::File takeFolder);

    /** The line printed before any command is read. */
    juce::String readyLine() const;

    /** Handles one command line and returns its reply line. */
    juce::String handle (const juce::String& line);

    /** A "quit" command was handled. */
    bool wantsToQuit() const { return quit; }

    BridgeSession& session() { return *bridgeSession; }

private:
    juce::var run (const juce::String& cmd, const juce::var& args);
    juce::var capture (const juce::var& args);
    juce::var stateReply() const;

    std::optional<juce::Time> fixedTime;
    juce::File takes;
    int takeNumber = 0;
    bool quit = false;
    std::unique_ptr<BridgeSession> bridgeSession;
};

} // namespace dpm
