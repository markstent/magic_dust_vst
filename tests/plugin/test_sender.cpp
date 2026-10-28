#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "AllocationGuard.h"
#include "SeedHelper.h"
#include "plugin/Link.h"
#include "plugin/Playback.h"
#include "plugin/PluginProcessor.h"

namespace
{

constexpr int kBlock = 512;
constexpr double kRate = 48000.0;
constexpr int kBassChannel = 1;

struct FakeHead : juce::AudioPlayHead
{
    double ppq = 0.0;

    [[nodiscard]] juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(120.0);
        info.setIsPlaying(true);
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
        head.ppq += md::ppqForSamples(kBlock, 120.0, kRate);
    }
    return run;
}

std::vector<Ev> laneEventsUpTo(const md::Idea& idea, int channel, double endPpq)
{
    std::vector<Ev> out;
    for (const md::PlayEvent& e : md::makePattern(idea).events)
    {
        if (e.channel == static_cast<std::uint8_t>(channel) &&
            e.tick / static_cast<double>(md::kTicksPerQuarter) < endPpq)
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

void prepare(MagicDustProcessor& p, FakeHead& head)
{
    p.prepareToPlay(kRate, kBlock);
    p.setPlayHead(&head);
}

juce::MemoryBlock save(MagicDustProcessor& p)
{
    juce::MemoryBlock state;
    p.getStateInformation(state);
    return state;
}

} // namespace

TEST_CASE("the first instance is main and later ones send chords, bass, melody")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor first;
    MagicDustProcessor second;
    MagicDustProcessor third;
    MagicDustProcessor fourth;

    CHECK(first.mode() == InstanceMode::main);
    CHECK(second.mode() == InstanceMode::sender);
    CHECK(second.senderLane() == md::LaneId::chords);
    CHECK(third.senderLane() == md::LaneId::bass);
    CHECK(fourth.senderLane() == md::LaneId::melody);
    CHECK(second.senderLinked());
    CHECK(second.idea() == nullptr);
    CHECK(Link::instance().oldestMain() == &first);
}

TEST_CASE("a fifth instance sends bass when every lane has a sender")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor a;
    MagicDustProcessor b;
    MagicDustProcessor c;
    MagicDustProcessor extra;
    CHECK(extra.senderLane() == md::LaneId::bass);
    CHECK(Link::instance().sendersFor(md::LaneId::bass) == 2);
}

TEST_CASE("a bass sender plays exactly the main idea's bass on channel 2 and is silent")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor chords;
    MagicDustProcessor bass;
    bass.setSenderLane(md::LaneId::bass);
    FakeHead head;
    prepare(bass, head);

    constexpr int blocks = 187;
    const Run run = runBlocks(bass, head, blocks);

    const double endPpq = md::ppqForSamples(blocks * kBlock, 120.0, kRate);
    const auto expected = laneEventsUpTo(*main.idea(), kBassChannel, endPpq);
    CHECK_FALSE(expected.empty());
    CHECK(run.events == expected);
    CHECK(run.energy == 0.0);
}

TEST_CASE("a bass sender plays the main's pinned bass after the main sprinkles")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    auto* pin = main.params.getParameter("bassPin");
    pin->setValueNotifyingHost(1.0F);
    const auto pinned = main.idea()->lanes.at(1).events;
    MagicDustProcessor chords;
    MagicDustProcessor bass;
    bass.setSenderLane(md::LaneId::bass);
    main.sprinkle();
    FakeHead head;
    prepare(bass, head);

    constexpr int blocks = 187;
    const Run run = runBlocks(bass, head, blocks);

    const double endPpq = md::ppqForSamples(blocks * kBlock, 120.0, kRate);
    CHECK(main.idea()->lanes.at(1).pinned);
    CHECK(run.events == laneEventsUpTo(*main.idea(), kBassChannel, endPpq));
    CHECK_FALSE(run.events.empty());
    CHECK(main.idea()->lanes.at(1).events.size() == pinned.size());
}

