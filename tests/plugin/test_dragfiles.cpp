#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "md/midifile.hpp"
#include "md/ranges.hpp"
#include "plugin/DragFiles.h"
#include "plugin/PluginProcessor.h"

namespace
{

constexpr int kTempo = 118;
constexpr int kFourChordSeed = 1;

// Puts the processor on a known idea: the state's seed is restored and the lane seeds derived.
void setSeed(MagicDustProcessor& p, int seed)
{
    juce::MemoryBlock state;
    p.getStateInformation(state);
    auto xml =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    xml->setAttribute("seed", seed);
    xml->removeAttribute("chordsSeed");
    juce::MemoryBlock edited;
    juce::AudioProcessor::copyXmlToBinary(*xml, edited);
    p.setStateInformation(edited.getData(), static_cast<int>(edited.getSize()));
}

// A scratch folder that is gone when the test ends.
struct TempFolder
{
    juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("md-dragfiles-" + juce::Uuid().toString());
    TempFolder() { folder.createDirectory(); }
    TempFolder(const TempFolder&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;
    ~TempFolder() { folder.deleteRecursively(); }
};

std::vector<std::uint8_t> contents(const juce::File& file)
{
    juce::MemoryBlock block;
    REQUIRE(file.loadFileAsData(block));
    std::vector<std::uint8_t> bytes(block.getSize());
    block.copyTo(bytes.data(), 0, bytes.size());
    return bytes;
}

} // namespace

TEST_CASE("a chord's drag file holds that chord's notes under the expected name", "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const auto& idea = *processor.idea();
    const TempFolder temp;

    const auto file =
        md::writeDragFile(idea, std::nullopt, 1, kTempo, temp.folder).value_or(juce::File());
    REQUIRE(file != juce::File());
    CHECK(file.getFileName().toStdString() == md::dragFileName(idea, std::nullopt, 1, kTempo));
    const std::vector<md::MidiTrack> tracks{md::chordSlotTrack(idea, 1)};
    CHECK(contents(file) ==
          md::writeMidiFile(tracks, kTempo, idea.progression.at(1).bars * md::kTicksPerBar));
}

TEST_CASE("a lane's drag file holds that lane for the whole idea", "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const auto& idea = *processor.idea();
    const TempFolder temp;

    const auto file = md::writeDragFile(idea, md::LaneId::bass, std::nullopt, kTempo, temp.folder)
                          .value_or(juce::File());
    REQUIRE(file != juce::File());
    CHECK(file.getFileName().toStdString() ==
          md::dragFileName(idea, md::LaneId::bass, std::nullopt, kTempo));
    const std::vector<md::MidiTrack> tracks{md::laneTrack(idea, md::LaneId::bass)};
    CHECK(contents(file) == md::writeMidiFile(tracks, kTempo, md::ideaLengthTicks(idea)));
}

TEST_CASE("a folder that cannot be made gives no file", "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const TempFolder temp;
    const auto blocker = temp.folder.getChildFile("blocker");
    REQUIRE(blocker.create().wasOk());

    CHECK_FALSE(
        md::writeDragFile(*processor.idea(), std::nullopt, 0, kTempo, blocker.getChildFile("drag"))
            .has_value());
}

TEST_CASE("cleaning removes files older than 24 hours and keeps newer ones", "[dragfiles]")
{
    const TempFolder temp;
    const auto old = temp.folder.getChildFile("old.mid");
    const auto recent = temp.folder.getChildFile("recent.mid");
    REQUIRE(old.create().wasOk());
    REQUIRE(recent.create().wasOk());
    const auto now = juce::Time::getCurrentTime();
    REQUIRE(old.setLastModificationTime(now - juce::RelativeTime::hours(25)));
    REQUIRE(recent.setLastModificationTime(now - juce::RelativeTime::hours(1)));

    md::cleanDragFolder(temp.folder);

    CHECK_FALSE(old.exists());
    CHECK(recent.exists());
}

TEST_CASE("the tempo is the host's rounded, or 124 when unknown", "[dragfiles]")
{
    CHECK(md::dragTempo(std::nullopt) == 124);
    CHECK(md::dragTempo(120.4) == 120);
    CHECK(md::dragTempo(120.6) == 121);
    CHECK(md::dragTempo(100000.0) == md::kMidiTempoMax);
}

TEST_CASE("a drag starts only past 4 px of movement", "[dragfiles]")
{
    CHECK_FALSE(md::exceedsDragThreshold({10, 10}, {10, 10}));
    CHECK_FALSE(md::exceedsDragThreshold({10, 10}, {14, 10}));
    CHECK_FALSE(md::exceedsDragThreshold({10, 10}, {13, 13}));
    CHECK(md::exceedsDragThreshold({10, 10}, {15, 10}));
    CHECK(md::exceedsDragThreshold({10, 10}, {14, 14}));
}
