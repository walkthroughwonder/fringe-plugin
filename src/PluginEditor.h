#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "core/FringeTypes.h"

class FringeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit FringeAudioProcessorEditor (FringeAudioProcessor&);
    ~FringeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void paintWaveField (juce::Graphics& g, juce::Rectangle<int> area);
    juce::Colour amplitudeColour (float amp, float uvX) const;

    FringeAudioProcessor& audioProcessor;
    fringe::FieldSnapshot snapshot_;

    juce::ComboBox presetBox;
    juce::TextButton gateButton { "SOURCE" };
    juce::TextButton scaleButton { "SCALE" };
    juce::TextButton droneButton { "DRONE" };

    juce::Slider volumeSlider, speedSlider, freqSlider, slitSlider, sensSlider, filterSlider, reverbSlider;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> volumeAtt, speedAtt, freqAtt, slitAtt, sensAtt, filterAtt, reverbAtt;
    std::unique_ptr<CA> presetAtt;
    std::unique_ptr<BA> gateAtt, scaleAtt, droneAtt;

    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessorEditor)
};
