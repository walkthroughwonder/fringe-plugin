#pragma once

#include <JuceHeader.h>
#include "core/FringeEngine.h"

class FringeAudioProcessor final : public juce::AudioProcessor
{
public:
    FringeAudioProcessor();
    ~FringeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return static_cast<int> (fringe::Preset::Count); }
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    fringe::FringeEngine& getEngine() { return engine_; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void pushParamsToEngine();
    void handleMidi (juce::MidiBuffer& midi);

    juce::AudioProcessorValueTreeState apvts;
    fringe::FringeEngine engine_;
    int currentProgram_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessor)
};
