#include "PluginProcessor.h"

#include "Translations.h"
#include "core/CaptureNaming.h"
#include "ui/PluginEditor.h"

namespace dpm
{

juce::File PluginProcessor::takeFolder()
{
    // Takes are written outside the bridge folder and moved in only when
    // kept, so the app never sees one being recorded.
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DAW Project Manager");
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      client (std::make_unique<BridgeClient> (BridgeFolder::forThisMachine())),
      clientRef (client.get()),
      engine (takeFolder(), juce::Uuid().toString(), [weak = clientRef] (const FinishedTakeFile& take)
      {
          // Writer thread: hand over to the message thread.
          juce::MessageManager::callAsync ([weak, take]
          {
              if (auto* c = weak.get())
                  c->takeFinished (take);
              else
                  take.file.deleteFile();
          });
      })
{
    installTranslations();
    applyCaptureSettings();

    // Takes a crashed or killed DAW left half-written: nothing else ever
    // removes them from the temp folder.
    deleteStaleTakes (takeFolder(), juce::Time::getCurrentTime());
}

PluginProcessor::~PluginProcessor()
{
    engine.release();
}

void PluginProcessor::applyCaptureSettings()
{
    const auto settings = client->withSession ([] (BridgeSession& s) { return s.state().capture; });
    engine.setMode (settings.mode);
    engine.setMaxSeconds (settings.maxSeconds);
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumInputChannels());
}

void PluginProcessor::releaseResources()
{
    engine.release();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Pass-through: the input is the output. Only channels the host gave us
    // no input for are cleared.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    TransportState transport;
    transport.isRealtime = ! isNonRealtime();

    if (isStandalone())
    {
        // No DAW around the Standalone: its test panel is the transport, and
        // the tone stands in for the mix (JUCE mutes the Standalone's input).
        testTransport.process (buffer, getSampleRate());
        transport.isPlaying = testTransport.isPlaying();
        transport.isLooping = testTransport.isLooping();
        playhead.store (testTransport.positionSeconds (getSampleRate()));
    }
    else if (auto* head = getPlayHead())
    {
        if (auto position = head->getPosition())
        {
            transport.isPlaying = position->getIsPlaying();
            transport.isLooping = position->getIsLooping();
            if (auto seconds = position->getTimeInSeconds())
                playhead.store (*seconds);
        }
    }

    engine.process (buffer, transport);
}

std::optional<double> PluginProcessor::playheadSeconds() const
{
    const auto p = playhead.load();
    return p >= 0 ? std::optional (p) : std::nullopt;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = client->saveState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        client->restoreState (juce::ValueTree::fromXml (*xml));
        applyCaptureSettings();
    }
}

} // namespace dpm

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new dpm::PluginProcessor();
}
