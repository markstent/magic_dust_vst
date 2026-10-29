#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "AllocationGuard.h"
#include "SeedHelper.h"
#include "md/bank.hpp"
#include "md/validate.hpp"
#include "plugin/Link.h"
#include "plugin/Playback.h"
#include "plugin/PluginProcessor.h"

namespace
{

constexpr int kBlock = 512;
constexpr double kRate = 48000.0;

struct FakeHead : juce::AudioPlayHead
{
    bool playing = true;
    double ppq = 0.0;
    double bpm = 120.0;

    [[nodiscard]] juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(bpm);
        info.setIsPlaying(playing);
        info.setPpqPosition(ppq);
        return info;
    }
};

// channel is 0-based
struct Ev
{
    int channel;
    int note;
    bool on;
    bool operator==(const Ev&) const = default;
};

struct Run
{
    std::vector<Ev> events;
    double energy = 0.0;
};

// Runs whole blocks, advancing the fake host's position when it is playing. Only processBlock
// is guarded: collecting the events allocates.
Run runBlocks(MagicDustProcessor& p, FakeHead& head, int blocks)
{
    Run run;
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize(16384);
    for (int b = 0; b < blocks; ++b)
    {
        {
            const md_test::ScopedNoAllocation guard;
            p.processBlock(buffer, midi);
        }
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            run.events.push_back(
                {.channel = m.getChannel() - 1, .note = m.getNoteNumber(), .on = m.isNoteOn()});
        }
        run.energy += buffer.getMagnitude(0, kBlock);
        if (head.playing)
        {
            head.ppq += md::ppqForSamples(kBlock, head.bpm, kRate);
        }
    }
    return run;
}

std::vector<Ev> expectedEvents(const md::Idea& idea, double endPpq)
{
    std::vector<Ev> out;
    const md::PlaybackPattern pattern = md::makePattern(idea);
    for (const md::PlayEvent& e : pattern.events)
    {
        if (e.tick / static_cast<double>(md::kTicksPerQuarter) < endPpq)
        {
            out.push_back({.channel = e.channel, .note = e.note, .on = e.on});
        }
    }
    return out;
}

std::set<std::pair<int, int>> sounding(const std::vector<Ev>& events)
{
    std::set<std::pair<int, int>> held;
    for (const Ev& e : events)
    {
        if (e.on)
        {
            held.insert({e.channel, e.note});
        }
        else
        {
            held.erase({e.channel, e.note});
        }
    }
    return held;
}

void setParam(MagicDustProcessor& p, const char* id, int index)
{
    auto* param = p.params.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
}

std::vector<std::vector<md::NoteEvent>> laneEvents(const md::Idea& idea)
{
    std::vector<std::vector<md::NoteEvent>> out;
    out.reserve(idea.lanes.size());
    for (const md::Lane& lane : idea.lanes)
    {
        out.push_back(lane.events);
    }
    return out;
}

std::vector<md::Quality> qualities(const md::Idea& idea)
{
    std::vector<md::Quality> out;
    out.reserve(idea.progression.size());
    for (const md::PlacedChord& c : idea.progression)
    {
        out.push_back(c.symbol.quality);
    }
    return out;
}

} // namespace

TEST_CASE("allocation guard counts allocations")
{
    md_test::armAllocationCount();
    md_test::allocateOnce();
    CHECK(md_test::disarmAllocationCount() == 1);
    md_test::allocateOnce();
    md_test::armAllocationCount();
    CHECK(md_test::disarmAllocationCount() == 0);
}

TEST_CASE("allocation guard counts malloc, realloc and calloc")
{
    if (!md_test::kCountsMalloc)
    {
        SKIP("sanitizer builds keep their own malloc");
    }
    md_test::armAllocationCount();
    md_test::mallocOnce();
    CHECK(md_test::disarmAllocationCount() >= 1);
}

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

TEST_CASE("construction writes the default idea")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    REQUIRE(processor.idea() != nullptr);
    CHECK_FALSE(processor.lastError().has_value());

    const auto seed = processor.idea()->seed;
    const md::Bank bank = md::loadBundledBank();
    const md::Idea expected = md::generate(bank, md::Settings{.groove = md::Groove::straight}, seed,
                                           md::defaultLaneSeeds(seed));
    CHECK(laneEvents(*processor.idea()) == laneEvents(expected));
    CHECK(processor.idea()->entryId == expected.entryId);
}

TEST_CASE("processor outputs silence with no transport")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);

    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::FloatVectorOperations::fill(buffer.getWritePointer(0), 1.0F, kBlock);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    CHECK(buffer.getMagnitude(0, kBlock) == 0.0F);
    CHECK(midi.isEmpty());
}

TEST_CASE("processor drops incoming midi")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);

    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 0);
    processor.processBlock(buffer, midi);

    CHECK(midi.isEmpty());
}

TEST_CASE("a playing host drives midi and sound")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    constexpr int blocks = 187; // just under 2 seconds
    const Run run = runBlocks(processor, head, blocks);

    const double endPpq = md::ppqForSamples(blocks * kBlock, 120.0, kRate);
    const auto expected = expectedEvents(*processor.idea(), endPpq);
    CHECK(run.events == expected);
    CHECK(run.energy > 0.0);
    std::set<int> channels;
    for (const Ev& e : run.events)
    {
        channels.insert(e.channel);
    }
    CHECK(channels == std::set<int>{0, 1, 2});
}

