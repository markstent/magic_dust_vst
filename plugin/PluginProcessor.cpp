#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <utility>

#include "IdeaState.h"
#include "Link.h"
#include "PluginEditor.h"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/validate.hpp"

namespace
{

constexpr int schemaVersion = 1;
constexpr juce::int64 kQuietMs = 150;
constexpr juce::int64 kFollowNoticeMs = 2000;
constexpr int kAuditionVelocity = 90;
constexpr std::size_t kMaxAuditionNotes = 7;
// A host position this far from where the last block ended is a locate or a loop.
constexpr double kLocateBeats = 0.05;

const char* const kBankFailed = "the style bank failed to load";
const char* const kNoIdea = "couldn't write that idea";

constexpr float kMaxGain = 2.0F; // +6 dB
constexpr float kUnityTravel = 0.75F;
constexpr float kMinusInfinityDb = -100.0F;

// Gain is stored linearly (1.0 is unity, as in saved projects) and shown to the host in dB.
std::unique_ptr<juce::AudioParameterFloat> gainParameter(const char* id, const juce::String& name,
                                                         float defaultGain)
{
    // The skew puts unity at 3/4 of the knob travel, so there is room to turn up as well as down.
    juce::NormalisableRange<float> range(0.0F, kMaxGain);
    range.skew = std::log(kUnityTravel) / std::log(0.5F);
    const auto attributes = juce::AudioParameterFloatAttributes()
                                .withStringFromValueFunction(
                                    [](float gain, int)
                                    {
                                        return juce::Decibels::toString(
                                            juce::Decibels::gainToDecibels(gain, kMinusInfinityDb),
                                            1, kMinusInfinityDb, true, "-inf");
                                    })
                                .withValueFromStringFunction(
                                    [](const juce::String& text)
                                    {
                                        return text.containsIgnoreCase("inf")
                                                   ? 0.0F
                                                   : juce::Decibels::decibelsToGain(
                                                         text.getFloatValue(), kMinusInfinityDb);
                                    });
    return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, range,
                                                       defaultGain, attributes);
}

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(
        std::make_unique<juce::AudioParameterInt>(ParameterID{"mood", 1}, "mood", 0, 100, 30));
    layout.add(
        std::make_unique<juce::AudioParameterInt>(ParameterID{"energy", 1}, "energy", 0, 100, 50));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"groove", 1}, "groove",
        juce::StringArray{"straight", "swing 54%", "uk shuffle 58%", "heavy 62%"}, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParameterID{"flavour", 1}, "flavour", juce::StringArray{"simple", "soulful", "jazzy"}, 1));
    layout.add(gainParameter("volume", "volume", 0.8F));
    for (const char* id : {"chordsLevel", "bassLevel", "melodyLevel"})
    {
        layout.add(gainParameter(id, juce::String(id).replace("Level", " level"), 1.0F));
    }
    for (const char* id : {"chordsMute", "bassMute", "melodyMute"})
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParameterID{id, 1}, juce::String(id).replace("Mute", " mute"), false));
    }
    for (const char* id : {"chordsPin", "bassPin", "melodyPin"})
    {
        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParameterID{id, 1}, juce::String(id).replace("Pin", " pin"), false));
    }
    for (const char* id : {"chordsOctave", "bassOctave", "melodyOctave"})
    {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParameterID{id, 1}, juce::String(id).replace("Octave", " octave"), md::kOctaveMin,
            md::kOctaveMax, 0));
    }
    return layout;
}

std::optional<md::Bank> tryLoadBank()
{
    try
    {
        return md::loadBundledBank();
    }
    catch (const md::BankError&)
    {
        return std::nullopt;
    }
}

bool sameFeel(const md::Settings& a, const md::Settings& b)
{
    return a.mood == b.mood && a.energy == b.energy && a.flavour == b.flavour;
}

