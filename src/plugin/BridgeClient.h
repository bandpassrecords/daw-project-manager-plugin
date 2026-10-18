#pragma once

#include "core/CompanionSession.h"

#include <juce_events/juce_events.h>

namespace dpm
{

/** Runs a CompanionSession on the message thread: polls the bridge folder,
    keeps the host context up to date, links, delivers, and tells the editor
    when something changed.

    The session is behind a lock because hosts may save and restore plugin
    state from other threads. Everything else happens on the message thread.
*/
class BridgeClient : public juce::ChangeBroadcaster,
                     private juce::Timer
{
public:
    explicit BridgeClient (BridgeFolder folder);
    ~BridgeClient() override;

    /** Runs `fn` with the session locked. Message thread, or any thread for
        reads of the state.
    */
    template <typename Fn>
    auto withSession (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        return fn (session);
    }

    /** After the UI changed something: deliver at once and repaint. */
    void changed();

    juce::ValueTree saveState();
    void restoreState (const juce::ValueTree& tree);

    /** Message thread: a take finished writing. */
    void takeFinished (const FinishedTakeFile& take);

    /** What became of the last take, for the capture status line. */
    struct LastTakeReport
    {
        bool any = false;
        TakeVerdict verdict = TakeVerdict::keep;
        double seconds = 0;
        juce::Time at;
    };
    LastTakeReport lastTake() const;

    // --- The Standalone's test panel ------------------------------------------
    // A pretend DAW: the Standalone has no host to read, close or crash.

    /** Stands in for the host's name and window title. Empty title: read
        the real window as usual.
    */
    void setTestHost (const juce::String& hostName, const juce::String& windowTitle);

    /** The DAW closes the project: goodbye, as the host destroying the plugin. */
    void simulateProjectClosed();

    /** The DAW opens the project again: hello. */
    void simulateProjectReopened();

    /** The DAW crashes: everything stops, no goodbye, the heartbeat file is
        left to go stale — what the app sees when Cubase dies.
    */
    void simulateCrash();

    /** Back from a crash, as a freshly started DAW: hello and heartbeats again. */
    void simulateRecovery();

    bool isCrashed() const { return crashed; }

    JUCE_DECLARE_WEAK_REFERENCEABLE (BridgeClient)

private:
    void timerCallback() override;
    void readHostContext();

    juce::CriticalSection lock;
    CompanionSession session;
    LastTakeReport lastTakeReport;
    int ticks = 0;

    juce::String testHostName, testWindowTitle;
    bool crashed = false;
};

} // namespace dpm
