#include "TestTransport.h"

namespace dpm
{

void TestTransport::setPlaying (bool shouldPlay)
{
    playing.store (shouldPlay);
}

double TestTransport::positionSeconds (double sampleRate) const
{
    return sampleRate > 0 ? (double) positionSamples.load() / sampleRate : 0.0;
}

void TestTransport::process (juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (! playing.load() || sampleRate <= 0)
        return;

    const auto numSamples = buffer.getNumSamples();
    positionSamples.fetch_add (numSamples);

    if (! toneOn.load())
        return;

    const auto step = juce::MathConstants<double>::twoPi * toneHz / sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto v = toneGain * (float) std::sin (phase);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, v);
        phase += step;
        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }
}

} // namespace dpm
