#include "TestHelpers.h"

#include "core/CaptureEngine.h"
#include "core/TestTransport.h"

using namespace dpm;

namespace
{
    juce::AudioBuffer<float> silence (int channels = 2, int samples = 480)
    {
        juce::AudioBuffer<float> b (channels, samples);
        b.clear();
        return b;
    }
}

TEST_CASE ("The test transport moves only while playing", "[test-transport]")
{
    TestTransport t;
    auto block = silence();

    t.process (block, 48000.0);
    CHECK (t.positionSeconds (48000.0) == 0.0);

    t.setPlaying (true);
    for (int i = 0; i < 100; ++i)
        t.process (block, 48000.0);
    CHECK (t.positionSeconds (48000.0) == Catch::Approx (1.0));

    t.setPlaying (false);
    t.process (block, 48000.0);
    CHECK (t.positionSeconds (48000.0) == Catch::Approx (1.0));

    t.rewind();
    CHECK (t.positionSeconds (48000.0) == 0.0);
}

TEST_CASE ("The test tone sounds while playing, at -18 dBFS, on every channel", "[test-transport]")
{
    TestTransport t;
    t.setPlaying (true);
    auto block = silence (2, 4800);
    t.process (block, 48000.0);

    CHECK (block.getMagnitude (0, 0, 4800) == Catch::Approx (TestTransport::toneGain).margin (0.001));
    CHECK (block.getMagnitude (1, 0, 4800) == Catch::Approx (TestTransport::toneGain).margin (0.001));
    CHECK (block.getSample (0, 1234) == block.getSample (1, 1234));
    CHECK (juce::Decibels::gainToDecibels (TestTransport::toneGain) == Catch::Approx (-18.06).margin (0.01));
}

TEST_CASE ("No tone when stopped or switched off", "[test-transport]")
{
    TestTransport t;
    auto stopped = silence();
    t.process (stopped, 48000.0);
    CHECK (stopped.getMagnitude (0, 0, stopped.getNumSamples()) == 0.0f);

    t.setPlaying (true);
    t.setToneOn (false);
    auto quiet = silence();
    t.process (quiet, 48000.0);
    CHECK (quiet.getMagnitude (0, 0, quiet.getNumSamples()) == 0.0f);
}

TEST_CASE ("The tone is continuous across blocks", "[test-transport]")
{
    // A click at every block boundary would make captures of it useless to
    // listen to; the phase carries over.
    TestTransport t;
    t.setPlaying (true);
    auto a = silence (1, 333);
    auto b = silence (1, 333);
    t.process (a, 48000.0);
    t.process (b, 48000.0);

    const auto step = juce::MathConstants<double>::twoPi * TestTransport::toneHz / 48000.0;
    const auto expected = TestTransport::toneGain * (float) std::sin (step * 333);
    CHECK (b.getSample (0, 0) == Catch::Approx (expected).margin (1.0e-5));
}

TEST_CASE ("Playing the test transport with the tone makes a capture take", "[test-transport]")
{
    // The Standalone's whole point: a take without a DAW.
    test::TempDir tmp;
    std::vector<FinishedTakeFile> takes;
    std::mutex m;
    CaptureEngine engine (tmp.dir, "standalone", [&] (const FinishedTakeFile& f)
    {
        const std::lock_guard<std::mutex> lock (m);
        takes.push_back (f);
    });
    engine.prepare (8000.0, 2);

    TestTransport t;
    t.setPlaying (true);
    for (int i = 0; i < 20; ++i)
    {
        auto block = silence (2, 400);
        t.process (block, 8000.0);
        engine.process (block, { t.isPlaying(), t.isLooping(), true });
    }
    t.setPlaying (false);
    auto block = silence (2, 400);
    engine.process (block, { t.isPlaying(), t.isLooping(), true });

    REQUIRE (engine.waitUntilDrained (5000));
    for (int i = 0; i < 100 && takes.empty(); ++i)
        juce::Thread::sleep (10);

    const std::lock_guard<std::mutex> lock (m);
    REQUIRE (takes.size() == 1);
    CHECK (takes[0].summary.seconds == Catch::Approx (1.0));
    CHECK (takes[0].summary.end == TakeEnd::stopped);
    CHECK (takes[0].summary.peak == Catch::Approx (TestTransport::toneGain).margin (0.001));
}
