#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{

constexpr int schemaVersion = 1;

} // namespace

MagicDustProcessor::MagicDustProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MagicDustProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

void MagicDustProcessor::releaseResources() {}

void MagicDustProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    buffer.clear();
}

juce::AudioProcessorEditor* MagicDustProcessor::createEditor()
{
    return new MagicDustEditor(*this);
}

bool MagicDustProcessor::hasEditor() const { return true; }

const juce::String MagicDustProcessor::getName() const { return "Magic Dust"; }

bool MagicDustProcessor::acceptsMidi() const { return true; }

bool MagicDustProcessor::producesMidi() const { return true; }

double MagicDustProcessor::getTailLengthSeconds() const { return 0.0; }

int MagicDustProcessor::getNumPrograms() { return 1; }

int MagicDustProcessor::getCurrentProgram() { return 0; }

void MagicDustProcessor::setCurrentProgram(int /*index*/) {}

const juce::String MagicDustProcessor::getProgramName(int /*index*/) { return {}; }

void MagicDustProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

void MagicDustProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("MagicDust");
    state.setProperty("schemaVersion", schemaVersion, nullptr);
    if (const auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void MagicDustProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/) {}

// Entry point JUCE's plugin wrappers call to create the processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MagicDustProcessor(); }