namespace
{

void setLevel(MagicDustProcessor& p, const char* id, float value)
{
    auto* parameter = p.params.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

// The energy of a playing host over just under 2 seconds, with the given levels set.
double playedEnergy(const char* id, float level, float volume = 0.8F)
{
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setLevel(processor, "volume", volume);
    setLevel(processor, id, level);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    return runBlocks(processor, head, 187).energy;
}

double auditionEnergy(const char* id, float level, float volume)
{
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setLevel(processor, "volume", volume);
    setLevel(processor, id, level);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    head.playing = false;
    processor.setPlayHead(&head);
    processor.auditionChord(0);
    return runBlocks(processor, head, 4).energy;
}

} // namespace

TEST_CASE("a lane level of 0 silences only that lane's built-in sound")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const double all = playedEnergy("bassLevel", 1.0F);
    const double noBass = playedEnergy("bassLevel", 0.0F);
    const double noChords = playedEnergy("chordsLevel", 0.0F);
    const double noMelody = playedEnergy("melodyLevel", 0.0F);
    CHECK(noBass > 0.0);
    CHECK(noBass < all);
    CHECK(noChords > 0.0);
    CHECK(noChords < all);
    CHECK(noMelody > 0.0);
    CHECK(noMelody < all);
    CHECK(auditionEnergy("chordsLevel", 0.0F, 0.8F) == 0.0);
    CHECK(auditionEnergy("bassLevel", 0.0F, 0.8F) > 0.0);
}

TEST_CASE("a lane level leaves the midi out unchanged")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    auto events = [](float level)
    {
        MagicDustProcessor processor;
        setSeed(processor, 48213);
        setLevel(processor, "bassLevel", level);
        processor.prepareToPlay(kRate, kBlock);
        FakeHead head;
        processor.setPlayHead(&head);
        return runBlocks(processor, head, 187).events;
    };
    CHECK(events(0.0F) == events(1.0F));
}

TEST_CASE("a voice's level is the volume times its lane level")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const double a = auditionEnergy("chordsLevel", 0.5F, 1.0F);
    const double b = auditionEnergy("chordsLevel", 1.0F, 0.5F);
    CHECK(a > 0.0);
    CHECK(a == Catch::Approx(b).epsilon(0.001));
}

TEST_CASE("a lane level above 1.0 makes the built-in voice louder")
{
    const double unity = auditionEnergy("chordsLevel", 1.0F, 0.8F);
    CHECK(auditionEnergy("chordsLevel", 2.0F, 0.8F) == Catch::Approx(2.0 * unity).epsilon(0.001));
    CHECK(auditionEnergy("chordsLevel", 2.0F, 2.0F) == Catch::Approx(5.0 * unity).epsilon(0.001));
}

TEST_CASE("levels and volume reach +6 dB with unity at three quarters of the travel")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    for (const char* id : {"chordsLevel", "bassLevel", "melodyLevel", "volume"})
    {
        const auto* parameter = processor.params.getParameter(id);
        CHECK(parameter->convertFrom0to1(1.0F) == Catch::Approx(2.0F));
        CHECK(parameter->convertFrom0to1(0.0F) == Catch::Approx(0.0F));
        CHECK(parameter->convertFrom0to1(0.75F) == Catch::Approx(1.0F).margin(0.001));
        CHECK(parameter->convertTo0to1(1.0F) == Catch::Approx(0.75F).margin(0.001));
    }
    for (const char* id : {"chordsLevel", "bassLevel", "melodyLevel"})
    {
        CHECK(processor.params.getParameter(id)->getDefaultValue() ==
              Catch::Approx(0.75F).margin(0.001));
        CHECK(processor.params.getRawParameterValue(id)->load() == Catch::Approx(1.0F));
    }
    CHECK(processor.params.getRawParameterValue("volume")->load() == Catch::Approx(0.8F));
}

TEST_CASE("level text is in dB and parses back")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    const auto* parameter = processor.params.getParameter("bassLevel");
    CHECK(parameter->getText(parameter->convertTo0to1(2.0F), 0) == "+6.0 dB");
    CHECK(parameter->getText(parameter->convertTo0to1(1.0F), 0) == "+0.0 dB");
    CHECK(parameter->getText(0.0F, 0) == "-inf dB");
    for (const float gain : {0.0F, 0.25F, 0.5F, 1.0F, 1.5F, 2.0F})
    {
        const auto text = parameter->getText(parameter->convertTo0to1(gain), 0);
        CHECK(parameter->convertFrom0to1(parameter->getValueForText(text)) ==
              Catch::Approx(gain).margin(0.01));
    }
}

TEST_CASE("a state saved with levels 1.0 and 0.5 loads the same gains")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor saver;
    juce::MemoryBlock state;
    saver.getStateInformation(state);
    auto saved =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(saved != nullptr);
    auto* params = saved->getChildByName(saver.params.state.getType());
    REQUIRE(params != nullptr);
    for (const auto& [id, gain] : {std::pair{"chordsLevel", 1.0}, std::pair{"bassLevel", 0.5}})
    {
        auto* child = params->getChildByAttribute("id", id);
        REQUIRE(child != nullptr);
        child->setAttribute("value", gain);
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*saved, old);
    MagicDustProcessor fresh;
    fresh.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    CHECK(fresh.params.getRawParameterValue("chordsLevel")->load() == Catch::Approx(1.0F));
    CHECK(fresh.params.getRawParameterValue("bassLevel")->load() == Catch::Approx(0.5F));
}

