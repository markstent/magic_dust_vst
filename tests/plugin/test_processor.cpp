#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "AllocationGuard.h"
#include "md/bank.hpp"
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

// Gives the processor a known idea: the state's seed is restored and the lane seeds derived.
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
