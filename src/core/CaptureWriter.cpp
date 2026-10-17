#include "CaptureWriter.h"

namespace dpm
{

CaptureWriter::~CaptureWriter()
{
    if (isOpen())
        abandon();
}

bool CaptureWriter::open (const juce::File& file, double sampleRate, int numChannels)
{
    if (isOpen())
        abandon();

    if (! file.getParentDirectory().createDirectory())
        return false;
    file.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::FlacAudioFormat flac;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (numChannels)
                             .withBitsPerSample (24);

    writer = flac.createWriterFor (stream, options);
    if (writer == nullptr)
    {
        stream.reset();
        file.deleteFile();
        return false;
    }

    target = file;
    rate = sampleRate;
    channels = numChannels;
    written = 0;
    return true;
}

bool CaptureWriter::write (const float* const* data, int numSamples)
{
    if (! isOpen() || numSamples <= 0)
        return isOpen();

    if (! writer->writeFromFloatArrays (data, channels, numSamples))
        return false;

    written += numSamples;
    return true;
}

double CaptureWriter::close()
{
    if (! isOpen())
        return 0;

    writer.reset();     // flushes and finalises the FLAC header
    return rate > 0 ? (double) written / rate : 0;
}

void CaptureWriter::abandon()
{
    writer.reset();
    target.deleteFile();
    written = 0;
}

} // namespace dpm
