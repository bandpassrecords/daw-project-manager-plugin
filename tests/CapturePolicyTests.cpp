#include "core/CapturePolicy.h"

#include <catch2/catch_test_macros.hpp>

using namespace dpm;

namespace
{
    constexpr double rate = 1000.0;     // 1000 samples a second keeps the numbers readable
    constexpr int block = 100;          // a tenth of a second
    constexpr float loud = 0.5f, quiet = 0.0f;

    struct Run
    {
        CapturePolicy policy;
        CaptureMode mode = CaptureMode::automatic;
        int maxSeconds = 60;

        Run() { policy.reset (rate); }

        CaptureStep step (bool playing, float peak, bool looping = false, bool realtime = true,
                          bool manualStart = false, bool manualStop = false)
        {
            CaptureBlock b;
            b.isPlaying = playing;
            b.isLooping = looping;
            b.isRealtime = realtime;
            b.peak = peak;
            b.numSamples = block;
            b.mode = mode;
            b.maxSamples = (juce::int64) (maxSeconds * rate);
            b.manualStart = manualStart;
            b.manualStop = manualStop;
            return policy.process (b);
        }

        /** Plays `seconds` of blocks at `peak`; returns the last step that ended a take, if any. */
        TakeEnd play (double seconds, float peak, juce::int64* written = nullptr)
        {
            for (int i = 0; i < (int) (seconds * rate / block); ++i)
            {
                const auto s = step (true, peak);
                if (written != nullptr)
                    *written += s.samplesToWrite;
                if (s.end != TakeEnd::none)
                    return s.end;
            }
            return TakeEnd::none;
        }
    };
}

TEST_CASE ("A take starts on the first audible block of playback, not before", "[policy]")
{
    Run r;
    CHECK_FALSE (r.step (false, loud).startTake);     // stopped
    CHECK_FALSE (r.step (true, quiet).startTake);     // playing silence (count-in, empty bars)

    const auto s = r.step (true, loud);
    CHECK (s.startTake);
    CHECK (s.samplesToWrite == block);
    CHECK (r.policy.isRecording());
}

TEST_CASE ("Stopping the transport ends the take", "[policy]")
{
    Run r;
    r.step (true, loud);
    r.play (2.0, loud);
    const auto s = r.step (false, loud);
    CHECK (s.end == TakeEnd::stopped);
    CHECK (s.samplesToWrite == 0);
    CHECK_FALSE (r.policy.isRecording());
}

TEST_CASE ("A take stops at the chosen length, exactly", "[policy]")
{
    Run r;
    r.maxSeconds = 2;
    juce::int64 written = 0;
    CHECK (r.play (5.0, loud, &written) == TakeEnd::reachedMax);
    CHECK (written == 2000);

    // No new take until playback stops and starts again.
    CHECK_FALSE (r.step (true, loud).startTake);
    r.step (false, quiet);
    CHECK (r.step (true, loud).startTake);
}

TEST_CASE ("The last block is cut so the take is exactly the chosen length", "[policy]")
{
    Run r;
    r.maxSeconds = 1;
    CapturePolicy& p = r.policy;
    CaptureBlock b;
    b.isPlaying = true;
    b.peak = loud;
    b.numSamples = 384;     // doesn't divide 1000
    b.maxSamples = 1000;

    juce::int64 written = 0;
    TakeEnd end = TakeEnd::none;
    for (int i = 0; i < 10 && end == TakeEnd::none; ++i)
    {
        const auto s = p.process (b);
        written += s.samplesToWrite;
        end = s.end;
    }
    CHECK (end == TakeEnd::reachedMax);
    CHECK (written == 1000);
}

TEST_CASE ("Seconds of silence after the song end the take", "[policy]")
{
    Run r;
    r.play (20.0, loud);
    CHECK (r.play (silenceEndSeconds + 0.5, quiet) == TakeEnd::silence);
    CHECK_FALSE (r.policy.isRecording());
}

TEST_CASE ("A quiet passage shorter than the silence limit doesn't end the take", "[policy]")
{
    Run r;
    r.play (10.0, loud);
    CHECK (r.play (silenceEndSeconds - 1.0, quiet) == TakeEnd::none);
    CHECK (r.play (5.0, loud) == TakeEnd::none);
    CHECK (r.policy.isRecording());
}

TEST_CASE ("Cycling a loop ends the take as looped and starts no new one", "[policy]")
{
    Run r;
    r.play (5.0, loud);
    CHECK (r.step (true, loud, true).end == TakeEnd::looped);
    CHECK_FALSE (r.step (true, loud, false).startTake);   // still the same pass

    r.step (false, quiet);
    CHECK_FALSE (r.step (true, loud, true).startTake);    // loop on from the start
}

