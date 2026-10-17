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
}

void BridgeClient::readHostContext()
{
    protocol::HostContext context;
    context.hostName = juce::PluginHostType().getHostDescription();
    context.windowTitle = readHostWindowTitle();

    const juce::ScopedLock sl (lock);
    session.setHostContext (context);
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
    }

    if (somethingChanged)
        sendChangeMessage();
}

void BridgeClient::changed()
{
    {
        const juce::ScopedLock sl (lock);
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