std::optional<std::uint64_t> parseSeed(const juce::String& text)
{
    const std::string s = text.toStdString();
    std::uint64_t value = 0;
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic): from_chars takes a range
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || end != s.data() + s.size())
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    {
        return std::nullopt;
    }
    return value;
}

// An audition request packs the note count into the low byte, then one note per byte.
template <class F> void forEachAuditionNote(std::uint64_t packed, const F& f)
{
    const auto count = std::min<std::size_t>(packed & 0xFFU, kMaxAuditionNotes);
    for (std::size_t i = 0; i < count; ++i)
    {
        f(static_cast<int>((packed >> (8 * (i + 1))) & 0x7FU));
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void addMidi(juce::MidiBuffer& midi, bool on, int channel, int note, int velocity, int offset)
{
    const int status = (on ? 0x90 : 0x80) + channel;
    const std::array<std::uint8_t, 3> bytes = {static_cast<std::uint8_t>(status),
                                               static_cast<std::uint8_t>(note),
                                               static_cast<std::uint8_t>(velocity)};
    midi.addEvent(bytes.data(), 3, offset);
}

// A new sender takes the first lane nobody sends yet, or bass when all have a sender.
md::LaneId firstFreeLane(const Link& link)
{
    for (const md::LaneId lane : {md::LaneId::chords, md::LaneId::bass, md::LaneId::melody})
    {
        if (link.sendersFor(lane) == 0)
        {
            return lane;
        }
    }
    return md::LaneId::bass;
}

} // namespace

MagicDustProcessor::MagicDustProcessor() : MagicDustProcessor(tryLoadBank()) {}

MagicDustProcessor::MagicDustProcessor(std::optional<md::Bank> bank,
                                       std::optional<std::int64_t> randomSeed)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", makeLayout()),
      random_(randomSeed ? juce::Random(*randomSeed) : juce::Random()), bank_(std::move(bank)),
      volume_(params.getRawParameterValue("volume")),
      chordsLevel_(params.getRawParameterValue("chordsLevel")),
      bassLevel_(params.getRawParameterValue("bassLevel")),
      melodyLevel_(params.getRawParameterValue("melodyLevel")),
      mutes_{params.getRawParameterValue("chordsMute"), params.getRawParameterValue("bassMute"),
             params.getRawParameterValue("melodyMute")},
      octaves_{params.getRawParameterValue("chordsOctave"),
               params.getRawParameterValue("bassOctave"),
               params.getRawParameterValue("melodyOctave")},
      pins_{params.getRawParameterValue("chordsPin"), params.getRawParameterValue("bassPin"),
            params.getRawParameterValue("melodyPin")}
{
    Link& link = Link::instance();
    const bool sender = link.oldestMain() != nullptr;
    if (sender)
    {
        enterSender(firstFreeLane(link));
    }
    else
    {
        link.registerMain(this);
    }
    if (!bank_)
    {
        lastError_ = kBankFailed;
    }
    else if (!sender)
    {
        seed_ = static_cast<std::uint32_t>(random_.nextInt(md::kSeedMax + 1));
        laneSeeds_ = md::defaultLaneSeeds(seed_);
        regenerate();
    }
    startTimerHz(30);
}

MagicDustProcessor::~MagicDustProcessor()
{
    stopTimer();
    Link& link = Link::instance();
    link.remove(this);
    if (mode_.load() == InstanceMode::sender)
    {
        link.removeSender(senderLane_.load());
    }
}

void MagicDustProcessor::enterSender(md::LaneId lane)
{
    Link& link = Link::instance();
    if (mode_.load() == InstanceMode::sender)
    {
        link.removeSender(senderLane_.load());
    }
    else
    {
        link.remove(this);
        idea_.reset();
    }
    senderLane_.store(lane);
    mode_.store(InstanceMode::sender);
    link.addSender(lane);
    releasePending_.store(true);
}

void MagicDustProcessor::enterMain()
{
    if (mode_.load() == InstanceMode::main)
    {
        return;
    }
    Link& link = Link::instance();
    link.removeSender(senderLane_.load());
    link.registerMain(this);
    mode_.store(InstanceMode::main);
    releasePending_.store(true);
}

void MagicDustProcessor::setSenderLane(md::LaneId lane)
{
    if (mode_.load() == InstanceMode::sender && lane != senderLane_.load())
    {
        enterSender(lane);
    }
}

void MagicDustProcessor::becomeMain()
{
    if (mode_.load() == InstanceMode::sender)
    {
        enterMain();
        sprinkle();
    }
}

bool MagicDustProcessor::senderLinked() { return Link::instance().oldestMain() != nullptr; }

md::Settings MagicDustProcessor::currentSettings() const
{
    return md::Settings{
        .style = style_,
        .keyRoot = keyRoot_,
        .mood = static_cast<int>(params.getRawParameterValue("mood")->load()),
        .energy = static_cast<int>(params.getRawParameterValue("energy")->load()),
        .groove = static_cast<md::Groove>(params.getRawParameterValue("groove")->load()),
        .flavour = static_cast<md::Flavour>(params.getRawParameterValue("flavour")->load()),
        .bars = bars_};
}

bool MagicDustProcessor::regenerate(std::uint32_t seed, const md::LaneSeeds& laneSeeds,
                                    bool keepPins)
{
    if (!bank_ || mode_.load() != InstanceMode::main)
    {
        return false;
    }
    const md::Settings settings = currentSettings();
    generated_ = settings;
    seen_ = settings;
    try
    {
        adopt(idea_ && keepPins ? md::generate(*bank_, settings, seed, laneSeeds, *idea_, pins())
                                : md::generate(*bank_, settings, seed, laneSeeds));
    }
    catch (const std::exception&)
    {
        lastError_ = kNoIdea;
        return false;
    }
    return true;
}

// The seeds follow the idea, which keeps its own when lanes are pinned.
void MagicDustProcessor::adopt(md::Idea idea)
{
    idea_ = std::move(idea);
    seed_ = idea_->seed;
    laneSeeds_ = {.chords = idea_->lanes.at(0).seed,
                  .bass = idea_->lanes.at(1).seed,
                  .melody = idea_->lanes.at(2).seed};
    lastError_.reset();
    Link::instance().publish(this,
                             std::make_shared<const md::PlaybackPattern>(md::makePattern(*idea_)));
}

// A saved idea stands in for regenerating only when it is this state's idea (same seed, style, key
// and bars), still valid, and from an entry the bank has.
bool MagicDustProcessor::usableSaved(const md::Idea& saved, std::uint32_t seed) const
{
    if (!bank_)
    {
        return false;
    }
    const md::Settings& s = saved.settings;
    return saved.seed == seed && s.style == style_ && s.keyRoot == keyRoot_ && s.bars == bars_ &&
           md::validate(saved).empty() &&
           std::ranges::any_of(bank_->entries,
                               [&](const md::StyleEntry& e) { return e.id == saved.entryId; });
}

void MagicDustProcessor::sprinkle()
{
    const auto seed = static_cast<std::uint32_t>(random_.nextInt(md::kSeedMax + 1));
    regenerate(seed, md::defaultLaneSeeds(seed));
}

void MagicDustProcessor::setStyle(md::Style style)
{
    const md::Style old = std::exchange(style_, style);
    if (!regenerate())
    {
        style_ = old;
    }
}

void MagicDustProcessor::setKeyRoot(int keyRoot)
{
    const int old = std::exchange(keyRoot_, keyRoot);
    if (!regenerate())
    {
        keyRoot_ = old;
    }
}

std::optional<double> MagicDustProcessor::hostBpm() const
{
    const double bpm = hostBpm_.load();
    return bpm > 0.0 ? std::optional<double>(bpm) : std::nullopt;
}

void MagicDustProcessor::auditionChord(std::size_t slot)
{
    if (!idea_ || slot >= idea_->progression.size())
    {
        return;
    }
    // The chord sounds at the chords octave; a note shifted out of range is dropped.
    const int shift = octave(md::LaneId::chords) * md::kPitchClasses;
    std::uint64_t packed = 0;
    std::size_t count = 0;
    for (const int pitch : idea_->progression.at(slot).voicing)
    {
        const int note = pitch + shift;
        if (count < kMaxAuditionNotes && note >= md::kMidiPitchMin && note <= md::kMidiPitchMax)
        {
            ++count;
            packed |= static_cast<std::uint64_t>(note) << (8 * count);
        }
    }
    if (count > 0)
    {
        auditionRequest_.store(packed | count);
    }
}

void MagicDustProcessor::timerCallback() { tick(juce::Time::getMillisecondCounter()); }

void MagicDustProcessor::transportKey(Key key)
{
    if (hostPlaying_.load())
    {
        followUntilMs_ = juce::Time::getMillisecondCounter() + kFollowNoticeMs;
        return;
    }
    keyCommand_.store(key);
}

void MagicDustProcessor::tick(juce::int64 nowMs)
{
    if (followUntilMs_ != 0 && nowMs >= followUntilMs_)
    {
        followUntilMs_ = 0;
    }
    Link::instance().retire();
    if (!bank_ || mode_.load() != InstanceMode::main)
    {
        return;
    }
    const md::Settings cur = currentSettings();
    if (!sameFeel(cur, seen_))
    {
        seen_ = cur;
        changedAtMs_ = nowMs;
    }
    const bool quiet = nowMs - changedAtMs_ >= kQuietMs;
    if (cur.groove != generated_.groove || (!sameFeel(cur, generated_) && quiet))
    {
        regenerate();
    }
}

void MagicDustProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;
    auditionRequest_.store(0);
    auditionPacked_ = 0;
    releasePending_.store(true);
    organ_.prepare(sampleRate);
    subBass_.prepare(sampleRate);
    pluck_.prepare(sampleRate);
}

