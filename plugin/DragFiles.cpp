#include "plugin/DragFiles.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "md/midifile.hpp"
#include "md/ranges.hpp"
#include "ui/Tokens.h"

namespace md
{

juce::File defaultDragFolder()
{
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Caches/Magic Dust/drag");
}

std::optional<juce::File> writeDragFile(const Idea& idea, std::optional<LaneId> lane,
                                        std::optional<std::size_t> slot, int tempoBpm,
                                        const juce::File& folder)
{
    std::vector<MidiTrack> tracks;
    int totalTicks = ideaLengthTicks(idea);
    if (slot)
    {
        tracks.push_back(chordSlotTrack(idea, *slot));
        totalTicks = idea.progression.at(*slot).bars * kTicksPerBar;
    }
    else if (lane)
    {
        tracks.push_back(laneTrack(idea, *lane));
    }
    else
    {
        tracks = ideaTracks(idea);
    }
    const auto bytes = writeMidiFile(tracks, tempoBpm, totalTicks);

    if (!folder.createDirectory().wasOk())
    {
        return std::nullopt;
    }
    auto file = folder.getChildFile(
        juce::File::createLegalFileName(dragFileName(idea, lane, slot, tempoBpm)));
    if (!file.replaceWithData(bytes.data(), bytes.size()))
    {
        return std::nullopt;
    }
    return file;
}

void cleanDragFolder(const juce::File& folder)
{
    const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::hours(24);
    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false))
    {
        if (file.getLastModificationTime() < cutoff)
        {
            file.deleteFile();
        }
    }
}

int dragTempo(std::optional<double> hostBpm)
{
    if (!hostBpm)
    {
        return kDefaultTempo;
    }
    return std::clamp(static_cast<int>(std::lround(*hostBpm)), kMidiTempoMin, kMidiTempoMax);
}

bool exceedsDragThreshold(juce::Point<int> down, juce::Point<int> now)
{
    return down.getDistanceFrom(now) > md::tokens::dragThreshold;
}

} // namespace md