TEST_CASE ("Offline renders are not captured", "[policy]")
{
    Run r;
    r.play (3.0, loud);
    CHECK (r.step (true, loud, false, false).end == TakeEnd::offline);
    CHECK_FALSE (r.step (true, loud, false, false).startTake);
    CHECK_FALSE (r.step (true, loud).startTake);
    r.step (false, quiet);
    CHECK (r.step (true, loud).startTake);
}

TEST_CASE ("Manual mode records from Record to Stop, transport or not", "[policy]")
{
    Run r;
    r.mode = CaptureMode::manual;
    CHECK_FALSE (r.step (true, loud).startTake);          // playing alone doesn't start it

    const auto start = r.step (false, quiet, false, true, true);
    CHECK (start.startTake);
    CHECK (start.samplesToWrite == block);
    CHECK (r.policy.isManualTake());

    CHECK (r.step (false, quiet).samplesToWrite == block); // keeps going while stopped
    const auto stop = r.step (true, loud, false, true, false, true);
    CHECK (stop.end == TakeEnd::manualStop);
    CHECK (stop.samplesToWrite == 0);
    CHECK_FALSE (r.policy.isRecording());
}

TEST_CASE ("Manual takes stop at the chosen length too", "[policy]")
{
    Run r;
    r.mode = CaptureMode::manual;
    r.maxSeconds = 1;
    r.step (true, loud, false, true, true);
    TakeEnd end = TakeEnd::none;
    for (int i = 0; i < 20 && end == TakeEnd::none; ++i)
        end = r.step (true, loud).end;
    CHECK (end == TakeEnd::reachedMax);
    CHECK (r.step (true, loud, false, true, true).startTake);  // and may start again
}

TEST_CASE ("Changing mode mid-take ends it; Off records nothing", "[policy]")
{
    Run r;
    r.play (3.0, loud);
    r.mode = CaptureMode::off;
    CHECK (r.step (true, loud).end == TakeEnd::modeChanged);
    CHECK (r.play (5.0, loud) == TakeEnd::none);
    CHECK_FALSE (r.policy.isRecording());

    r.mode = CaptureMode::automatic;
    CHECK (r.step (true, loud).startTake);
}

// --- judgeTake ------------------------------------------------------------------

TEST_CASE ("Looped, offline and interrupted takes are thrown away", "[judge]")
{
    for (auto end : { TakeEnd::looped, TakeEnd::offline, TakeEnd::modeChanged })
        CHECK (judgeTake ({ 60.0, end, false, loud }, {}) == TakeVerdict::discardEnded);
}

TEST_CASE ("A take that lost audio is thrown away", "[judge]")
{
    CHECK (judgeTake ({ 60.0, TakeEnd::reachedMax, false, loud, true }, {}) == TakeVerdict::discardIncomplete);
}

TEST_CASE ("Short and silent takes are thrown away", "[judge]")
{
    CHECK (judgeTake ({ minAutoSeconds - 1, TakeEnd::stopped, false, loud }, {}) == TakeVerdict::discardTooShort);
    CHECK (judgeTake ({ minManualSeconds - 1, TakeEnd::manualStop, true, loud }, {}) == TakeVerdict::discardTooShort);
    CHECK (judgeTake ({ 60.0, TakeEnd::stopped, false, 0.0001f }, {}) == TakeVerdict::discardSilent);
}

TEST_CASE ("The first good take is kept", "[judge]")
{
    CHECK (judgeTake ({ 12.0, TakeEnd::stopped, false, loud }, {}) == TakeVerdict::keep);
}

TEST_CASE ("A shorter automatic take doesn't replace a longer capture", "[judge]")
{
    const LastCapture last { "captures/x.flac", 58.0, false, {} };
    CHECK (judgeTake ({ 20.0, TakeEnd::stopped, false, loud }, last) == TakeVerdict::discardNotBetter);
    CHECK (judgeTake ({ 57.6, TakeEnd::stopped, false, loud }, last) == TakeVerdict::keep);   // within slack
    CHECK (judgeTake ({ 70.0, TakeEnd::silence, false, loud }, last) == TakeVerdict::keep);
}

TEST_CASE ("A take that filled the length always replaces, as does a manual one", "[judge]")
{
    const LastCapture last { "captures/x.flac", 120.0, true, {} };
    CHECK (judgeTake ({ 30.0, TakeEnd::reachedMax, false, loud }, last) == TakeVerdict::keep);
    CHECK (judgeTake ({ 5.0, TakeEnd::manualStop, true, loud }, last) == TakeVerdict::keep);
}