void MagicDustProcessor::releaseResources() { releasePending_.store(true); }

md::AuditionSynth& MagicDustProcessor::voiceFor(int channel)
{
    switch (channel)
    {
    case 0:
        return organ_;
    case 1:
        return subBass_;
    default:
        return pluck_;
    }
}

MagicDustProcessor::Transport MagicDustProcessor::readTransport()
{
    if (const auto* head = getPlayHead())
    {
        if (const auto pos = head->getPosition())
        {
            if (const auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
            {
                bpm_ = *bpm;
                hostBpm_.store(bpm_);
            }
            const auto ppq = pos->getPpqPosition();
            if (pos->getIsPlaying() && ppq.hasValue())
            {
                internal_.stop();
                return {.source = Source::host, .ppq = *ppq, .bpm = bpm_};
            }
        }
    }
    if (internal_.running())
    {
        return {.source = Source::internal, .ppq = internal_.ppq(), .bpm = bpm_};
    }
    return {.source = Source::none, .ppq = 0.0, .bpm = bpm_};
}

void MagicDustProcessor::releaseAllNotes(juce::MidiBuffer& midi)
{
    // A held note went out at its lane's octave, which cannot have changed since.
    sounding_.releaseAll([this, &midi](std::uint8_t channel, std::uint8_t note)
                         { addMidi(midi, false, channel, shiftedNote(channel, note), 0, 0); });
    organ_.allNotesOff();
    subBass_.allNotesOff();
    pluck_.allNotesOff();
}

void MagicDustProcessor::startAudition(double bpm)
{
    if (auditionPacked_ != 0 && auditionSamplesLeft_ <= 0)
    {
        endAudition();
    }
    const std::uint64_t request = auditionRequest_.exchange(0);
    if (request == 0)
    {
        return;
    }
    endAudition();
    auditionPacked_ = request;
    auditionSamplesLeft_ = static_cast<int>(60.0 / bpm * sampleRate_);
    forEachAuditionNote(request, [this](int note) { organ_.noteOn(note, kAuditionVelocity); });
}

void MagicDustProcessor::endAudition()
{
    forEachAuditionNote(auditionPacked_, [this](int note) { organ_.noteOff(note); });
    auditionPacked_ = 0;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int MagicDustProcessor::shiftedNote(unsigned lane, int note) const
{
    const int octave = lane < octave_.size() ? octave_.at(lane) : 0;
    return note + (octave * md::kPitchClasses);
}

// A note shifted past 0 or 127 is dropped, not clamped: clamping would sound it at the wrong pitch,
// doubling another note, and a byte above 127 is not a note at all. Its note-off is dropped with
// it, because only a sent note is held.
bool MagicDustProcessor::emitMidi(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset)
{
    const int note = shiftedNote(e.channel, e.note);
    const bool inRange = note >= md::kMidiPitchMin && note <= md::kMidiPitchMax;
    if (e.on ? (mutedMask_ & (1U << (e.channel % 16U))) != 0U || !inRange : !sounding_.holds(e))
    {
        return false;
    }
    sounding_.apply(e);
    addMidi(midi, e.on, e.channel, note, e.velocity, offset);
    return true;
}

void MagicDustProcessor::emitEvent(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): guarded
    if (!emitMidi(e, midi, offset) || (e.channel < laneSent_.size() && laneSent_[e.channel]))
    {
        return;
    }
    md::AuditionSynth& voice = voiceFor(e.channel);
    const int note = shiftedNote(e.channel, e.note);
    if (e.on)
    {
        voice.noteOn(note, e.velocity);
    }
    else
    {
        voice.noteOff(note);
    }
}

void MagicDustProcessor::renderVoices(juce::AudioBuffer<float>& buffer, int start, int num)
{
    organ_.render(buffer, start, num);
    subBass_.render(buffer, start, num);
    pluck_.render(buffer, start, num);
}

void MagicDustProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    // No pass-through: host notes must not reach the plugin's MIDI output.
    midi.clear();
    const int numSamples = buffer.getNumSamples();

    const float volume = volume_->load();
    organ_.setLevel(volume * chordsLevel_->load());
    subBass_.setLevel(volume * bassLevel_->load());
    pluck_.setLevel(volume * melodyLevel_->load());

    bool release = releasePending_.exchange(false);
    const Key key = keyCommand_.exchange(Key::none);
    if (key == Key::play)
    {
        internal_.start();
        release = true;
    }
    else if (key == Key::stop)
    {
        internal_.stop();
    }

    const Transport t = readTransport();
    const bool sender = mode_.load() == InstanceMode::sender;
    Link& link = Link::instance();
    const md::PlaybackPattern* pattern = link.current(sender ? link.oldestMain() : this);
    const bool located = t.source == Source::host && lastSource_ == Source::host &&
                         std::abs(t.ppq - expectedPpq_) > kLocateBeats;
    if (release || located || t.source != lastSource_ || pattern != lastPattern_)
    {
        releaseAllNotes(midi);
    }
    lastSource_ = t.source;
    lastPattern_ = pattern;
    updateMutes(midi, sender);
    updateOctaves(midi, sender);

    if (sender)
    {
        senderBlock(midi, numSamples, pattern, t);
    }
    else
    {
        mainBlock(buffer, midi, pattern, t);
    }

    internal_.advance(numSamples, t.bpm, sampleRate_);
    expectedPpq_ = t.ppq + md::ppqForSamples(numSamples, t.bpm, sampleRate_);
    previewPlaying_.store(t.source != Source::none);
    hostPlaying_.store(t.source == Source::host);
    previewPpq_.store(t.ppq);
}