TEST_CASE("lane levels round trip, and a state without them loads 1.0")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setLevel(processor, "chordsLevel", 0.25F);
    setLevel(processor, "bassLevel", 0.5F);
    setLevel(processor, "melodyLevel", 0.75F);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh;
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto level = [](MagicDustProcessor& p, const char* id)
    { return p.params.getRawParameterValue(id)->load(); };
    CHECK(level(fresh, "chordsLevel") == Catch::Approx(0.25F));
    CHECK(level(fresh, "bassLevel") == Catch::Approx(0.5F));
    CHECK(level(fresh, "melodyLevel") == Catch::Approx(0.75F));

    // A state saved before the levels existed: parameters present, levels absent.
    auto saved =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(saved != nullptr);
    auto* params = saved->getChildByName(fresh.params.state.getType());
    REQUIRE(params != nullptr);
    for (const char* id : {"chordsLevel", "bassLevel", "melodyLevel"})
    {
        auto* child = params->getChildByAttribute("id", id);
        REQUIRE(child != nullptr);
        params->removeChildElement(child, true);
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*saved, old);
    setLevel(fresh, "bassLevel", 0.1F);
    fresh.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    CHECK(level(fresh, "chordsLevel") == Catch::Approx(1.0F));
    CHECK(level(fresh, "bassLevel") == Catch::Approx(1.0F));
    CHECK(level(fresh, "melodyLevel") == Catch::Approx(1.0F));
}

namespace
{

constexpr int kBassChannel = 1;

bool hasChannel(const std::vector<Ev>& events, int channel)
{
    return std::ranges::any_of(events, [channel](const Ev& e) { return e.channel == channel; });
}

} // namespace

TEST_CASE("a muted lane plays no midi and no built-in sound while the other lanes play on")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    const double bassLevelOff = playedEnergy("bassLevel", 0.0F);
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setLevel(processor, "bassMute", 1.0F);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    const Run run = runBlocks(processor, head, 187);
    CHECK_FALSE(hasChannel(run.events, kBassChannel));
    CHECK(hasChannel(run.events, 0));
    CHECK(hasChannel(run.events, 2));
    CHECK(run.energy > 0.0);
    CHECK(run.energy == Catch::Approx(bassLevelOff).epsilon(0.0001));
}

TEST_CASE("muting mid-note releases the note at once and unmuting waits for the next note start")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    // The held bass note, once one is sounding; chords hold lower-numbered channel entries first.
    const auto bassNote = [](const std::vector<Ev>& events) -> std::optional<int>
    {
        for (const auto& [channel, note] : sounding(events))
        {
            if (channel == kBassChannel)
            {
                return note;
            }
        }
        return std::nullopt;
    };
    std::vector<Ev> before;
    while (!bassNote(before))
    {
        const Run r = runBlocks(processor, head, 1);
        before.insert(before.end(), r.events.begin(), r.events.end());
        REQUIRE(head.ppq < 64.0);
    }
    const auto heldNote = bassNote(before);
    REQUIRE(heldNote.has_value());
    const int held = heldNote.value_or(0);

    setLevel(processor, "bassMute", 1.0F);
    const Run muted = runBlocks(processor, head, 1);
    CHECK(std::ranges::find(muted.events, Ev{kBassChannel, held, false}) != muted.events.end());

    setLevel(processor, "bassMute", 0.0F);
    const Run resumed = runBlocks(processor, head, 187);
    const auto firstBass =
        std::ranges::find_if(resumed.events, [](const Ev& e) { return e.channel == kBassChannel; });
    REQUIRE(firstBass != resumed.events.end());
    CHECK(firstBass->on);
}

TEST_CASE("lane mutes round trip, and a state saved without them loads all off")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setLevel(processor, "bassMute", 1.0F);
    setLevel(processor, "melodyMute", 1.0F);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh;
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK_FALSE(fresh.laneMuted(md::LaneId::chords));
    CHECK(fresh.laneMuted(md::LaneId::bass));
    CHECK(fresh.laneMuted(md::LaneId::melody));

    auto saved =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(saved != nullptr);
    auto* params = saved->getChildByName(fresh.params.state.getType());
    REQUIRE(params != nullptr);
    for (const char* id : {"chordsMute", "bassMute", "melodyMute"})
    {
        if (auto* child = params->getChildByAttribute("id", id))
        {
            params->removeChildElement(child, true);
        }
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*saved, old);
    fresh.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    CHECK_FALSE(fresh.laneMuted(md::LaneId::bass));
    CHECK_FALSE(fresh.laneMuted(md::LaneId::melody));
}

namespace
{

constexpr int kOctave = 12;

// The events of the lane's channel, in order.
std::vector<Ev> onChannel(const std::vector<Ev>& events, int channel)
{
    std::vector<Ev> out;
    std::ranges::copy_if(events, std::back_inserter(out),
                         [channel](const Ev& e) { return e.channel == channel; });
    return out;
}

// Runs blocks until the lane has a held note, and returns what has played so far.
std::vector<Ev> runUntilHeld(MagicDustProcessor& p, FakeHead& head, int channel)
{
    std::vector<Ev> played;
    while (!std::ranges::any_of(sounding(played),
                                [channel](const auto& n) { return n.first == channel; }))
    {
        const Run r = runBlocks(p, head, 1);
        played.insert(played.end(), r.events.begin(), r.events.end());
        REQUIRE(head.ppq < 64.0);
    }
    return played;
}

} // namespace

