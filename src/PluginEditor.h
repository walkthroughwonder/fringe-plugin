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
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void paintWaveField (juce::Graphics& g, juce::Rectangle<int> area);
    void paintDetectorGraph (juce::Graphics& g, juce::Rectangle<int> area);
    juce::Colour amplitudeColour (float amp, float uvX) const;
    void paintAtEvent (const juce::MouseEvent& e);
    juce::Rectangle<int> fieldBounds() const;

    FringeAudioProcessor& audioProcessor;
    fringe::FieldSnapshot snapshot_;
    juce::Rectangle<int> fieldArea_;
    bool drawing_ = false;
    bool eraser_ = false;

    juce::ComboBox presetBox;
    juce::TextButton gateButton { "SOURCE" };
    juce::TextButton scaleButton { "SCALE" };
    juce::TextButton droneButton { "DRONE" };
    juce::TextButton clearDrawButton { "CLEAR" };
    juce::TextButton eraserButton { "ERASE" };

    juce::Slider volumeSlider, speedSlider, freqSlider, slitSlider, slitWSlider,
        sensSlider, filterSlider, reverbSlider;
    juce::Slider lfo1Rate, lfo1Depth, lfo2Rate, lfo2Depth, lfo3Rate, lfo3Depth;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> volumeAtt, speedAtt, freqAtt, slitAtt, slitWAtt, sensAtt, filterAtt, reverbAtt;
    std::unique_ptr<SA> lfo1RateAtt, lfo1DepthAtt, lfo2RateAtt, lfo2DepthAtt, lfo3RateAtt, lfo3DepthAtt;
    std::unique_ptr<CA> presetAtt;
    std::unique_ptr<BA> gateAtt, scaleAtt, droneAtt;

    juce::Label titleLabel, hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessorEditor)
};
