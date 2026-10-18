#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>

namespace dpm
{

/** A pretend DAW for the Standalone build, which has no host around it:
    a transport to press Play on, a loop switch, and a test tone to record.
    Driven from the editor's test panel, read on the audio thread.

    Never used by the VST3/AU: there the host is the transport.
*/
class TestTransport
{
public:
    void setPlaying (bool shouldPlay);
    void setLooping (bool shouldLoop)   { looping.store (shouldLoop); }
    void setToneOn (bool shouldSound)   { toneOn.store (shouldSound); }
    void rewind()                       { positionSamples.store (0); }

    bool isPlaying() const              { return playing.load(); }
    bool isLooping() const              { return looping.load(); }
    bool isToneOn() const               { return toneOn.load(); }
    double positionSeconds (double sampleRate) const;

    /** Audio thread: moves the playhead on by one block when playing, and
        writes the test tone over the block when it's on. A stopped transport
        plays no tone: a DAW's master bus is quiet when stopped.
    */
    void process (juce::AudioBuffer<float>& buffer, double sampleRate);

    /** -18 dBFS: clearly audible, comfortably below clipping. */
    static constexpr float toneGain = 0.125f;
    static constexpr double toneHz = 220.0;

private:
    std::atomic<bool> playing { false }, looping { false }, toneOn { true };
    std::atomic<juce::int64> positionSamples { 0 };
    double phase = 0.0;     // audio thread only
};

} // namespace dpm