TEST_CASE("a part's octave shifts its midi out by 12 semitones per step and no other part")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setParam(processor, "bassOctave", -1);
    setParam(processor, "melodyOctave", 2);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    constexpr int blocks = 187;
    const Run run = runBlocks(processor, head, blocks);

    const double endPpq = md::ppqForSamples(blocks * kBlock, 120.0, kRate);
    auto expected = expectedEvents(*processor.idea(), endPpq);
    for (Ev& e : expected)
    {
        const std::array<int, 3> octaves{0, -1, 2};
        e.note += octaves.at(static_cast<std::size_t>(e.channel)) * kOctave;
    }
    CHECK(run.events == expected);
    CHECK(hasChannel(run.events, 1));
    CHECK(hasChannel(run.events, 2));
}

TEST_CASE("the built-in sound follows the octave, and a chord audition does too")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    // How often the left channel crosses zero: it halves when the pitch drops an octave.
    const auto crossings = [](MagicDustProcessor& processor, FakeHead& head, int blocks)
    {
        processor.setPlayHead(&head);
        juce::AudioBuffer<float> buffer(2, kBlock);
        juce::MidiBuffer midi;
        int count = 0;
        float last = 0.0F;
        for (int b = 0; b < blocks; ++b)
        {
            processor.processBlock(buffer, midi);
            for (int i = 0; i < kBlock; ++i)
            {
                const float v = buffer.getSample(0, i);
                count += (last < 0.0F) != (v < 0.0F) && v != 0.0F ? 1 : 0;
                last = v != 0.0F ? v : last;
            }
            head.ppq += md::ppqForSamples(kBlock, head.bpm, kRate);
        }
        return count;
    };
    const auto played = [&](const char* lane, int octave)
    {
        MagicDustProcessor processor;
        setSeed(processor, 48213);
        for (const char* level : {"chordsLevel", "bassLevel", "melodyLevel"})
        {
            setLevel(processor, level, 0.0F);
        }
        setLevel(processor, (std::string(lane) + "Level").c_str(), 1.0F);
        setParam(processor, (std::string(lane) + "Octave").c_str(), octave);
        processor.prepareToPlay(kRate, kBlock);
        FakeHead head;
        return crossings(processor, head, 187);
    };
    for (const char* lane : {"bass", "melody"})
    {
        const double plain = played(lane, 0);
        REQUIRE(plain > 0.0);
        CHECK(played(lane, -1) < plain * 0.7);
        CHECK(played(lane, 1) > plain * 1.3);
    }

    const auto audition = [&](int octave)
    {
        MagicDustProcessor processor;
        setSeed(processor, 48213);
        setParam(processor, "chordsOctave", octave);
        FakeHead head;
        head.playing = false;
        processor.prepareToPlay(kRate, kBlock);
        processor.auditionChord(0);
        return crossings(processor, head, 4);
    };
    CHECK(audition(-1) < audition(0) * 0.7);
}

TEST_CASE("a note shifted outside 0 to 127 is dropped with its note-off, never clamped")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setParam(processor, "bassOctave", -1);
    setParam(processor, "melodyOctave", 1);
    processor.prepareToPlay(kRate, kBlock);
    constexpr std::uint8_t kMelody = 2;
    const auto event = [](int tick, std::uint8_t channel, int note, bool on)
    {
        return md::PlayEvent{.tick = tick,
                             .channel = channel,
                             .note = static_cast<std::uint8_t>(note),
                             .velocity = static_cast<std::uint8_t>(on ? 90 : 0),
                             .on = on};
    };
    auto pattern = std::make_shared<const md::PlaybackPattern>(md::PlaybackPattern{
        .events = {event(0, 1, 5, true), event(0, kMelody, 120, true), event(0, kMelody, 60, true),
                   event(240, 1, 5, false), event(240, kMelody, 120, false),
                   event(240, kMelody, 60, false)},
        .lengthTicks = md::kTicksPerBar});
    Link::instance().publish(&processor, pattern);
    FakeHead head;
    processor.setPlayHead(&head);

    const Run run = runBlocks(processor, head, 20);
    CHECK(run.events ==
          std::vector<Ev>{{kMelody, 60 + kOctave, true}, {kMelody, 60 + kOctave, false}});
    Link::instance().remove(&processor);
}

TEST_CASE("changing an octave mid-note releases the note at the pitch it was sent at")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setParam(processor, "bassOctave", -1);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    std::vector<Ev> played = runUntilHeld(processor, head, kBassChannel);
    const auto held = *sounding(played).begin();
    REQUIRE(held.first == kBassChannel);

    setParam(processor, "bassOctave", 1);
    const Run changed = runBlocks(processor, head, 1);
    CHECK(std::ranges::find(changed.events, Ev{kBassChannel, held.second, false}) !=
          changed.events.end());
    played.insert(played.end(), changed.events.begin(), changed.events.end());
    CHECK_FALSE(std::ranges::any_of(sounding(played),
                                    [](const auto& n) { return n.first == kBassChannel; }));

    // The next notes start two octaves up, and stopping leaves nothing held.
    const Run later = runBlocks(processor, head, 187);
    const auto bass = onChannel(later.events, kBassChannel);
    REQUIRE_FALSE(bass.empty());
    const auto expected = onChannel(expectedEvents(*processor.idea(), 1000.0), kBassChannel);
    CHECK(std::ranges::any_of(expected, [&](const Ev& e)
                              { return e.on && e.note + kOctave == bass.front().note; }));
    played.insert(played.end(), later.events.begin(), later.events.end());
    head.playing = false;
    const Run stopped = runBlocks(processor, head, 1);
    played.insert(played.end(), stopped.events.begin(), stopped.events.end());
    CHECK(sounding(played).empty());
}