// A muted lane plays nothing: lanes that just went quiet release at once, and emitMidi drops the
// note starts after that. A sender follows the mute state of the main instance it plays for.
void MagicDustProcessor::updateMutes(juce::MidiBuffer& midi, bool sender)
{
    Link& link = Link::instance();
    unsigned mask = 0;
    if (sender)
    {
        mask = link.mutes(link.oldestMain());
    }
    else
    {
        for (unsigned lane = 0; lane < 3; ++lane)
        {
            if (laneMuted(static_cast<md::LaneId>(lane)))
            {
                mask |= 1U << lane;
            }
        }
        link.setMutes(this, mask);
    }
    const unsigned fresh = mask & ~mutedMask_;
    mutedMask_ = mask;
    for (unsigned lane = 0; lane < 3; ++lane)
    {
        if ((fresh & (1U << lane)) != 0U)
        {
            releaseLane(midi, lane);
        }
    }
}

// Releases a lane's notes at the pitch they were sent at.
void MagicDustProcessor::releaseLane(juce::MidiBuffer& midi, unsigned lane)
{
    sounding_.releaseChannel(static_cast<std::uint8_t>(lane),
                             [this, &midi](std::uint8_t channel, std::uint8_t note)
                             { addMidi(midi, false, channel, shiftedNote(channel, note), 0, 0); });
    voiceFor(static_cast<int>(lane)).allNotesOff();
}

