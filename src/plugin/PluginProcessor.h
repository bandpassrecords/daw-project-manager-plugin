#pragma once

#include "BridgeClient.h"
#include "core/CaptureEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <optional>

namespace dpm
{

/** The plugin. Audio passes through untouched; a copy is captured for the
    app's pre-listen, and the editor shows the linked project's to-dos.
*/
class CompanionProcessor : public juce::AudioProcessor
{
public:
    CompanionProcessor();
    ~CompanionProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    BridgeClient& bridge() { return *client; }
    CaptureEngine& capture() { return engine; }

    /** Pushes the session's capture settings to the audio side. */
    void applyCaptureSettings();

    /** Where the host's playhead was at the last block, if it says. */
    std::optional<double> playheadSeconds() const;

private:
    static juce::File takeFolder();

    std::unique_ptr<BridgeClient> client;
    juce::WeakReference<BridgeClient> clientRef;
    CaptureEngine engine;   // after client: destroyed first, so no take outlives it
    std::atomic<double> playhead { -1.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionProcessor)
};

} // namespace dpm
