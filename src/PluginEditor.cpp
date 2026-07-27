#include "PluginEditor.h"

namespace
{
const juce::Colour kBg { 0xff0a0a0f };
const juce::Colour kBone { 0xffd4c9a8 };
const juce::Colour kAccent { 0xffc8a44e };
} // namespace

FringeAudioProcessorEditor::FringeAudioProcessorEditor (FringeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (480, 280);
    setResizable (false, false);

    titleLabel.setText ("FRINGE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (28.0f).withStyle ("Bold"));
    titleLabel.setColour (juce::Label::textColourId, kBone);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("photonic synthesizer · scaffold", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, kAccent.withAlpha (0.85f));
    subtitleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitleLabel);

    hintLabel.setText ("PR-01 sine stub — play MIDI notes\nFDTD + optics sonification coming next",
                       juce::dontSendNotification);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setColour (juce::Label::textColourId, kBone.withAlpha (0.55f));
    hintLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (hintLabel);

    volumeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    volumeSlider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    volumeSlider.setColour (juce::Slider::thumbColourId, kBone);
    volumeSlider.setColour (juce::Slider::textBoxTextColourId, kBone);
    volumeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (volumeSlider);

    volumeAttachment = std::make_unique<Attachment> (processor.getAPVTS(), "volume", volumeSlider);
}

void FringeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kAccent.withAlpha (0.15f));
    g.drawRect (getLocalBounds().reduced (8), 1);
}

void FringeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    titleLabel.setBounds (r.removeFromTop (36));
    subtitleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    hintLabel.setBounds (r.removeFromTop (40));
    volumeSlider.setBounds (r.withSizeKeepingCentre (120, 120));
}
