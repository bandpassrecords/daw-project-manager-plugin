#include "TestHelpers.h"

#include "core/CaptureWriter.h"

using namespace dpm;

namespace
{
    std::unique_ptr<juce::AudioFormatReader> readFlac (const juce::File& f)
    {
        juce::FlacAudioFormat flac;
        return std::unique_ptr<juce::AudioFormatReader> (flac.createReaderFor (f.createInputStream().release(), true));
    }
}

TEST_CASE ("A take is written as a 24-bit FLAC that reads back the same", "[writer]")
{
    test::TempDir tmp;
    const auto file = tmp.dir.getChildFile ("take.flac");

    constexpr double rate = 48000.0;
    juce::AudioBuffer<float> sine (2, 48000);
    for (int i = 0; i < sine.getNumSamples(); ++i)
    {
        const auto v = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / (float) rate);
        sine.setSample (0, i, v);
        sine.setSample (1, i, -v);
    }

    CaptureWriter writer;
    REQUIRE (writer.open (file, rate, 2));
    for (int pos = 0; pos < sine.getNumSamples(); pos += 4800)
    {
        const float* ptrs[] = { sine.getReadPointer (0, pos), sine.getReadPointer (1, pos) };
        REQUIRE (writer.write (ptrs, 4800));
    }
    CHECK (writer.close() == 1.0);
    CHECK_FALSE (writer.isOpen());

    auto reader = readFlac (file);
    REQUIRE (reader != nullptr);
    CHECK (reader->sampleRate == rate);
    CHECK (reader->numChannels == 2);
    CHECK (reader->bitsPerSample == 24);
    CHECK (reader->lengthInSamples == 48000);

    juce::AudioBuffer<float> back (2, 48000);
    reader->read (&back, 0, 48000, 0, true, true);
    for (int i = 0; i < 48000; i += 997)
    {
        CHECK (std::abs (back.getSample (0, i) - sine.getSample (0, i)) < 1.0e-5f);
        CHECK (std::abs (back.getSample (1, i) - sine.getSample (1, i)) < 1.0e-5f);
    }
}

TEST_CASE ("Abandoning a take deletes its file", "[writer]")
{
    test::TempDir tmp;
    const auto file = tmp.dir.getChildFile ("sub/take.flac");

    CaptureWriter writer;
    REQUIRE (writer.open (file, 44100.0, 1));
    CHECK (file.existsAsFile());
    writer.abandon();
    CHECK_FALSE (file.exists());
    CHECK_FALSE (writer.isOpen());
}

TEST_CASE ("A writer that isn't open writes nothing and closes to zero", "[writer]")
{
    CaptureWriter writer;
    const float* none[] = { nullptr };
    CHECK_FALSE (writer.write (none, 10));
    CHECK (writer.close() == 0.0);
}
