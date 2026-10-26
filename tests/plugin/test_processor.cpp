#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PluginProcessor.h"

TEST_CASE("processor identity")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    CHECK(processor.getName() == "Magic Dust");
    CHECK(processor.acceptsMidi());
    CHECK(processor.producesMidi());
    CHECK(processor.hasEditor());
    CHECK(processor.getTailLengthSeconds() == 0.0);
}

TEST_CASE("processor outputs silence")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    for (int channel = 0; channel < 2; ++channel)
    {
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 1.0F, 512);
    }
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int i = 0; i < 512; ++i)
        {
            REQUIRE(buffer.getSample(channel, i) == 0.0F);
        }
    }
}

TEST_CASE("state round trip does not throw")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    const auto xml =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(xml != nullptr);
    CHECK(xml->hasTagName("MagicDust"));
    CHECK(xml->getIntAttribute("schemaVersion") == 1);

    MagicDustProcessor fresh;
    CHECK_NOTHROW(fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize())));
}

TEST_CASE("processor drops incoming midi")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 0);
    processor.processBlock(buffer, midi);

    CHECK(midi.isEmpty());
}