TEST_CASE("an octave change and the same note's own note-off in one block send one off")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);
    constexpr std::uint8_t kBass = 1;
    constexpr int kNote = 40;
    // One block is about 20 ticks, so the off at tick 30 falls in the second block.
    const auto event = [](int tick, bool on)
    {
        return md::PlayEvent{.tick = tick,
                             .channel = kBass,
                             .note = kNote,
                             .velocity = static_cast<std::uint8_t>(on ? 90 : 0),
                             .on = on};
    };
    Link::instance().publish(
        &processor,
        std::make_shared<const md::PlaybackPattern>(md::PlaybackPattern{
            .events = {event(0, true), event(30, false)}, .lengthTicks = md::kTicksPerBar}));
    FakeHead head;
    processor.setPlayHead(&head);

    const Run first = runBlocks(processor, head, 1);
    CHECK(first.events == std::vector<Ev>{{kBass, kNote, true}});
    setParam(processor, "bassOctave", 1);
    const Run both = runBlocks(processor, head, 1);
    CHECK(both.events == std::vector<Ev>{{kBass, kNote, false}});
    const Run rest = runBlocks(processor, head, 5);
    CHECK(rest.events.empty());
    Link::instance().remove(&processor);
}

TEST_CASE("muting a shifted lane releases its notes at the pitch they were sent at")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    setParam(processor, "bassOctave", 2);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    std::vector<Ev> played = runUntilHeld(processor, head, kBassChannel);
    setLevel(processor, "bassMute", 1.0F);
    const Run muted = runBlocks(processor, head, 1);
    played.insert(played.end(), muted.events.begin(), muted.events.end());
    CHECK_FALSE(std::ranges::any_of(sounding(played),
                                    [](const auto& n) { return n.first == kBassChannel; }));
}

TEST_CASE("octaves round trip, and a state saved without them loads 0")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setParam(processor, "chordsOctave", -2);
    setParam(processor, "bassOctave", -1);
    setParam(processor, "melodyOctave", 2);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh;
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(fresh.octave(md::LaneId::chords) == -2);
    CHECK(fresh.octave(md::LaneId::bass) == -1);
    CHECK(fresh.octave(md::LaneId::melody) == 2);

    auto saved =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(saved != nullptr);
    auto* params = saved->getChildByName(fresh.params.state.getType());
    REQUIRE(params != nullptr);
    for (const char* id : {"chordsOctave", "bassOctave", "melodyOctave"})
    {
        auto* child = params->getChildByAttribute("id", id);
        REQUIRE(child != nullptr);
        params->removeChildElement(child, true);
    }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary(*saved, old);
    fresh.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    for (const auto lane : {md::LaneId::chords, md::LaneId::bass, md::LaneId::melody})
    {
        CHECK(fresh.octave(lane) == 0);
    }
}

TEST_CASE("stopping the host releases every sounding note")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    const auto held = sounding(runBlocks(processor, head, 40).events);
    REQUIRE_FALSE(held.empty());

    head.playing = false;
    const Run next = runBlocks(processor, head, 1);
    std::set<std::pair<int, int>> released;
    for (const Ev& e : next.events)
    {
        CHECK_FALSE(e.on);
        released.insert({e.channel, e.note});
    }
    CHECK(released == held);
    CHECK(next.events.size() == held.size());
}

TEST_CASE("the play key runs the internal clock and stop releases")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    const Run hosted = runBlocks(processor, head, 40);
    head.playing = false;
    runBlocks(processor, head, 1);

    processor.play();
    const Run internal = runBlocks(processor, head, 40);
    CHECK(internal.events == hosted.events);
    CHECK(processor.previewPlaying());

    const auto held = sounding(internal.events);
    REQUIRE_FALSE(held.empty());
    processor.stop();
    const Run stopped = runBlocks(processor, head, 1);
    std::set<std::pair<int, int>> released;
    for (const Ev& e : stopped.events)
    {
        CHECK_FALSE(e.on);
        released.insert({e.channel, e.note});
    }
    CHECK(released == held);
    CHECK(stopped.events.size() == held.size());
    CHECK_FALSE(processor.previewPlaying());
}

TEST_CASE("an audition request plays the chord on the built-in sound only")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    head.playing = false;
    processor.setPlayHead(&head);

    processor.auditionChord(0);
    const Run run = runBlocks(processor, head, 4);
    CHECK(run.events.empty());
    CHECK(run.energy > 0.0);

    runBlocks(processor, head, 70); // one beat at 120 bpm plus the release
    CHECK(runBlocks(processor, head, 1).energy == 0.0);
}

TEST_CASE("suspending and re-preparing releases held notes while the host keeps playing")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    const auto held = sounding(runBlocks(processor, head, 40).events);
    REQUIRE_FALSE(held.empty());

    // The host position and pattern are unchanged, so only the suspend can trigger the release.
    processor.releaseResources();
    processor.prepareToPlay(kRate, kBlock);
    const Run next = runBlocks(processor, head, 1);
    std::set<std::pair<int, int>> released;
    std::size_t offs = 0;
    for (const Ev& e : next.events)
    {
        if (!e.on)
        {
            released.insert({e.channel, e.note});
            ++offs;
        }
    }
    CHECK(released == held);
    CHECK(offs == held.size());
}

