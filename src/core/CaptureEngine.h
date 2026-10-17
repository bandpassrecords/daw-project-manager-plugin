#pragma once

#include "CapturePolicy.h"
#include "CaptureWriter.h"

#include <array>
#include <atomic>
#include <functional>

namespace dpm
{

/** A take that has been written to disk and is waiting to be judged. */
struct FinishedTakeFile
{
    juce::File file;
    TakeSummary summary;
    double sampleRate = 0;
    int channels = 0;
};

/** Host transport state for one block. */
struct TransportState
{
    bool isPlaying = false;
    bool isLooping = false;
    bool isRealtime = true;
};

/** Records takes without ever blocking the audio thread.

    The audio thread runs CapturePolicy and pushes samples into a lock-free
    FIFO, with take start/end markers in a second FIFO that say where in the
    sample stream they fall. A writer thread drains both and streams each take
    to a FLAC file in the take folder; when a take ends it is handed to the
    callback, on the writer thread, for the session to judge.
*/
class CaptureEngine : private juce::Thread
{
public:
    using TakeCallback = std::function<void (const FinishedTakeFile&)>;

    CaptureEngine (juce::File takeFolder, juce::String instanceTag, TakeCallback onTakeFinished);
    ~CaptureEngine() override;

    /** Allocates the FIFOs. Never concurrently with process(), which hosts
        guarantee for prepareToPlay/processBlock. A take in progress is lost.
    */
    void prepare (double sampleRate, int numChannels);
    void release();

    /** Audio thread. Reads the first two channels of `buffer`; never writes to it. */
    void process (const juce::AudioBuffer<float>& buffer, const TransportState& transport);

    void setMode (CaptureMode m)            { mode.store (m); }
    void setMaxSeconds (int seconds)        { maxSeconds.store (juce::jmax (1, seconds)); }
    void pressRecord()                      { manualStartRequested.store (true); }
    void pressStop()                        { manualStopRequested.store (true); }

    bool isRecording() const                { return recording.load(); }
    bool isManualTake() const               { return manualTakeActive.load(); }
    double recordedSeconds() const;

    /** The loudest sample since the last call, for the level meter. */
    float takeMeterPeak()                   { return meterPeak.exchange (0.0f); }

    /** Blocks until everything pushed so far has been written. For tests. */
    bool waitUntilDrained (int timeoutMs);

private:
    struct Marker
    {
        enum class Kind { start, end } kind = Kind::start;
        juce::int64 streamPos = 0;
        TakeEnd end = TakeEnd::none;
        bool manual = false;
        float peak = 0.0f;
        bool overflowed = false;
    };

    void run() override;
    void drain();
    void drainAudio (juce::int64 frames);
    void handle (const Marker& marker);
    bool pushMarker (const Marker& marker);
    void stopWriterThread();

    const juce::File takeFolder;
    const juce::String instanceTag;
    const TakeCallback onTakeFinished;

    // Audio thread only.
    CapturePolicy policy;
    juce::int64 framesPushed = 0;
    float takePeak = 0.0f;
    bool takeOverflowed = false;

    // Shared.
    juce::AbstractFifo audioFifo { 1 };
    juce::AudioBuffer<float> audioBuffer;
    juce::AbstractFifo markerFifo { (int) markerCapacity };
    static constexpr size_t markerCapacity = 64;
    std::array<Marker, markerCapacity> markers;

    std::atomic<CaptureMode> mode { CaptureMode::automatic };
    std::atomic<int> maxSeconds { 60 };
    std::atomic<bool> manualStartRequested { false }, manualStopRequested { false };
    std::atomic<bool> recording { false }, manualTakeActive { false };
    std::atomic<juce::int64> samplesRecorded { 0 };
    std::atomic<float> meterPeak { 0.0f };
    std::atomic<juce::int64> framesPushedShared { 0 };
    std::atomic<juce::int64> framesWritten { 0 };

    double sampleRate = 44100.0;
    int channels = 2;

    // Writer thread only.
    CaptureWriter writer;
    juce::int64 framesRead = 0;
    int takeNumber = 0;
};

} // namespace dpm
