#include "CapturePolicy.h"

namespace dpm
{

void CapturePolicy::reset (double newSampleRate)
{
    sampleRate = newSampleRate > 0 ? newSampleRate : 44100.0;
    state = State::idle;
    manualTake = false;
    recorded = 0;
    silentRun = 0;
}

int CapturePolicy::samplesFor (const CaptureBlock& block) const
{
    const auto room = juce::jmax ((juce::int64) 0, block.maxSamples - recorded);
    return (int) juce::jmin ((juce::int64) block.numSamples, room);
}

CaptureStep CapturePolicy::startTake (const CaptureBlock& block, bool manual)
{
    state = State::recording;
    manualTake = manual;
    recorded = 0;
    silentRun = 0;

    CaptureStep step;
    step.startTake = true;
    step.samplesToWrite = samplesFor (block);
    recorded += step.samplesToWrite;

    if (recorded >= block.maxSamples)
    {
        step.end = TakeEnd::reachedMax;
        state = manual ? State::idle : State::waitingForStop;
    }
    return step;
}

CaptureStep CapturePolicy::endTake (TakeEnd why, State next)
{
    state = next;
    CaptureStep step;
    step.end = why;
    return step;
}

CaptureStep CapturePolicy::process (const CaptureBlock& block)
{
    if (block.mode != lastMode)
    {
        lastMode = block.mode;
        if (state == State::recording)
            return endTake (TakeEnd::modeChanged, State::idle);
        state = State::idle;
    }

    switch (block.mode)
    {
        case CaptureMode::off:
            return {};

        case CaptureMode::manual:
        {
            if (state != State::recording)
                return block.manualStart ? startTake (block, true) : CaptureStep {};

            if (block.manualStop)
                return endTake (TakeEnd::manualStop, State::idle);

            CaptureStep step;
            step.samplesToWrite = samplesFor (block);
            recorded += step.samplesToWrite;
            if (recorded >= block.maxSamples)
            {
                step.end = TakeEnd::reachedMax;
                state = State::idle;
            }
            return step;
        }

        case CaptureMode::automatic:
            break;
    }

    if (! block.isRealtime)
        return state == State::recording ? endTake (TakeEnd::offline, State::waitingForStop)
                                         : (state = State::waitingForStop, CaptureStep {});

    if (! block.isPlaying)
        return state == State::recording ? endTake (TakeEnd::stopped, State::idle)
                                         : (state = State::idle, CaptureStep {});

    if (block.isLooping)
        return state == State::recording ? endTake (TakeEnd::looped, State::waitingForStop)
                                         : (state = State::waitingForStop, CaptureStep {});

    switch (state)
    {
        case State::idle:
        case State::waitingForSound:
            state = State::waitingForSound;
            if (block.peak >= audibleThreshold)
                return startTake (block, false);
            return {};

        case State::recording:
        {
            CaptureStep step;
            step.samplesToWrite = samplesFor (block);
            recorded += step.samplesToWrite;
            silentRun = block.peak < audibleThreshold ? silentRun + block.numSamples : 0;

            if (recorded >= block.maxSamples)
            {
                step.end = TakeEnd::reachedMax;
                state = State::waitingForStop;
            }
            else if ((double) silentRun >= silenceEndSeconds * sampleRate)
            {
                step.end = TakeEnd::silence;
                state = State::waitingForStop;
            }
            return step;
        }

        case State::waitingForStop:
            return {};
    }

    return {};
}

TakeVerdict judgeTake (const TakeSummary& take, const LastCapture& last)
{
    switch (take.end)
    {
        case TakeEnd::looped:
        case TakeEnd::offline:
        case TakeEnd::modeChanged:
        case TakeEnd::none:
            return TakeVerdict::discardEnded;

        case TakeEnd::stopped:
        case TakeEnd::reachedMax:
        case TakeEnd::silence:
        case TakeEnd::manualStop:
            break;
    }

    if (take.overflowed)
        return TakeVerdict::discardIncomplete;
    if (take.peak < audibleThreshold)
        return TakeVerdict::discardSilent;

    if (take.manual)
        return take.seconds >= minManualSeconds ? TakeVerdict::keep : TakeVerdict::discardTooShort;

    if (take.seconds < minAutoSeconds)
        return TakeVerdict::discardTooShort;

    if (take.end == TakeEnd::reachedMax || ! last.exists())
        return TakeVerdict::keep;

    // Half a second of slack: two passes over the same section rarely come
    // out at exactly the same length.
    return take.seconds + 0.5 >= last.seconds ? TakeVerdict::keep : TakeVerdict::discardNotBetter;
}

} // namespace dpm
