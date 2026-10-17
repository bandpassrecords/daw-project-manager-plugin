#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace dpm
{

/** Streams one take to a 24-bit FLAC file. Lossless at about half the size
    of a WAV, and a format the app's player already reads.

    Not for the audio thread: it does file I/O. CaptureEngine calls it from
    its writer thread.
*/
class CaptureWriter
{
public:
    CaptureWriter() = default;
    ~CaptureWriter();

    bool open (const juce::File& file, double sampleRate, int numChannels);
    bool write (const float* const* channels, int numSamples);

    /** Finishes the file and returns how many seconds it holds. */
    double close();

    /** Closes and deletes the file. */
    void abandon();

    bool isOpen() const { return writer != nullptr; }
    const juce::File& file() const { return target; }
    int numChannels() const { return channels; }
    double sampleRate() const { return rate; }

private:
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::File target;
    double rate = 0;
    int channels = 0;
    juce::int64 written = 0;
};

} // namespace dpm