TEST_CASE("a sender plays nothing while its main has that lane muted and resumes after")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor chords;
    MagicDustProcessor bass;
    bass.setSenderLane(md::LaneId::bass);
    // The idea is random per instance and a few have no bass note in the first eight beats, so pin
    // one whose bass plays in both halves of the run.
    setSeed(main, 1);
    FakeHead head;
    prepare(bass, head);
    prepare(main, head);
    auto* mute = main.params.getParameter("bassMute");
    const double halfPpq = md::ppqForSamples(100 * kBlock, 120.0, kRate);
    REQUIRE_FALSE(laneEventsUpTo(*main.idea(), kBassChannel, halfPpq).empty());
    REQUIRE(laneEventsUpTo(*main.idea(), kBassChannel, 2 * halfPpq).size() >
            laneEventsUpTo(*main.idea(), kBassChannel, halfPpq).size());

    mute->setValueNotifyingHost(1.0F);
    const double startPpq = head.ppq;
    // The main instance publishes its mutes from its own audio thread; run it first, as a host
    // does.
    juce::AudioBuffer<float> mainBuffer(2, kBlock);
    juce::MidiBuffer mainMidi;
    mainMidi.ensureSize(16384);
    main.processBlock(mainBuffer, mainMidi);
    head.ppq = startPpq;
    const Run quiet = runBlocks(bass, head, 100);
    CHECK(quiet.events.empty());

    mute->setValueNotifyingHost(0.0F);
    main.processBlock(mainBuffer, mainMidi);
    const Run resumed = runBlocks(bass, head, 100);
    CHECK_FALSE(resumed.events.empty());
}

TEST_CASE("a sender plays its lane at its main's octave and releases when that changes")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor chords;
    MagicDustProcessor bass;
    bass.setSenderLane(md::LaneId::bass);
    FakeHead head;
    prepare(bass, head);
    prepare(main, head);
    const auto setOctave = [&main](int octave)
    {
        auto* param = main.params.getParameter("bassOctave");
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(octave)));
        // The main instance publishes its octaves from its own audio thread.
        juce::AudioBuffer<float> buffer(2, kBlock);
        juce::MidiBuffer midi;
        midi.ensureSize(16384);
        main.processBlock(buffer, midi);
    };

    setOctave(-1);
    constexpr int blocks = 187;
    const Run run = runBlocks(bass, head, blocks);
    const double endPpq = md::ppqForSamples(blocks * kBlock, 120.0, kRate);
    auto expected = laneEventsUpTo(*main.idea(), kBassChannel, endPpq);
    REQUIRE_FALSE(expected.empty());
    for (Ev& e : expected)
    {
        e.note -= 12;
    }
    CHECK(run.events == expected);

    // Mid-note: back to 0 releases at the pitch that was sent.
    std::vector<Ev> played;
    while (sounding(played).empty())
    {
        const Run r = runBlocks(bass, head, 1);
        played.insert(played.end(), r.events.begin(), r.events.end());
        REQUIRE(head.ppq < 64.0);
    }
    const auto held = *sounding(played).begin();
    setOctave(0);
    const Run changed = runBlocks(bass, head, 1);
    CHECK(std::ranges::find(changed.events, Ev{held.first, held.second, false}) !=
          changed.events.end());
}

TEST_CASE("a sender ignores the internal clock and plays only with the host")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor sender;
    sender.prepareToPlay(kRate, kBlock);
    sender.play();
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer midi;
    for (int b = 0; b < 100; ++b)
    {
        sender.processBlock(buffer, midi);
        CHECK(midi.isEmpty());
    }
}

TEST_CASE("a bass sender mutes the main sub bass until it is destroyed")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    setSeed(main, 48213);
    FakeHead head;
    prepare(main, head);
    constexpr int blocks = 187;
    const double alone = runBlocks(main, head, blocks).energy;
    REQUIRE(alone > 0.0);

    double withSender = 0.0;
    {
        MagicDustProcessor bass;
        bass.setSenderLane(md::LaneId::bass);
        head.ppq = 0.0;
        withSender = runBlocks(main, head, blocks).energy;
    }
    head.ppq = 0.0;
    const double after = runBlocks(main, head, blocks).energy;

    CHECK(withSender > 0.0);
    CHECK(withSender < alone * 0.95);
    CHECK(after == Catch::Approx(alone).epsilon(0.01));
}

TEST_CASE("destroying the main instance releases a playing sender and then silences it")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    auto main = std::make_unique<MagicDustProcessor>();
    MagicDustProcessor sender;
    sender.setSenderLane(md::LaneId::bass);
    FakeHead head;
    prepare(sender, head);

    std::vector<Ev> played;
    for (int b = 0; b < 400 && sounding(played).empty(); ++b)
    {
        const Run run = runBlocks(sender, head, 1);
        played.insert(played.end(), run.events.begin(), run.events.end());
    }
    REQUIRE_FALSE(sounding(played).empty());

    main = nullptr;
    const Run after = runBlocks(sender, head, 1);
    CHECK_FALSE(after.events.empty());
    for (const Ev& e : after.events)
    {
        CHECK_FALSE(e.on);
    }
    played.insert(played.end(), after.events.begin(), after.events.end());
    CHECK(sounding(played).empty());
    CHECK(runBlocks(sender, head, 200).events.empty());
    CHECK_FALSE(sender.senderLinked());
}

