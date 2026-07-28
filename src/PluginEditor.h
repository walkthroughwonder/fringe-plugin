#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "core/FringeTypes.h"

/** Ultra-wide cinematic Fringe editor (20:9). */
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
    void mouseMove (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void paintChrome (juce::Graphics& g);
    void paintWaveField (juce::Graphics& g, juce::Rectangle<int> area);
    void paintDetectorPanel (juce::Graphics& g, juce::Rectangle<int> area);
    void paintKnobRailLabels (juce::Graphics& g);
    juce::Colour amplitudeColour (float amp, float uvX, float energy) const;
    juce::Image renderFieldImage() const;
    void paintAtEvent (const juce::MouseEvent& e);
    void styleKnob (juce::Slider& s, const juce::String& name, juce::Colour accent);
    void styleToggle (juce::TextButton& b);

    FringeAudioProcessor& audioProcessor;
    fringe::FieldSnapshot snapshot_;
    juce::Image fieldCache_;
    bool fieldDirty_ = true;

    juce::Rectangle<int> fieldArea_, detectorArea_, topBar_, knobRail_, statusBar_;
    juce::ComponentBoundsConstrainer constrainer_;

    bool drawing_ = false;
    bool eraser_ = false;
    juce::Point<float> hoverUv_ { -1.0f, -1.0f };
    float animPhase_ = 0.0f;

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

    juce::Label brandLabel, tagLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessorEditor)
};
