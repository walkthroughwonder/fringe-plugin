#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class FringeAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FringeAudioProcessorEditor (FringeAudioProcessor&);
    ~FringeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FringeAudioProcessor& processor;

    juce::Slider volumeSlider;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label hintLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> volumeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FringeAudioProcessorEditor)
};