// A lane whose octave just changed releases at once, at the old octave, so no note sticks; the
// next note start uses the new one. A sender follows the octaves of the main instance it plays for.
void MagicDustProcessor::updateOctaves(juce::MidiBuffer& midi, bool sender)
{
    Link& link = Link::instance();
    for (unsigned lane = 0; lane < octave_.size(); ++lane)
    {
        const auto id = static_cast<md::LaneId>(lane);
        const int next = sender ? link.octave(link.oldestMain(), id) : octave(id);
        if (!sender)
        {
            link.setOctave(this, id, next);
        }
        if (next != octave_.at(lane))
        {
            releaseLane(midi, lane);
            octave_.at(lane) = next;
        }
    }
}

// A sender plays only its lane's notes on that lane's channel, with no sound of its own.
void MagicDustProcessor::senderBlock(juce::MidiBuffer& midi, int numSamples,
                                     const md::PlaybackPattern* pattern, const Transport& t)
{
    // The main instance's internal clock does not drive senders.
    if (pattern == nullptr || t.source != Source::host)
    {
        return;
    }
    const auto channel = static_cast<std::uint8_t>(senderLane_.load());
    md::schedule(*pattern, t.ppq, t.bpm, sampleRate_, numSamples,
                 [&](const md::ScheduledEvent& e)
                 {
                     if (e.event.channel == channel)
                     {
                         emitMidi(e.event, midi, e.sampleOffset);
                     }
                 });
}

