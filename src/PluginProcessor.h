#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>

/**
 * PR-01 scaffold: sine instrument stub so the plugin loads and makes sound.
 * Real Fringe FDTD + detector voices arrive in later PRs (see ultraplan).
 */
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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;

    double sampleRate_ { 44100.0 };
    double phase_ { 0.0 };
    double phaseInc_ { 0.0 };
    float currentAmp_ { 0.0f };
    float targetAmp_ { 0.0f };
    int activeNote_ { -1 };

    void handleMidi (const juce::MidiBuffer& midi);
    void noteOn (int note, float velocity);
    void noteOff (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessor)
};
