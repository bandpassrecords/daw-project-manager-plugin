#pragma once

#include "PluginState.h"

namespace dpm
{

/** Why a take ended. */
enum class TakeEnd
{
    none,
    stopped,      // transport stopped
    reachedMax,   // the chosen length is full
    silence,      // the song ended and playback ran on in silence
    looped,       // the host was cycling a loop: not a pre-listen of the song
    offline,      // an offline render (the app already picks up bounces)
    manualStop,
    modeChanged
};

/** What the audio thread knows about one block. */
struct CaptureBlock
{
    bool isPlaying = false;
    bool isLooping = false;
    bool isRealtime = true;
    float peak = 0.0f;          // linear, loudest sample of the block
    int numSamples = 0;
    CaptureMode mode = CaptureMode::automatic;
    juce::int64 maxSamples = 0;
    bool manualStart = false;   // the Record button was pressed since the last block
    bool manualStop = false;
};

/** What to do with that block. A take that starts writes this block's
    samples; a take that ends writes none of them.
*/
struct CaptureStep
{
    bool startTake = false;
    int samplesToWrite = 0;
    TakeEnd end = TakeEnd::none;
};

/** Decides, block by block, when a take starts and ends. Runs on the audio
    thread: no allocation, no locks, everything it needs passed in.

    Automatic mode records each playback pass from its first audible block,
    up to the chosen length. A pass that cycles a loop, renders offline, or
    stops short is left for judgeTake() to throw away.
*/
class CapturePolicy
{
public:
    void reset (double sampleRate);
    CaptureStep process (const CaptureBlock& block);

    bool isRecording() const { return state == State::recording; }
    bool isManualTake() const { return manualTake; }
    juce::int64 samplesRecorded() const { return recorded; }

private:
    enum class State
    {
        idle,
        waitingForSound,
        recording,
        waitingForStop    // a take ended mid-pass: no new one until playback stops
    };

    CaptureStep startTake (const CaptureBlock& block, bool manual);
    CaptureStep endTake (TakeEnd why, State next);
    int samplesFor (const CaptureBlock& block) const;

    State state = State::idle;
    CaptureMode lastMode = CaptureMode::automatic;
    bool manualTake = false;
    juce::int64 recorded = 0;
    juce::int64 silentRun = 0;
    double sampleRate = 44100.0;
};

/** Audible means louder than -50 dBFS. */
inline constexpr float audibleThreshold = 0.0031623f;

/** Seconds of silence after which an automatic take ends. */
inline constexpr double silenceEndSeconds = 4.0;

/** Shortest automatic take worth keeping, and shortest manual one. */
inline constexpr double minAutoSeconds = 10.0;
inline constexpr double minManualSeconds = 3.0;

struct TakeSummary
{
    double seconds = 0;
    TakeEnd end = TakeEnd::none;
    bool manual = false;
    float peak = 0.0f;
    bool overflowed = false;    // the writer fell behind and lost audio
};

enum class TakeVerdict
{
    keep,
    discardEnded,       // looped, offline, mode changed
    discardIncomplete,  // audio was lost
    discardTooShort,
    discardSilent,
    discardNotBetter    // shorter than the capture it would replace
};

/** Whether a finished take becomes the project's pre-listen capture.

    A manual take always replaces the last one. An automatic take replaces it
    when it filled the chosen length or is at least as long: working on the
    intro for twenty seconds shouldn't throw away a full minute of the song.
*/
TakeVerdict judgeTake (const TakeSummary& take, const LastCapture& last);

} // namespace dpm
