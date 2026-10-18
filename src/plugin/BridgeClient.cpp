#include "BridgeClient.h"

#include "platform/HostWindowTitle.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace dpm
{

namespace
{
    constexpr int tickMs = 2000;

    // The window title is re-read every few ticks: it changes when the
    // project is saved under a new name, and reading it walks every window.
    constexpr int titleEveryTicks = 3;

    // Well inside protocol::heartbeatStaleSeconds, so a busy message thread
    // missing a tick or two never reads as a crash.
    constexpr int heartbeatEveryTicks = protocol::heartbeatSeconds * 1000 / tickMs;
}

BridgeClient::BridgeClient (BridgeFolder folder) : session (std::move (folder))
{
    readHostContext();
    session.refresh();
    session.autoLink();
    startTimer (tickMs);
}

BridgeClient::~BridgeClient()
{
    stopTimer();

    // The host destroys the plugin when the project closes, the DAW quits or
    // the plugin is removed: the app may end the work session on this. A
    // simulated crash says nothing, as a real one couldn't.
    const juce::ScopedLock sl (lock);
    if (! crashed)
        session.sayGoodbye();
}

void BridgeClient::readHostContext()
{
    protocol::HostContext context;
    {
        const juce::ScopedLock sl (lock);
        context.hostName = testHostName;
        context.windowTitle = testWindowTitle;
    }
    if (context.hostName.isEmpty())
        context.hostName = juce::PluginHostType().getHostDescription();
    if (context.windowTitle.isEmpty())
        context.windowTitle = readHostWindowTitle();

    const juce::ScopedLock sl (lock);
    session.setHostContext (context);
}

void BridgeClient::setTestHost (const juce::String& hostName, const juce::String& windowTitle)
{
    {
        const juce::ScopedLock sl (lock);
        testHostName = hostName.trim();
        testWindowTitle = windowTitle.trim();
    }
    readHostContext();
    {
        const juce::ScopedLock sl (lock);
        session.autoLink();
    }
    changed();
}

void BridgeClient::simulateProjectClosed()
{
    {
        const juce::ScopedLock sl (lock);
        session.sayGoodbye();
    }
    sendChangeMessage();
}

void BridgeClient::simulateProjectReopened()
{
    {
        const juce::ScopedLock sl (lock);
        session.announceAgain();
    }
    changed();
}

void BridgeClient::simulateCrash()
{
    crashed = true;
    stopTimer();
    sendChangeMessage();
}

void BridgeClient::simulateRecovery()
{
    if (! crashed)
        return;
    crashed = false;
    {
        const juce::ScopedLock sl (lock);
        session.announceAgain();
    }
    startTimer (tickMs);
    changed();
}

void BridgeClient::timerCallback()
{
    if (++ticks % titleEveryTicks == 1)
        readHostContext();

    bool somethingChanged = false;
    {
        const juce::ScopedLock sl (lock);
        somethingChanged |= session.refresh();
        somethingChanged |= session.autoLink();
        somethingChanged |= session.deliver() > 0;

        if (ticks % heartbeatEveryTicks == 0)
            session.heartbeat();
    }

    if (somethingChanged)
        sendChangeMessage();
}

void BridgeClient::changed()
{
    {
        const juce::ScopedLock sl (lock);
        // A (simulated) crashed DAW delivers nothing; changes wait in the
        // state as they would in the project file.
        if (! crashed)
            session.deliver();
    }
    sendChangeMessage();
}

juce::ValueTree BridgeClient::saveState()
{
    const juce::ScopedLock sl (lock);
    return session.state().toValueTree();
}

void BridgeClient::restoreState (const juce::ValueTree& tree)
{
    {
        const juce::ScopedLock sl (lock);
        session.restore (CompanionState::fromValueTree (tree));
    }
    sendChangeMessage();    // async: safe from any thread
}

void BridgeClient::takeFinished (const FinishedTakeFile& take)
{
    {
        const juce::ScopedLock sl (lock);
        const auto outcome = session.handleFinishedTake (take);
        lastTakeReport = { true, outcome.verdict, take.summary.seconds, juce::Time::getCurrentTime() };
    }
    sendChangeMessage();
}

BridgeClient::LastTakeReport BridgeClient::lastTake() const
{
    const juce::ScopedLock sl (lock);
    return lastTakeReport;
}

} // namespace dpm
