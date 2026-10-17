#include "TestHelpers.h"

#include "core/CaptureEngine.h"

#include <mutex>

using namespace dpm;

namespace
{
    constexpr double rate = 8000.0;
    constexpr int blockSize = 400;  // 50 ms

    /** Collects finished takes from the writer thread. */
    struct Takes
    {
        std::mutex mutex;
        std::vector<FinishedTakeFile> list;
        juce::WaitableEvent arrived;

        CaptureEngine::TakeCallback callback()
        {
            return [this] (const FinishedTakeFile& t)
            {
                {
                    const std::lock_guard<std::mutex> lock (mutex);
                    list.push_back (t);
                }
                arrived.signal();
            };
        }

        bool waitFor (size_t count)
        {
            for (int i = 0; i < 200; ++i)
            {
                {
                    const std::lock_guard<std::mutex> lock (mutex);
                    if (list.size() >= count)
                        return true;
                }
                arrived.wait (25);
            }
            return false;
        }
    };

    juce::AudioBuffer<float> blockOf (float value, int channels = 2)
    {
        juce::AudioBuffer<float> b (channels, blockSize);
        for (int ch = 0; ch < channels; ++ch)
            juce::FloatVectorOperations::fill (b.getWritePointer (ch), value, blockSize);
        return b;
    }

    std::unique_ptr<juce::AudioFormatReader> readFlac (const juce::File& f)
    {
        juce::FlacAudioFormat flac;
        return std::unique_ptr<juce::AudioFormatReader> (flac.createReaderFor (f.createInputStream().release(), true));
    }
}

TEST_CASE ("A playback pass becomes one FLAC take with every sample in order", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    CaptureEngine engine (tmp.dir, "test", takes.callback());
    engine.prepare (rate, 2);

    // Silence while playing, then 2 s of a rising ramp of distinct values, then stop.
    engine.process (blockOf (0.0f), { true, false, true });
    const int blocks = (int) (2.0 * rate / blockSize);
    for (int i = 0; i < blocks; ++i)
        engine.process (blockOf (0.1f + 0.8f * (float) i / (float) blocks), { true, false, true });

    CHECK (engine.isRecording());
    CHECK (engine.recordedSeconds() == Catch::Approx (2.0));
    engine.process (blockOf (0.5f), { false, false, true });

    REQUIRE (takes.waitFor (1));
    const auto& take = takes.list[0];
    CHECK (take.summary.end == TakeEnd::stopped);
    CHECK_FALSE (take.summary.manual);
    CHECK_FALSE (take.summary.overflowed);
    CHECK (take.summary.seconds == Catch::Approx (2.0));
    CHECK (take.summary.peak == Catch::Approx (0.1f + 0.8f * (float) (blocks - 1) / (float) blocks));
    CHECK (take.sampleRate == rate);
    CHECK (take.channels == 2);
    CHECK (take.file.getParentDirectory() == tmp.dir);

    auto reader = readFlac (take.file);
    REQUIRE (reader != nullptr);
    REQUIRE (reader->lengthInSamples == blocks * blockSize);

    juce::AudioBuffer<float> back (2, blocks * blockSize);
    reader->read (&back, 0, blocks * blockSize, 0, true, true);
    for (int i = 0; i < blocks; ++i)
        CHECK (back.getSample (0, i * blockSize) == Catch::Approx (0.1f + 0.8f * (float) i / (float) blocks).margin (1.0e-5));
}

TEST_CASE ("The engine never changes the audio passing through", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    CaptureEngine engine (tmp.dir, "test", takes.callback());
    engine.prepare (rate, 2);

    auto buffer = blockOf (0.25f);
    const auto copy = buffer;
    engine.process (buffer, { true, false, true });

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            REQUIRE (buffer.getSample (ch, i) == copy.getSample (ch, i));
}

TEST_CASE ("A mono input makes a mono take", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    CaptureEngine engine (tmp.dir, "test", takes.callback());
    engine.prepare (rate, 1);
    engine.setMode (CaptureMode::manual);
    engine.pressRecord();

    for (int i = 0; i < 20; ++i)
        engine.process (blockOf (0.3f, 1), { false, false, true });
    engine.pressStop();
    engine.process (blockOf (0.3f, 1), { false, false, true });

    REQUIRE (takes.waitFor (1));
    CHECK (takes.list[0].summary.manual);
    CHECK (takes.list[0].summary.end == TakeEnd::manualStop);

    auto reader = readFlac (takes.list[0].file);
    REQUIRE (reader != nullptr);
    CHECK (reader->numChannels == 1);
}

TEST_CASE ("Consecutive passes become separate takes", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    CaptureEngine engine (tmp.dir, "test", takes.callback());
    engine.prepare (rate, 2);

    for (int pass = 0; pass < 3; ++pass)
    {
        for (int i = 0; i < 10; ++i)
            engine.process (blockOf (0.4f), { true, false, true });
        engine.process (blockOf (0.0f), { false, false, true });
    }

    REQUIRE (takes.waitFor (3));
    CHECK (takes.list[0].file != takes.list[1].file);
    CHECK (takes.list[1].file != takes.list[2].file);
    for (auto& t : takes.list)
        CHECK (t.summary.seconds == Catch::Approx (0.5));
}

TEST_CASE ("The meter reports the loudest sample once", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    CaptureEngine engine (tmp.dir, "test", takes.callback());
    engine.prepare (rate, 2);
    engine.setMode (CaptureMode::off);

    engine.process (blockOf (0.2f), { true, false, true });
    engine.process (blockOf (0.7f), { true, false, true });
    engine.process (blockOf (0.1f), { true, false, true });
    CHECK (engine.takeMeterPeak() == Catch::Approx (0.7f));
    CHECK (engine.takeMeterPeak() == 0.0f);
    CHECK_FALSE (engine.isRecording());
}

TEST_CASE ("A take still open when playback is torn down is deleted, not handed over", "[engine]")
{
    test::TempDir tmp;
    Takes takes;
    {
        CaptureEngine engine (tmp.dir, "test", takes.callback());
        engine.prepare (rate, 2);
        for (int i = 0; i < 10; ++i)
            engine.process (blockOf (0.4f), { true, false, true });
        REQUIRE (engine.waitUntilDrained (2000));
        engine.release();
    }

    CHECK (takes.list.empty());
    CHECK (tmp.dir.findChildFiles (juce::File::findFiles, true).isEmpty());
}
