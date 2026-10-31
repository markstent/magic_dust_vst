#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "SeedHelper.h"
#include "md/midifile.hpp"
#include "md/ranges.hpp"
#include "plugin/DragFiles.h"
#include "plugin/PluginProcessor.h"
#include "ui/Drag.h"

namespace
{

constexpr int kTempo = 118;
constexpr int kFourChordSeed = 1;

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
        md::writeDragFile(idea, std::nullopt, 1, kTempo, 0, temp.folder).value_or(juce::File());
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

    const auto file =
        md::writeDragFile(idea, md::LaneId::bass, std::nullopt, kTempo, 0, temp.folder)
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

    CHECK_FALSE(md::writeDragFile(*processor.idea(), std::nullopt, 0, kTempo, 0,
                                  blocker.getChildFile("drag"))
                    .has_value());
}

TEST_CASE("a missing chord or no target gives no file", "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const auto& idea = *processor.idea();
    const TempFolder temp;

    CHECK_FALSE(
        md::writeDragFile(idea, std::nullopt, idea.progression.size(), kTempo, 0, temp.folder)
            .has_value());
    CHECK_FALSE(
        md::writeDragFile(idea, std::nullopt, std::nullopt, kTempo, 0, temp.folder).has_value());
    CHECK(temp.folder.getNumberOfChildFiles(juce::File::findFilesAndDirectories) == 0);
}

TEST_CASE("cleaning removes old .mid files and touches nothing else", "[dragfiles]")
{
    const TempFolder temp;
    const auto now = juce::Time::getCurrentTime();
    const auto old = now - juce::RelativeTime::hours(25);
    const auto make = [&](const char* name, juce::Time when)
    {
        const auto file = temp.folder.getChildFile(name);
        REQUIRE(file.create().wasOk());
        REQUIRE(file.setLastModificationTime(when));
        return file;
    };
    const auto stale = make("old.mid", old);
    const auto recent = make("recent.mid", now - juce::RelativeTime::hours(1));
    const auto other = make("old.txt", old);
    const auto subfolder = temp.folder.getChildFile("old-folder.mid");
    REQUIRE(subfolder.createDirectory().wasOk());
    REQUIRE(subfolder.setLastModificationTime(old));
    const auto target = make("target.dat", old);
    const auto link = temp.folder.getChildFile("link.mid");
    REQUIRE(target.createSymbolicLink(link, true));

    md::cleanDragFolder(temp.folder);

    CHECK_FALSE(stale.exists());
    CHECK(recent.exists());
    CHECK(other.exists());
    CHECK(subfolder.isDirectory());
    CHECK(link.isSymbolicLink());
    CHECK(target.exists());
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
    CHECK_FALSE(md::ui::exceedsDragThreshold({10, 10}, {10, 10}));
    CHECK_FALSE(md::ui::exceedsDragThreshold({10, 10}, {14, 10}));
    CHECK_FALSE(md::ui::exceedsDragThreshold({10, 10}, {13, 13}));
    CHECK(md::ui::exceedsDragThreshold({10, 10}, {15, 10}));
    CHECK(md::ui::exceedsDragThreshold({10, 10}, {14, 14}));
}

TEST_CASE("dragged chords and lanes carry the part's octave", "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const auto& idea = *processor.idea();
    const TempFolder temp;

    const auto lane =
        md::writeDragFile(idea, md::LaneId::bass, std::nullopt, kTempo, -1, temp.folder)
            .value_or(juce::File());
    REQUIRE(lane != juce::File());
    const std::vector<md::MidiTrack> laneTracks{
        md::shiftOctaves(md::laneTrack(idea, md::LaneId::bass), -1)};
    CHECK(contents(lane) == md::writeMidiFile(laneTracks, kTempo, md::ideaLengthTicks(idea)));
    CHECK(contents(lane) != md::writeMidiFile(std::vector{md::laneTrack(idea, md::LaneId::bass)},
                                              kTempo, md::ideaLengthTicks(idea)));

    const auto chord =
        md::writeDragFile(idea, std::nullopt, 1, kTempo, 2, temp.folder).value_or(juce::File());
    REQUIRE(chord != juce::File());
    const std::vector<md::MidiTrack> chordTracks{md::shiftOctaves(md::chordSlotTrack(idea, 1), 2)};
    CHECK(contents(chord) ==
          md::writeMidiFile(chordTracks, kTempo, idea.progression.at(1).bars * md::kTicksPerBar));
}

TEST_CASE("dragged lanes keep the bass on the grid and chords and melody within the feel",
          "[dragfiles]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor;
    setSeed(processor, kFourChordSeed);
    REQUIRE(processor.idea() != nullptr);
    const auto& idea = *processor.idea();
    constexpr int kMelodyBound = 14; // the widest melody offset, at groove heavy62
    constexpr int kChordBound = 20;  // the widest chord lay-back, at groove heavy62

    for (const md::LaneId lane : {md::LaneId::chords, md::LaneId::bass, md::LaneId::melody})
    {
        const auto& written = idea.lanes.at(static_cast<std::size_t>(lane)).events;
        const auto dragged = md::laneTrack(idea, lane);
        REQUIRE(dragged.notes.size() == written.size());
        REQUIRE_FALSE(written.empty());
        int bound = kMelodyBound;
        if (lane == md::LaneId::bass)
        {
            bound = 0;
        }
        else if (lane == md::LaneId::chords)
        {
            bound = kChordBound;
        }
        CHECK(std::abs(dragged.notes.front().start - written.front().start) <= bound);
        CHECK_NOTHROW(md::writeMidiFile(std::vector{dragged}, kTempo, md::ideaLengthTicks(idea)));
        for (std::size_t i = 0; i < written.size(); ++i)
        {
            CHECK(std::abs(dragged.notes.at(i).start - written.at(i).start) <= bound);
            CHECK(dragged.notes.at(i).start + dragged.notes.at(i).length <=
                  md::ideaLengthTicks(idea));
        }
        if (lane == md::LaneId::bass)
        {
            CHECK(dragged.notes == written);
        }
    }
}
