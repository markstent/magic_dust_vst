#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <utility>

#include "Link.h"
#include "PluginEditor.h"
#include "md/bank.hpp"
#include "md/ranges.hpp"

namespace
{

constexpr int schemaVersion = 1;
constexpr juce::int64 kQuietMs = 150;
constexpr int kAuditionVelocity = 90;
constexpr std::size_t kMaxAuditionNotes = 7;
// A host position this far from where the last block ended is a locate or a loop.
constexpr double kLocateBeats = 0.05;

const char* const kBankFailed = "the style bank failed to load";
const char* const kNoIdea = "couldn't write that idea";

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
    layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterID{"volume", 1}, "volume", 0.0F,
                                                           1.0F, 0.8F));
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

} // namespace

MagicDustProcessor::MagicDustProcessor() : MagicDustProcessor(tryLoadBank()) {}

MagicDustProcessor::MagicDustProcessor(std::optional<md::Bank> bank)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "params", makeLayout()), bank_(std::move(bank)),
      volume_(params.getRawParameterValue("volume"))
{
    if (!bank_)
    {
        lastError_ = kBankFailed;
    }
    else
    {
        seed_ =
            static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt(md::kSeedMax + 1));
        laneSeeds_ = md::defaultLaneSeeds(seed_);
        regenerate();
    }
    startTimerHz(30);
}

MagicDustProcessor::~MagicDustProcessor()
{
    stopTimer();
    Link::instance().remove(this);
}

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

bool MagicDustProcessor::regenerate(std::uint32_t seed, const md::LaneSeeds& laneSeeds)
{
    if (!bank_)
    {
        return false;
    }
    const md::Settings settings = currentSettings();
    generated_ = settings;
    seen_ = settings;
    try
    {
        idea_ = md::generate(*bank_, settings, seed, laneSeeds);
    }
    catch (const std::exception&)
    {
        lastError_ = kNoIdea;
        return false;
    }
    seed_ = seed;
    laneSeeds_ = laneSeeds;
    lastError_.reset();
    Link::instance().publish(this,
                             std::make_shared<const md::PlaybackPattern>(md::makePattern(*idea_)));
    return true;
}

void MagicDustProcessor::sprinkle()
{
    const auto seed =
        static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt(md::kSeedMax + 1));
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
    const md::Voicing& voicing = idea_->progression.at(slot).voicing;
    const std::size_t count = std::min(voicing.size(), kMaxAuditionNotes);
    std::uint64_t packed = count;
    for (std::size_t i = 0; i < count; ++i)
    {
        packed |= (static_cast<std::uint64_t>(voicing.at(i)) & 0x7FU) << (8 * (i + 1));
    }
    auditionRequest_.store(packed);
}

void MagicDustProcessor::timerCallback() { tick(juce::Time::getMillisecondCounter()); }

void MagicDustProcessor::tick(juce::int64 nowMs)
{
    Link::instance().retire();
    if (!bank_)
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
    softLead_.prepare(sampleRate);
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
        return softLead_;
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
    sounding_.releaseAll([&midi](std::uint8_t channel, std::uint8_t note)
                         { addMidi(midi, false, channel, note, 0, 0); });
    organ_.allNotesOff();
    subBass_.allNotesOff();
    softLead_.allNotesOff();
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

void MagicDustProcessor::emitEvent(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset)
{
    if (!e.on && !sounding_.holds(e))
    {
        return;
    }
    sounding_.apply(e);
    addMidi(midi, e.on, e.channel, e.note, e.velocity, offset);
    md::AuditionSynth& voice = voiceFor(e.channel);
    if (e.on)
    {
        voice.noteOn(e.note, e.velocity);
    }
    else
    {
        voice.noteOff(e.note);
    }
}

void MagicDustProcessor::renderVoices(juce::AudioBuffer<float>& buffer, int start, int num)
{
    organ_.render(buffer, start, num);
    subBass_.render(buffer, start, num);
    softLead_.render(buffer, start, num);
}

void MagicDustProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    // No pass-through: host notes must not reach the plugin's MIDI output.
    midi.clear();
    const int numSamples = buffer.getNumSamples();

    const float volume = volume_->load();
    organ_.setLevel(volume);
    subBass_.setLevel(volume);
    softLead_.setLevel(volume);

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
    const md::PlaybackPattern* pattern = Link::instance().current(this);
    const bool located = t.source == Source::host && lastSource_ == Source::host &&
                         std::abs(t.ppq - expectedPpq_) > kLocateBeats;
    if (release || located || t.source != lastSource_ || pattern != lastPattern_)
    {
        releaseAllNotes(midi);
    }
    lastSource_ = t.source;
    lastPattern_ = pattern;

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
    internal_.advance(numSamples, t.bpm, sampleRate_);
    expectedPpq_ = t.ppq + md::ppqForSamples(numSamples, t.bpm, sampleRate_);
    previewPlaying_.store(t.source != Source::none);
    previewPpq_.store(t.ppq);
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
    state.setProperty("seed", static_cast<int>(seed_), nullptr);
    state.setProperty("style", static_cast<int>(style_), nullptr);
    state.setProperty("keyRoot", keyRoot_, nullptr);
    state.setProperty("bars", bars_, nullptr);
    state.setProperty("chordsSeed", juce::String(laneSeeds_.chords), nullptr);
    state.setProperty("bassSeed", juce::String(laneSeeds_.bass), nullptr);
    state.setProperty("melodySeed", juce::String(laneSeeds_.melody), nullptr);
    state.setProperty("mode", "main", nullptr);
    state.addChild(params.copyState(), -1, nullptr);
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
    if (!regenerate(seed, laneSeeds))
    {
        style_ = oldStyle;
        keyRoot_ = oldKeyRoot;
        bars_ = oldBars;
    }
}

// Entry point JUCE's plugin wrappers call to create the processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MagicDustProcessor(); }
