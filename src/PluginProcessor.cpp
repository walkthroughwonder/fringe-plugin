#include "PluginProcessor.h"
#include "PluginEditor.h"

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
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "speed", 1 }, "Speed",
        juce::NormalisableRange<float> { 0.2f, 2.0f, 0.01f }, 0.9f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "freq", 1 }, "Freq",
        juce::NormalisableRange<float> { 15.0f, 100.0f, 0.1f }, 60.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "slit", 1 }, "Slit/Gap",
        juce::NormalisableRange<float> { 0.008f, 0.15f, 0.001f }, 0.03f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "slitW", 1 }, "Width",
        juce::NormalisableRange<float> { 0.004f, 0.06f, 0.001f }, 0.012f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sens", 1 }, "Sensitivity",
        juce::NormalisableRange<float> { 0.1f, 5.0f, 0.01f }, 1.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "filter", 1 }, "Filter",
        juce::NormalisableRange<float> { 200.0f, 12000.0f, 1.0f, 0.3f }, 7000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverb", 1 }, "Reverb",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release",
        juce::NormalisableRange<float> { 0.05f, 3.0f, 0.01f }, 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "preset", 1 }, "Preset",
        juce::StringArray { "Single Slit", "Double Slit", "Convex Lens", "Diffraction", "Open Field" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "midiMode", 1 }, "MIDI Mode",
        juce::StringArray { "Parity (pulse)", "Enhanced (hold)" },
        0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "scaleMode", 1 }, "Scale", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "droneMode", 1 }, "Drone", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gate", 1 }, "Source Gate", true));

    return { params.begin(), params.end() };
}

void FringeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    pushParamsToEngine();
}

void FringeAudioProcessor::releaseResources() {}

bool FringeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void FringeAudioProcessor::pushParamsToEngine()
{
    fringe::EngineParams p;
    auto load = [this] (const char* id, float def) -> float {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();
        return def;
    };

    p.volume = load ("volume", 0.35f);
    p.speed = load ("speed", 0.9f);
    p.freq = load ("freq", 60.0f);
    p.slit = load ("slit", 0.03f);
    p.slitW = load ("slitW", 0.012f);
    p.sensitivity = load ("sens", 1.5f);
    p.filterHz = load ("filter", 7000.0f);
    p.reverb = load ("reverb", 0.4f);
    p.release = load ("release", 0.5f);
    p.preset = static_cast<int> (load ("preset", 0.0f));
    p.midiMode = static_cast<int> (load ("midiMode", 0.0f));
    p.scaleMode = load ("scaleMode", 0.0f) > 0.5f;
    p.droneMode = load ("droneMode", 0.0f) > 0.5f;
    p.gate = load ("gate", 1.0f) > 0.5f;
    engine_.setParams (p);
}

void FringeAudioProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            engine_.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            // Mirror sim freq into APVTS for UI
            if (auto* p = apvts.getParameter ("freq"))
            {
                const float sim = fringe::midiNoteToSimFreq (msg.getNoteNumber());
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (sim));
                p->endChangeGesture();
            }
        }
        else if (msg.isNoteOff())
        {
            engine_.noteOff (msg.getNoteNumber());
        }
        else if (msg.isController())
        {
            if (msg.getControllerNumber() == 1) // mod wheel → freq
            {
                const float freq = 15.0f + msg.getControllerValue() / 127.0f * 85.0f;
                if (auto* p = apvts.getParameter ("freq"))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->convertTo0to1 (freq));
                    p->endChangeGesture();
                }
            }
            else if (msg.getControllerNumber() == 74) // filter
            {
                const float hz = 200.0f + msg.getControllerValue() / 127.0f * 11800.0f;
                if (auto* p = apvts.getParameter ("filter"))
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (p->convertTo0to1 (hz));
                    p->endChangeGesture();
                }
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            engine_.noteOff (-1);
        }
    }
}

void FringeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    pushParamsToEngine();
    handleMidi (midi);
    midi.clear();

    buffer.clear();
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;
    engine_.process (left, right, buffer.getNumSamples());
}

void FringeAudioProcessor::setCurrentProgram (int index)
{
    currentProgram_ = juce::jlimit (0, getNumPrograms() - 1, index);
    if (auto* p = apvts.getParameter ("preset"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (currentProgram_)));
        p->endChangeGesture();
    }
}

const juce::String FringeAudioProcessor::getProgramName (int index)
{
    index = juce::jlimit (0, getNumPrograms() - 1, index);
    return fringe::presetName (static_cast<fringe::Preset> (index));
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