TEST_CASE("a locate leaves no orphan note-offs")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);

    std::vector<Ev> events = runBlocks(processor, head, 40).events;
    REQUIRE_FALSE(sounding(events).empty());
    head.ppq += 1.0;
    const std::vector<Ev> after = runBlocks(processor, head, 200).events;
    events.insert(events.end(), after.begin(), after.end());

    std::set<std::pair<int, int>> held;
    for (const Ev& e : events)
    {
        if (e.on)
        {
            held.insert({e.channel, e.note});
        }
        else
        {
            CHECK(held.erase({e.channel, e.note}) == 1);
        }
    }
}

TEST_CASE("flavour regenerates after a quiet spell and groove at once")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 48213);
    const auto before = qualities(*processor.idea());

    setParam(processor, "flavour", 2);
    processor.tick(1000);
    processor.tick(1100);
    CHECK(qualities(*processor.idea()) == before);
    processor.tick(1200);
    CHECK(processor.idea()->seed == 48213);
    CHECK(processor.idea()->settings.flavour == md::Flavour::jazzy);
    CHECK(qualities(*processor.idea()) != before);

    setParam(processor, "groove", 3);
    processor.tick(1210);
    CHECK(processor.idea()->settings.groove == md::Groove::heavy62);
    CHECK(processor.idea()->seed == 48213);
}

TEST_CASE("state round trip restores the idea")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setSeed(processor, 777);
    setParam(processor, "flavour", 2);
    setParam(processor, "mood", 80);
    setParam(processor, "groove", 2);
    processor.setStyle(md::Style::festival);
    processor.setKeyRoot(3);
    REQUIRE_FALSE(processor.lastError().has_value());

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh;
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE(fresh.idea() != nullptr);
    const md::Idea& a = *processor.idea();
    const md::Idea& b = *fresh.idea();
    CHECK(b.seed == 777);
    CHECK(b.settings.style == md::Style::festival);
    CHECK(b.settings.keyRoot == 3);
    CHECK(b.settings.mood == 80);
    CHECK(b.settings.flavour == md::Flavour::jazzy);
    CHECK(b.settings.groove == md::Groove::ukShuffle58);
    for (std::size_t i = 0; i < a.lanes.size(); ++i)
    {
        CHECK(b.lanes.at(i).seed == a.lanes.at(i).seed);
    }
    CHECK(laneEvents(b) == laneEvents(a));
    CHECK(md::makePattern(b).events.size() == md::makePattern(a).events.size());
}

TEST_CASE("bad or old state leaves a valid default idea")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    const auto seed = processor.idea()->seed;

    const std::string garbage = "not a state";
    processor.setStateInformation(garbage.data(), static_cast<int>(garbage.size()));
    REQUIRE(processor.idea() != nullptr);
    CHECK(processor.idea()->seed == seed);

    juce::XmlElement old("MagicDust");
    old.setAttribute("schemaVersion", 1);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(old, block);
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(processor.idea() != nullptr);
    CHECK(processor.idea()->settings.style == md::Style::ukFunky);
    CHECK(processor.idea()->settings.keyRoot == 9);
    CHECK_FALSE(processor.lastError().has_value());
}

TEST_CASE("only a stereo output is supported")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    using Layout = juce::AudioProcessor::BusesLayout;

    Layout stereo;
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    CHECK(processor.isBusesLayoutSupported(stereo));

    Layout mono;
    mono.outputBuses.add(juce::AudioChannelSet::mono());
    CHECK_FALSE(processor.isBusesLayoutSupported(mono));

    Layout withInput = stereo;
    withInput.inputBuses.add(juce::AudioChannelSet::stereo());
    CHECK_FALSE(processor.isBusesLayoutSupported(withInput));
}

TEST_CASE("a failed generation keeps the previous idea and reports it")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    md::Bank bank = md::loadBundledBank();
    std::erase_if(bank.entries,
                  [](const md::StyleEntry& e) { return e.family == md::Family::mkDub; });
    MagicDustProcessor processor(std::move(bank));
    REQUIRE(processor.idea() != nullptr);
    const auto before = laneEvents(*processor.idea());

    processor.setStyle(md::Style::mkDub);

    REQUIRE(processor.idea() != nullptr);
    CHECK(laneEvents(*processor.idea()) == before);
    CHECK(processor.idea()->settings.style == md::Style::ukFunky);
    CHECK(processor.lastError() == std::optional<std::string>("couldn't write that idea"));
}

TEST_CASE("a bank that failed to load leaves no idea and says so")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(std::nullopt);
    CHECK(processor.idea() == nullptr);
    CHECK(processor.lastError() == std::optional<std::string>("the style bank failed to load"));

    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    const Run run = runBlocks(processor, head, 4);
    CHECK(run.events.empty());
    CHECK(run.energy == 0.0);
}

TEST_CASE("a state with no parameters resets them to their defaults")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor;
    setParam(processor, "mood", 80);
    setParam(processor, "flavour", 2);

    juce::XmlElement old("MagicDust");
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(old, block);
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    CHECK(processor.idea()->settings.mood == 30);
    CHECK(processor.idea()->settings.flavour == md::Flavour::soulful);
}

