#include "CaptureEngine.h"

#include "CaptureNaming.h"

namespace dpm
{

namespace
{
    // Seconds of audio the FIFO holds: the writer thread wakes every 40 ms,
    // so this is slack for a disk that stalls, not for normal running.
    constexpr double fifoSeconds = 4.0;
}

CaptureEngine::CaptureEngine (juce::File folder, juce::String tag, TakeCallback callback)
    : juce::Thread ("DAW Project Manager capture writer"),
      takeFolder (std::move (folder)),
      instanceTag (std::move (tag)),
      onTakeFinished (std::move (callback))
{
}

CaptureEngine::~CaptureEngine()
{
    release();
}

void CaptureEngine::prepare (double newSampleRate, int numChannels)
{
    stopWriterThread();

    sampleRate = newSampleRate > 0 ? newSampleRate : 44100.0;
    channels = juce::jlimit (1, 2, numChannels);

    const auto capacity = (int) (sampleRate * fifoSeconds);
    audioBuffer.setSize (channels, capacity, false, true, false);
    audioFifo.setTotalSize (capacity);
    audioFifo.reset();
    markerFifo.reset();

    policy.reset (sampleRate);
    framesPushed = 0;
    framesRead = 0;
    framesPushedShared.store (0);
    framesWritten.store (0);
    recording.store (false);
    manualTakeActive.store (false);
    samplesRecorded.store (0);

    startThread (juce::Thread::Priority::low);
}

void CaptureEngine::release()
{
    stopWriterThread();
    recording.store (false);
    manualTakeActive.store (false);
}

void CaptureEngine::stopWriterThread()
{
    if (! isThreadRunning())
        return;

    signalThreadShouldExit();
    notify();
    stopThread (5000);

    // A take still open when playback is torn down never ended properly.
    if (writer.isOpen())
        writer.abandon();
}

double CaptureEngine::recordedSeconds() const
{
    return (double) samplesRecorded.load() / sampleRate;
}

bool CaptureEngine::pushMarker (const Marker& marker)
{
    int start1, size1, start2, size2;
    markerFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 < 1)
        return false;

    markers[(size_t) (size1 > 0 ? start1 : start2)] = marker;
    markerFifo.finishedWrite (1);
    return true;
}

void CaptureEngine::process (const juce::AudioBuffer<float>& buffer, const TransportState& transport)
{
    if (audioBuffer.getNumSamples() == 0)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (channels, buffer.getNumChannels());
    if (numSamples <= 0 || numChannels <= 0)
        return;

    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));

    if (peak > meterPeak.load())
        meterPeak.store (peak);

    CaptureBlock block;
    block.isPlaying = transport.isPlaying;
    block.isLooping = transport.isLooping;
    block.isRealtime = transport.isRealtime;
    block.peak = peak;
    block.numSamples = numSamples;
    block.mode = mode.load();
    block.maxSamples = (juce::int64) (maxSeconds.load() * sampleRate);
    block.manualStart = manualStartRequested.exchange (false);
    block.manualStop = manualStopRequested.exchange (false);

    const auto step = policy.process (block);

    if (step.startTake)
    {
        takePeak = 0.0f;
        takeOverflowed = false;
        if (! pushMarker ({ Marker::Kind::start, framesPushed }))
            takeOverflowed = true;
    }

    if (step.samplesToWrite > 0)
    {
        // prepare/finished rather than AbstractFifo::write(), whose scope
        // object would commit a short write of samples never copied.
        int start1, size1, start2, size2;
        audioFifo.prepareToWrite (step.samplesToWrite, start1, size1, start2, size2);

        if (size1 + size2 == step.samplesToWrite)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                // A mono input fills both channels of a stereo take.
                const auto* src = buffer.getReadPointer (juce::jmin (ch, numChannels - 1));
                if (size1 > 0)
                    audioBuffer.copyFrom (ch, start1, src, size1);
                if (size2 > 0)
                    audioBuffer.copyFrom (ch, start2, src + size1, size2);
            }
            audioFifo.finishedWrite (step.samplesToWrite);
            framesPushed += step.samplesToWrite;
            framesPushedShared.store (framesPushed);
            takePeak = juce::jmax (takePeak, peak);
        }
        else
        {
            // The writer fell behind. Write nothing rather than a take with
            // a hole in it; the take is thrown away when it ends.
            takeOverflowed = true;
        }
    }

    if (step.end != TakeEnd::none)
        pushMarker ({ Marker::Kind::end, framesPushed, step.end, policy.isManualTake(), takePeak, takeOverflowed });

    recording.store (policy.isRecording());
    manualTakeActive.store (policy.isRecording() && policy.isManualTake());
    samplesRecorded.store (policy.samplesRecorded());
}

void CaptureEngine::run()
{
    while (! threadShouldExit())
    {
        drain();
        wait (40);
    }
    drain();
}

void CaptureEngine::drain()
{
    for (;;)
    {
        // Read how much audio is ready *before* looking at markers: a marker
        // pushed after this point can only refer to audio pushed after it.
        const auto ready = (juce::int64) audioFifo.getNumReady();

        int start1, size1, start2, size2;
        markerFifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 + size2 == 1)
        {
            const auto next = markers[(size_t) (size1 > 0 ? start1 : start2)];

            if (next.streamPos <= framesRead + ready)
            {
                drainAudio (next.streamPos - framesRead);
                markerFifo.finishedRead (1);
                handle (next);
                continue;
            }
        }

        drainAudio (ready);
        return;
    }
}

void CaptureEngine::drainAudio (juce::int64 frames)
{
    while (frames > 0)
    {
        const auto chunk = (int) juce::jmin (frames, (juce::int64) 8192);
        int start1, size1, start2, size2;
        audioFifo.prepareToRead (chunk, start1, size1, start2, size2);

        auto writeRegion = [this] (int start, int size)
        {
            if (size <= 0 || ! writer.isOpen())
                return;

            const float* ptrs[2] = { audioBuffer.getReadPointer (0, start),
                                     audioBuffer.getReadPointer (juce::jmin (1, channels - 1), start) };
            writer.write (ptrs, size);
        };

        writeRegion (start1, size1);
        writeRegion (start2, size2);

        const auto done = size1 + size2;
        audioFifo.finishedRead (done);
        framesRead += done;
        frames -= done;
        framesWritten.store (framesRead);

        if (done == 0)
            break;
    }
}

void CaptureEngine::handle (const Marker& marker)
{
    if (marker.kind == Marker::Kind::start)
    {
        const auto file = takeFolder.getChildFile (partialCaptureName (instanceTag, ++takeNumber));
        writer.open (file, sampleRate, channels);
        return;
    }

    if (! writer.isOpen())
        return;

    FinishedTakeFile take;
    take.file = writer.file();
    take.sampleRate = writer.sampleRate();
    take.channels = writer.numChannels();
    take.summary.seconds = writer.close();
    take.summary.end = marker.end;
    take.summary.manual = marker.manual;
    take.summary.peak = marker.peak;
    take.summary.overflowed = marker.overflowed;

    if (onTakeFinished)
        onTakeFinished (take);
}

bool CaptureEngine::waitUntilDrained (int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (juce::Time::getMillisecondCounter() < deadline)
    {
        if (markerFifo.getNumReady() == 0 && framesWritten.load() >= framesPushedShared.load())
            return true;
        notify();
        juce::Thread::sleep (5);
    }
    return false;
}

} // namespace dpm