TEST_CASE("a sender state round trip restores the mode and lane, even into a main instance")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    juce::MemoryBlock state;
    {
        MagicDustProcessor main;
        MagicDustProcessor sender;
        sender.setSenderLane(md::LaneId::melody);
        state = save(sender);
    }

    MagicDustProcessor restored;
    REQUIRE(restored.mode() == InstanceMode::main);
    const auto mainState = save(restored);
    REQUIRE(Link::instance().oldestMain() == &restored);
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));

    CHECK(restored.mode() == InstanceMode::sender);
    CHECK(restored.senderLane() == md::LaneId::melody);
    CHECK(Link::instance().oldestMain() == nullptr);
    CHECK(Link::instance().current(&restored) == nullptr);
    CHECK(Link::instance().sendersFor(md::LaneId::melody) == 1);

    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK(Link::instance().sendersFor(md::LaneId::melody) == 1);

    const auto saved = save(restored);
    const auto xml =
        juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    CHECK(xml->getStringAttribute("mode") == "sender");
    CHECK(xml->getIntAttribute("senderLane") == static_cast<int>(md::LaneId::melody));

    restored.setStateInformation(mainState.getData(), static_cast<int>(mainState.getSize()));
    CHECK(restored.mode() == InstanceMode::main);
    CHECK(Link::instance().sendersFor(md::LaneId::melody) == 0);
    CHECK(Link::instance().oldestMain() == &restored);
}

TEST_CASE("becomeMain makes a sender main with its own idea and leaves the oldest main alone")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor sender;
    REQUIRE(sender.mode() == InstanceMode::sender);
    REQUIRE(Link::instance().sendersFor(md::LaneId::chords) == 1);

    sender.becomeMain();

    CHECK(sender.mode() == InstanceMode::main);
    REQUIRE(sender.idea() != nullptr);
    CHECK(Link::instance().current(&sender) != nullptr);
    CHECK(Link::instance().current(&sender) != Link::instance().current(&main));
    CHECK(Link::instance().oldestMain() == &main);
    CHECK(Link::instance().sendersFor(md::LaneId::chords) == 0);
}

TEST_CASE("setSenderLane moves the sender count")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor sender;
    sender.setSenderLane(md::LaneId::bass);

    CHECK(Link::instance().sendersFor(md::LaneId::chords) == 0);
    CHECK(Link::instance().sendersFor(md::LaneId::bass) == 1);
}

TEST_CASE("a sender generates and publishes nothing")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor sender;
    sender.sprinkle();
    sender.setStyle(md::Style::mkDub);
    sender.setKeyRoot(3);

    CHECK(sender.idea() == nullptr);
    CHECK(Link::instance().current(&sender) == nullptr);
}

TEST_CASE("only the oldest main mutes the lanes that senders play")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    MagicDustProcessor main;
    MagicDustProcessor independent;
    independent.becomeMain();
    setSeed(independent, 48213);
    FakeHead head;
    prepare(independent, head);
    constexpr int blocks = 187;
    const double alone = runBlocks(independent, head, blocks).energy;
    REQUIRE(alone > 0.0);

    MagicDustProcessor bass;
    bass.setSenderLane(md::LaneId::bass);
    head.ppq = 0.0;
    const double withSender = runBlocks(independent, head, blocks).energy;

    CHECK(withSender == Catch::Approx(alone).epsilon(0.02));
}

TEST_CASE("a sender survives its main being destroyed and replaced while it plays")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    auto main = std::make_unique<MagicDustProcessor>();
    MagicDustProcessor sender;
    sender.setSenderLane(md::LaneId::bass);
    FakeHead head;
    prepare(sender, head);

    std::atomic<bool> stop{false};
    std::thread audio(
        [&]
        {
            juce::AudioBuffer<float> buffer(2, kBlock);
            juce::MidiBuffer midi;
            midi.ensureSize(16384);
            while (!stop.load())
            {
                sender.processBlock(buffer, midi);
                head.ppq += md::ppqForSamples(kBlock, 120.0, kRate);
            }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    main = nullptr;
    Link::instance().retire();
    main = std::make_unique<MagicDustProcessor>();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop.store(true);
    audio.join();
    // Only now, with no audio thread reading, is it safe to treat the grace period as over.
    Link::instance().retire(std::chrono::steady_clock::now() + std::chrono::seconds(2));

    std::set<int> bassNotes;
    for (const md::PlayEvent& e : md::makePattern(*main->idea()).events)
    {
        if (e.channel == kBassChannel)
        {
            bassNotes.insert(e.note);
        }
    }
    const Run run = runBlocks(sender, head, 400);
    int ons = 0;
    for (const Ev& e : run.events)
    {
        if (e.on)
        {
            ++ons;
            CHECK(bassNotes.contains(e.note));
        }
    }
    CHECK(ons > 0);
}
