#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float twoPi = 6.28318530717958647692f;

float midiNoteToHz (int note)
{
    return 440.0f * std::pow (2.0f, (static_cast<float> (note) - 69.0f) / 12.0f);
}
} // namespace

FringeAudioProcessor::FringeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FringeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 },
        "Volume",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.3f));

    // Placeholder knobs matching ultraplan IDs — wired later
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "freq", 1 },
        "Freq",
        juce::NormalisableRange<float> { 15.0f, 100.0f, 0.1f },
        35.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "filter", 1 },
        "Filter",
        juce::NormalisableRange<float> { 200.0f, 12000.0f, 1.0f, 0.3f },
        7000.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "midiMode", 1 },
        "MIDI Mode",
        juce::StringArray { "Parity", "Enhanced" },
        0));

    return { params.begin(), params.end() };
}

void FringeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0;
    currentAmp_ = 0.0f;
    targetAmp_ = 0.0f;
}

void FringeAudioProcessor::releaseResources() {}

bool FringeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void FringeAudioProcessor::noteOn (int note, float velocity)
{
    activeNote_ = note;
    const float hz = midiNoteToHz (note);
    phaseInc_ = static_cast<double> (hz) / sampleRate_;
    targetAmp_ = juce::jlimit (0.0f, 1.0f, velocity);
}

void FringeAudioProcessor::noteOff (int note)
{
    if (note == activeNote_ || note < 0)
    {
        activeNote_ = -1;
        targetAmp_ = 0.0f;
    }
}

void FringeAudioProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            // PR-01 Enhanced-style hold for audible testing.
            // Ultraplan Parity (10 ms pulse) lands with FringeEngine (PR-08).
            noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            noteOff (msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            noteOff (-1);
        }
    }
}

void FringeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    handleMidi (midi);
    midi.clear();

    const auto* volParam = apvts.getRawParameterValue ("volume");
    const float volume = volParam != nullptr ? volParam->load() : 0.3f;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    auto* left = buffer.getWritePointer (0);
    float* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    constexpr float ampSmooth = 0.002f;

    for (int i = 0; i < numSamples; ++i)
    {
        currentAmp_ += (targetAmp_ - currentAmp_) * ampSmooth;
        const float s = std::sin (static_cast<float> (phase_ * twoPi)) * currentAmp_ * volume * 0.25f;
        left[i] = s;
        if (right != nullptr)
            right[i] = s;

        phase_ += phaseInc_;
        if (phase_ >= 1.0)
            phase_ -= 1.0;
    }
}

juce::AudioProcessorEditor* FringeAudioProcessor::createEditor()
{
    return new FringeAudioProcessorEditor (*this);
}

void FringeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FringeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FringeAudioProcessor();
}