// The oldest main's built-in sound of a lane goes quiet while a sender plays that lane.
void MagicDustProcessor::muteSentLanes()
{
    const Link& link = Link::instance();
    const bool oldest = link.oldestMain() == this;
    for (int lane = 0; lane < 3; ++lane)
    {
        const bool sent = oldest && link.sendersFor(static_cast<md::LaneId>(lane)) > 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): lane < 3
        if (sent && !laneSent_[static_cast<std::size_t>(lane)])
        {
            voiceFor(lane).allNotesOff();
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): lane < 3
        laneSent_[static_cast<std::size_t>(lane)] = sent;
    }
}

void MagicDustProcessor::mainBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi,
                                   const md::PlaybackPattern* pattern, const Transport& t)
{
    const int numSamples = buffer.getNumSamples();
    muteSentLanes();
    startAudition(t.bpm);
    // Each scheduled event splits the block, so every note starts on its own sample.
    int rendered = 0;
    const auto renderTo = [&](int end)
    {
        if (end > rendered)
        {
            renderVoices(buffer, rendered, end - rendered);
            rendered = end;
        }
    };
    if (pattern != nullptr && t.source != Source::none)
    {
        md::schedule(*pattern, t.ppq, t.bpm, sampleRate_, numSamples,
                     [&](const md::ScheduledEvent& e)
                     {
                         renderTo(e.sampleOffset);
                         emitEvent(e.event, midi, e.sampleOffset);
                     });
    }
    renderTo(numSamples);

    if (auditionPacked_ != 0)
    {
        auditionSamplesLeft_ = std::max(0, auditionSamplesLeft_ - numSamples);
    }
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

bool MagicDustProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainInputChannelSet().isDisabled();
}

void MagicDustProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("MagicDust");
    state.setProperty("schemaVersion", schemaVersion, nullptr);
    if (mode_.load() == InstanceMode::sender)
    {
        state.setProperty("mode", "sender", nullptr);
        state.setProperty("senderLane", static_cast<int>(senderLane_.load()), nullptr);
    }
    else
    {
        state.setProperty("mode", "main", nullptr);
        state.setProperty("seed", static_cast<int>(seed_), nullptr);
        state.setProperty("style", static_cast<int>(style_), nullptr);
        state.setProperty("keyRoot", keyRoot_, nullptr);
        state.setProperty("bars", bars_, nullptr);
        state.setProperty("chordsSeed", juce::String(laneSeeds_.chords), nullptr);
        state.setProperty("bassSeed", juce::String(laneSeeds_.bass), nullptr);
        state.setProperty("melodySeed", juce::String(laneSeeds_.melody), nullptr);
        state.addChild(params.copyState(), -1, nullptr);
        if (idea_)
        {
            state.addChild(ideaToTree(*idea_), -1, nullptr);
        }
    }
    if (const auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void MagicDustProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("MagicDust"))
    {
        return;
    }
    if (xml->getStringAttribute("mode") == "sender")
    {
        enterSender(
            static_cast<md::LaneId>(std::clamp(xml->getIntAttribute("senderLane", 1), 0, 2)));
        return;
    }
    enterMain();
    if (const auto* child = xml->getChildByName(params.state.getType()))
    {
        params.replaceState(juce::ValueTree::fromXml(*child));
    }
    else
    {
        for (auto* parameter : getParameters())
        {
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
    }
    const md::Settings defaults;
    const md::Style oldStyle = style_;
    const int oldKeyRoot = keyRoot_;
    const int oldBars = bars_;
    const int style = xml->getIntAttribute("style", static_cast<int>(defaults.style));
    style_ = style >= 0 && style <= static_cast<int>(md::Style::nyGroove)
                 ? static_cast<md::Style>(style)
                 : defaults.style;
    keyRoot_ = std::clamp(xml->getIntAttribute("keyRoot", defaults.keyRoot), 0, 11);
    const int bars = xml->getIntAttribute("bars", defaults.bars);
    bars_ = bars == 4 || bars == 8 || bars == 16 ? bars : defaults.bars;

    const auto seed = static_cast<std::uint32_t>(
        std::clamp(xml->getIntAttribute("seed", static_cast<int>(seed_)), 0, md::kSeedMax));
    const auto chords = parseSeed(xml->getStringAttribute("chordsSeed"));
    const auto bass = parseSeed(xml->getStringAttribute("bassSeed"));
    const auto melody = parseSeed(xml->getStringAttribute("melodySeed"));
    const md::LaneSeeds laneSeeds =
        chords && bass && melody
            ? md::LaneSeeds{.chords = *chords, .bass = *bass, .melody = *melody}
            : md::defaultLaneSeeds(seed);
    if (const auto* saved = xml->getChildByName("Idea"); saved != nullptr && bank_)
    {
        auto idea = ideaFromTree(juce::ValueTree::fromXml(*saved));
        if (idea && usableSaved(*idea, seed))
        {
            // Feel params that differ from the idea regenerate, pins kept, after the quiet spell.
            generated_ = idea->settings;
            seen_ = idea->settings;
            adopt(std::move(*idea));
            return;
        }
    }
    if (!regenerate(seed, laneSeeds, false))
    {
        style_ = oldStyle;
        keyRoot_ = oldKeyRoot;
        bars_ = oldBars;
    }
}

// Entry point JUCE's plugin wrappers call to create the processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MagicDustProcessor(); }