TEST_CASE("a state whose idea cannot be written keeps the saved settings matching the idea")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    md::Bank bank = md::loadBundledBank();
    std::erase_if(bank.entries,
                  [](const md::StyleEntry& e) { return e.family == md::Family::mkDub; });
    MagicDustProcessor processor(std::move(bank));

    juce::XmlElement state("MagicDust");
    state.setAttribute("style", static_cast<int>(md::Style::mkDub));
    state.setAttribute("keyRoot", 2);
    state.setAttribute("bars", 4);
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(state, block);
    processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

    juce::MemoryBlock saved;
    processor.getStateInformation(saved);
    const auto xml =
        juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    CHECK(xml->getIntAttribute("style") == static_cast<int>(processor.idea()->settings.style));
    CHECK(xml->getIntAttribute("keyRoot") == processor.idea()->settings.keyRoot);
    CHECK(xml->getIntAttribute("bars") == processor.idea()->settings.bars);
}

TEST_CASE("link publishes per owner and removes")
{
    int a = 0;
    int b = 0;
    Link& link = Link::instance();
    auto pattern = std::make_shared<const md::PlaybackPattern>(
        md::PlaybackPattern{.events = {}, .lengthTicks = 960});
    link.publish(&a, pattern);
    CHECK(link.current(&a) == pattern.get());
    CHECK(link.current(&b) == nullptr);
    link.remove(&a);
    CHECK(link.current(&a) == nullptr);
    link.retire();
}

TEST_CASE("link publishes a main's octaves, which read 0 before and after")
{
    int a = 0;
    Link& link = Link::instance();
    link.setOctave(&a, md::LaneId::bass, 2);
    CHECK(link.octave(&a, md::LaneId::bass) == 0);
    link.publish(&a, std::make_shared<const md::PlaybackPattern>(
                         md::PlaybackPattern{.events = {}, .lengthTicks = 960}));
    link.setOctave(&a, md::LaneId::bass, -2);
    CHECK(link.octave(&a, md::LaneId::bass) == -2);
    CHECK(link.octave(&a, md::LaneId::chords) == 0);
    link.remove(&a);
    CHECK(link.octave(&a, md::LaneId::bass) == 0);
    link.retire();
}

TEST_CASE("a replaced pattern outlives retire for a second and is then freed")
{
    int owner = 0;
    Link& link = Link::instance();
    auto first = std::make_shared<const md::PlaybackPattern>(
        md::PlaybackPattern{.events = {}, .lengthTicks = 960});
    const std::weak_ptr<const md::PlaybackPattern> watch = first;
    link.publish(&owner, std::move(first));
    link.publish(&owner, std::make_shared<const md::PlaybackPattern>(
                             md::PlaybackPattern{.events = {}, .lengthTicks = 1920}));

    link.retire();
    CHECK_FALSE(watch.expired());
    link.retire(std::chrono::steady_clock::now() + std::chrono::milliseconds(1100));
    CHECK(watch.expired());
    link.remove(&owner);
}

TEST_CASE("hostPlaying follows the playhead", "[processor]")
{
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    CHECK_FALSE(processor.hostPlaying());
    processor.processBlock(buffer, midi);
    CHECK(processor.hostPlaying());
    head.playing = false;
    processor.processBlock(buffer, midi);
    CHECK_FALSE(processor.hostPlaying());
}

TEST_CASE("play and stop raise the follow notice, not the preview, while the host plays",
          "[processor]")
{
    MagicDustProcessor processor;
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
    CHECK_FALSE(processor.followNotice());
    processor.play();
    CHECK(processor.followNotice());
    processor.tick(juce::Time::getMillisecondCounter() + 2500);
    CHECK_FALSE(processor.followNotice());
    head.playing = false;
    processor.processBlock(buffer, midi);
    processor.play();
    CHECK_FALSE(processor.followNotice());
    processor.processBlock(buffer, midi);
    CHECK(processor.previewPlaying());
}

namespace
{

std::vector<int> starts(const md::Idea& idea, md::LaneId lane)
{
    std::vector<int> out;
    for (const md::NoteEvent& e : idea.lanes.at(static_cast<std::size_t>(lane)).events)
    {
        out.push_back(e.start);
    }
    return out;
}

} // namespace

TEST_CASE("a pinned bass keeps its starts through sprinkles and the chords change", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setParam(processor, "bassPin", 1);
    const auto bassStarts = starts(*processor.idea(), md::LaneId::bass);
    int chordsChanged = 0;
    constexpr int sprinkles = 10;
    for (int i = 0; i < sprinkles; ++i)
    {
        const auto before = qualities(*processor.idea());
        processor.sprinkle();
        REQUIRE_FALSE(processor.lastError().has_value());
        CHECK(starts(*processor.idea(), md::LaneId::bass) == bassStarts);
        chordsChanged += before != qualities(*processor.idea()) ? 1 : 0;
    }
    CHECK(chordsChanged >= 9);
}

TEST_CASE("pinned chords survive a mood change after the quiet spell", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setSeed(processor, 48213);
    setParam(processor, "chordsPin", 1);
    const auto chords = processor.idea()->lanes.at(0).events;
    const auto progression = qualities(*processor.idea());

    processor.tick(1000);
    setParam(processor, "mood", 90);
    processor.tick(1010);
    processor.tick(1200);
    REQUIRE(processor.idea()->settings.mood == 90);
    CHECK(processor.idea()->lanes.at(0).events == chords);
    CHECK(qualities(*processor.idea()) == progression);
}

