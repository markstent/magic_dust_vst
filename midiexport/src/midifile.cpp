#include "md/midifile.hpp"

#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace md
{
namespace
{
using Bytes = std::vector<std::uint8_t>;
using Byte = std::uint8_t;

constexpr std::size_t kMaxTracks = 254; // format 1 adds a conductor; the count is one byte
constexpr unsigned kMicrosPerMinute = 60'000'000U;

Byte lo(unsigned v) { return static_cast<Byte>(v & 0xFFU); }

void put(Bytes& out, std::initializer_list<Byte> bytes) { out.insert(out.end(), bytes); }

void putU32(Bytes& out, std::uint32_t v)
{
    put(out, {lo(v >> 24U), lo(v >> 16U), lo(v >> 8U), lo(v)});
}

void putVarLen(Bytes& out, int value)
{
    auto v = static_cast<unsigned>(value);
    std::array<Byte, 5> groups{};
    groups.at(0) = lo(v & 0x7FU);
    std::size_t n = 1;
    v >>= 7U;
    while (v != 0)
    {
        groups.at(n++) = lo((v & 0x7FU) | 0x80U);
        v >>= 7U;
    }
    while (n > 0)
    {
        out.push_back(groups.at(--n));
    }
}

void putText(Bytes& out, Byte type, const std::string& text)
{
    put(out, {0xFF, type});
    putVarLen(out, static_cast<int>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

void putTempoAndTimeSignature(Bytes& out, int tempoBpm)
{
    const unsigned micros = kMicrosPerMinute / static_cast<unsigned>(tempoBpm);
    put(out, {0x00, 0xFF, 0x51, 0x03, lo(micros >> 16U), lo(micros >> 8U), lo(micros)});
    put(out, {0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08});
}

struct Event
{
    int tick;
    int rank; // marker, note-off, note-on: offs sort before ons at equal ticks
    int pitch;
    int velocity;
    const std::string* text;
};

Bytes eventsChunk(Bytes body, const MidiTrack& track, int totalTicks)
{
    std::vector<Event> events;
    events.reserve(track.markers.size() + (2 * track.notes.size()));
    for (const auto& [tick, text] : track.markers)
    {
        events.push_back({.tick = tick, .rank = 0, .pitch = 0, .velocity = 0, .text = &text});
    }
    for (const NoteEvent& n : track.notes)
    {
        events.push_back({
            .tick = n.start,
            .rank = 2,
            .pitch = n.pitch,
            .velocity = n.velocity,
            .text = nullptr,
        });
        events.push_back({
            .tick = n.start + n.length,
            .rank = 1,
            .pitch = n.pitch,
            .velocity = 0,
            .text = nullptr,
        });
    }
    std::ranges::stable_sort(
        events, [](const Event& a, const Event& b)
        { return std::tie(a.tick, a.rank, a.pitch) < std::tie(b.tick, b.rank, b.pitch); });
    int last = 0;
    for (const Event& e : events)
    {
        putVarLen(body, e.tick - last);
        last = e.tick;
        if (e.text != nullptr)
        {
            putText(body, 0x06, *e.text);
        }
        else
        {
            put(body, {
                          lo((e.rank == 2 ? 0x90U : 0x80U) | static_cast<unsigned>(track.channel)),
                          lo(static_cast<unsigned>(e.pitch)),
                          lo(static_cast<unsigned>(e.velocity)),
                      });
        }
    }
    putVarLen(body, totalTicks - last);
    put(body, {0xFF, 0x2F, 0x00});
    return body;
}

void putChunk(Bytes& out, const Bytes& body)
{
    put(out, {'M', 'T', 'r', 'k'});
    putU32(out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): mirrors writeMidiFile.
void validate(std::span<const MidiTrack> tracks, int tempoBpm, int totalTicks)
{
    if (tracks.empty() || tracks.size() > kMaxTracks)
    {
        throw std::invalid_argument("writeMidiFile: track count out of range");
    }
    if (tempoBpm < kMidiTempoMin || tempoBpm > kMidiTempoMax)
    {
        throw std::invalid_argument("writeMidiFile: tempo out of range");
    }
    for (const MidiTrack& t : tracks)
    {
        if (t.channel < 0 || t.channel > 15)
        {
            throw std::invalid_argument("writeMidiFile: channel out of range");
        }
        for (const NoteEvent& n : t.notes)
        {
            if (n.pitch < 0 || n.pitch > 127 || n.velocity < 1 || n.velocity > 127)
            {
                throw std::invalid_argument("writeMidiFile: pitch or velocity out of range");
            }
            if (n.start < 0 || n.length <= 0 || n.length > totalTicks - n.start)
            {
                throw std::invalid_argument("writeMidiFile: note outside the track");
            }
        }
        for (const auto& marker : t.markers)
        {
            if (marker.first < 0 || marker.first > totalTicks)
            {
                throw std::invalid_argument("writeMidiFile: marker outside the track");
            }
        }
    }
}
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the spec fixes this signature.
std::vector<std::uint8_t> writeMidiFile(std::span<const MidiTrack> tracks, int tempoBpm,
                                        int totalTicks)
{
    validate(tracks, tempoBpm, totalTicks);
    const bool single = tracks.size() == 1;

    Bytes out;
    put(out, {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, single ? Byte{0} : Byte{1}});
    put(out, {0, lo(static_cast<unsigned>(tracks.size() + (single ? 0 : 1))), 0x03, 0xC0});

    if (single)
    {
        Bytes body = {0x00};
        putText(body, 0x03, tracks.front().name);
        putTempoAndTimeSignature(body, tempoBpm);
        putChunk(out, eventsChunk(std::move(body), tracks.front(), totalTicks));
        return out;
    }

    Bytes conductor;
    putTempoAndTimeSignature(conductor, tempoBpm);
    MidiTrack markers;
    for (const MidiTrack& t : tracks)
    {
        markers.markers.insert(markers.markers.end(), t.markers.begin(), t.markers.end());
    }
    putChunk(out, eventsChunk(std::move(conductor), markers, totalTicks));
    for (const MidiTrack& t : tracks)
    {
        Bytes body = {0x00};
        putText(body, 0x03, t.name);
        const MidiTrack notesOnly{
            .name = t.name, .notes = t.notes, .markers = {}, .channel = t.channel};
        putChunk(out, eventsChunk(std::move(body), notesOnly, totalTicks));
    }
    return out;
}

int ideaLengthTicks(const Idea& idea) { return idea.settings.bars * kTicksPerBar; }

MidiTrack laneTrack(const Idea& idea, LaneId lane)
{
    static constexpr std::array<std::string_view, 3> kNames = {"Chords", "Bass", "Melody"};
    const auto index = static_cast<std::size_t>(lane);
    if (index >= idea.lanes.size())
    {
        throw std::invalid_argument("laneTrack: unknown lane");
    }
    if (idea.lanes.at(index).id != lane)
    {
        throw std::logic_error("laneTrack: lanes out of order");
    }
    MidiTrack track{
        .name = std::string(kNames.at(index)),
        .notes = idea.lanes.at(index).events,
        .markers = {},
        .channel = static_cast<int>(index),
    };
    if (lane == LaneId::chords)
    {
        for (const PlacedChord& c : idea.progression)
        {
            track.markers.emplace_back(c.startBar * kTicksPerBar, chordName(idea.key, c.symbol));
        }
    }
    return track;
}

MidiTrack shiftOctaves(MidiTrack track, int octaves)
{
    const int shift = octaves * kPitchClasses;
    std::erase_if(track.notes, [shift](const NoteEvent& n)
                  { return n.pitch + shift < kMidiPitchMin || n.pitch + shift > kMidiPitchMax; });
    for (NoteEvent& n : track.notes)
    {
        n.pitch += shift;
    }
    return track;
}

MidiTrack chordSlotTrack(const Idea& idea, std::size_t slot)
{
    const PlacedChord& chord = idea.progression.at(slot);
    const int begin = chord.startBar * kTicksPerBar;
    const int end = begin + (chord.bars * kTicksPerBar);
    MidiTrack track{.name = "Chord", .notes = {}, .markers = {}, .channel = 0};
    for (NoteEvent n : laneTrack(idea, LaneId::chords).notes)
    {
        if (n.start >= begin && n.start < end)
        {
            n.length = std::min(n.start + n.length, end) - n.start;
            n.start -= begin;
            track.notes.push_back(n);
        }
    }
    return track;
}

std::string dragFileName(const Idea& idea, std::optional<LaneId> lane,
                         std::optional<std::size_t> slot, int tempoBpm)
{
    static constexpr std::array<std::string_view, 3> kLaneNames = {"Chords", "Bass", "Melody"};
    std::string prefix = "Magic Dust";
    std::string chords;
    if (slot)
    {
        prefix = "Chord";
        chords = chordName(idea.key, idea.progression.at(*slot).symbol);
    }
    else
    {
        if (lane)
        {
            prefix = kLaneNames.at(static_cast<std::size_t>(*lane));
        }
        for (std::size_t i = 0; i < std::min<std::size_t>(4, idea.progression.size()); ++i)
        {
            chords += (i == 0 ? "" : " ") + chordName(idea.key, idea.progression.at(i).symbol);
        }
    }
    return prefix + " - " + chords + " - " + std::to_string(tempoBpm) + "bpm.mid";
}

std::vector<MidiTrack> ideaTracks(const Idea& idea)
{
    return {
        laneTrack(idea, LaneId::chords),
        laneTrack(idea, LaneId::bass),
        laneTrack(idea, LaneId::melody),
    };
}
} // namespace md