TEST_CASE("a pinned lane that is muted stays pinned and silent", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setSeed(processor, 48213);
    setParam(processor, "bassPin", 1);
    setParam(processor, "bassMute", 1);
    const auto bass = starts(*processor.idea(), md::LaneId::bass);
    processor.sprinkle();
    CHECK(starts(*processor.idea(), md::LaneId::bass) == bass);
    processor.prepareToPlay(kRate, kBlock);
    FakeHead head;
    processor.setPlayHead(&head);
    CHECK_FALSE(hasChannel(runBlocks(processor, head, 187).events, kBassChannel));
}

TEST_CASE("a key change with the melody pinned transposes it and stays valid", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setParam(processor, "melodyPin", 1);
    processor.setKeyRoot((processor.idea()->settings.keyRoot + 2) % 12);
    REQUIRE_FALSE(processor.lastError().has_value());
    CHECK(md::validate(*processor.idea()).empty());
}

TEST_CASE("the saved idea loads back exactly, with its pins and refit", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setParam(processor, "bassPin", 1);
    setParam(processor, "melodyPin", 1);
    processor.setKeyRoot((processor.idea()->settings.keyRoot + 5) % 12);
    processor.sprinkle();
    REQUIRE_FALSE(processor.lastError().has_value());

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh(md::loadBundledBank(), 99);
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE(fresh.idea() != nullptr);
    const md::Idea& a = *processor.idea();
    const md::Idea& b = *fresh.idea();
    CHECK(laneEvents(b) == laneEvents(a));
    CHECK(qualities(b) == qualities(a));
    CHECK(b.entryId == a.entryId);
    CHECK(b.seed == a.seed);
    for (std::size_t i = 0; i < a.lanes.size(); ++i)
    {
        CHECK(b.lanes.at(i).refitted == a.lanes.at(i).refitted);
        CHECK(b.lanes.at(i).seed == a.lanes.at(i).seed);
    }
}

TEST_CASE("a state with no idea or a corrupt idea loads from the seeds", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setSeed(processor, 777);
    const auto expected = laneEvents(*processor.idea());

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    const auto saved =
        juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(saved->getChildByName("Idea") != nullptr);

    const auto load = [&](const juce::XmlElement& xml)
    {
        MagicDustProcessor fresh(md::loadBundledBank(), 99);
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary(xml, block);
        fresh.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        REQUIRE(fresh.idea() != nullptr);
        CHECK(laneEvents(*fresh.idea()) == expected);
        CHECK_FALSE(fresh.lastError().has_value());
    };

    juce::XmlElement without(*saved);
    without.removeChildElement(without.getChildByName("Idea"), true);
    load(without);

    const auto loadWith = [&](const char* attribute, const char* value)
    {
        auto edited = std::make_unique<juce::XmlElement>(*saved);
        edited->getChildByName("Idea")->setAttribute(attribute, value);
        load(*edited);
    };
    loadWith("mood", "loud");
    loadWith("entryId", "no-such-entry");
}

TEST_CASE("everything pinned: sprinkle, save and load keeps the idea and its number", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    const auto number = processor.idea()->seed;
    for (const char* id : {"chordsPin", "bassPin", "melodyPin"})
    {
        setParam(processor, id, 1);
    }
    processor.sprinkle();
    CHECK(processor.idea()->seed == number);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh(md::loadBundledBank(), 99);
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE(fresh.idea() != nullptr);
    CHECK(fresh.idea()->seed == number);
    CHECK(laneEvents(*fresh.idea()) == laneEvents(*processor.idea()));
}

TEST_CASE("a state saved inside the quiet window keeps the pinned chords and applies the mood",
          "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setParam(processor, "chordsPin", 1);
    const auto chords = processor.idea()->lanes.at(0).events;
    setParam(processor, "mood", 90);

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    MagicDustProcessor fresh(md::loadBundledBank(), 99);
    fresh.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    REQUIRE(fresh.idea() != nullptr);
    CHECK(fresh.idea()->lanes.at(0).events == chords);
    fresh.tick(1000);
    fresh.tick(1200);
    CHECK(fresh.idea()->settings.mood == 90);
    CHECK(fresh.idea()->lanes.at(0).events == chords);
}

TEST_CASE("a failed regenerate with a pinned lane keeps the idea and reports it", "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    md::Bank bank = md::loadBundledBank();
    std::erase_if(bank.entries,
                  [](const md::StyleEntry& e) { return e.family == md::Family::mkDub; });
    MagicDustProcessor processor(std::move(bank), 5);
    setParam(processor, "melodyPin", 1);
    const auto before = laneEvents(*processor.idea());

    processor.setStyle(md::Style::mkDub);

    REQUIRE(processor.idea() != nullptr);
    CHECK(laneEvents(*processor.idea()) == before);
    CHECK(processor.idea()->settings.style == md::Style::ukFunky);
    CHECK(processor.lastError() == std::optional<std::string>("couldn't write that idea"));
}

TEST_CASE("an unpinned bass regenerates from the seed its idea carried, not the sprinkle's",
          "[pins]")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor processor(md::loadBundledBank(), 5);
    setParam(processor, "bassPin", 1);
    const auto carried = processor.idea()->lanes.at(1).seed;
    processor.sprinkle();
    REQUIRE(processor.idea()->lanes.at(1).seed == carried);
    REQUIRE(md::defaultLaneSeeds(processor.idea()->seed).bass != carried);
    setParam(processor, "bassPin", 0);

    processor.tick(1000);
    setParam(processor, "energy", 20);
    processor.tick(1010);
    processor.tick(1200);

    REQUIRE(processor.idea()->settings.energy == 20);
    CHECK(processor.idea()->lanes.at(1).seed == carried);
}
