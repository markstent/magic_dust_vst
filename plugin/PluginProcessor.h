#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>

#include "md/generate.hpp"
#include "md/ranges.hpp"
#include "md/style.hpp"
#include "md/types.hpp"
#include "plugin/AuditionSynth.h"
#include "plugin/HousePiano.h"
#include "plugin/Instrument.h"
#include "plugin/LeadSynth.h"
#include "plugin/Playback.h"

enum class InstanceMode : std::uint8_t
{
    main,
    sender
};

class MagicDustProcessor : public juce::AudioProcessor, private juce::Timer
{
  public:
    MagicDustProcessor();
    // For tests: runs on the given bank instead of the bundled one; nullopt simulates a bank that
    // failed to load. A random seed makes the generated ideas repeatable.
    explicit MagicDustProcessor(std::optional<md::Bank> bank,
                                std::optional<std::int64_t> randomSeed = std::nullopt);
    MagicDustProcessor(const MagicDustProcessor&) = delete;
    MagicDustProcessor& operator=(const MagicDustProcessor&) = delete;
    MagicDustProcessor(MagicDustProcessor&&) = delete;
    MagicDustProcessor& operator=(MagicDustProcessor&&) = delete;
    ~MagicDustProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Message-thread API.
    [[nodiscard]] const md::Idea* idea() const { return idea_ ? &*idea_ : nullptr; }
    [[nodiscard]] std::optional<std::string> lastError() const { return lastError_; }
    void sprinkle();
    void setStyle(md::Style style);
    void setKeyRoot(int keyRoot);
    // While the host plays, these leave the preview alone and raise the follow notice instead.
    void play() { transportKey(Key::play); }
    void stop() { transportKey(Key::stop); }
    [[nodiscard]] bool hostPlaying() const { return hostPlaying_.load(); }
    // True for 2 s after a transport key was pressed while the host was playing.
    [[nodiscard]] bool followNotice() const { return followUntilMs_ != 0; }
    [[nodiscard]] bool previewPlaying() const { return previewPlaying_.load(); }
    [[nodiscard]] double previewPpq() const { return previewPpq_.load(); }
    [[nodiscard]] std::optional<double> hostBpm() const;
    void auditionChord(std::size_t slot);
    [[nodiscard]] InstanceMode mode() const { return mode_.load(); }
    [[nodiscard]] bool laneMuted(md::LaneId lane) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): 3 lanes
        return mutes_[static_cast<std::size_t>(lane)]->load() >= 0.5F;
    }
    [[nodiscard]] md::Pins pins() const
    {
        const auto on = [this](std::size_t lane) { return pins_.at(lane)->load() >= 0.5F; };
        return {.chords = on(0), .bass = on(1), .melody = on(2)};
    }
    // The part's octave step, -2 to 2.
    [[nodiscard]] int octave(md::LaneId lane) const
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): 3 lanes
        const auto raw = octaves_[static_cast<std::size_t>(lane)]->load();
        return std::clamp(static_cast<int>(std::lround(raw)), md::kOctaveMin, md::kOctaveMax);
    }
    [[nodiscard]] md::LaneId senderLane() const { return senderLane_.load(); }
    void setSenderLane(md::LaneId lane);
    // Turns a sender into a main instance with an idea of its own.
    void becomeMain();
    // True when a main instance exists for a sender to follow.
    [[nodiscard]] static bool senderLinked();
    // The 30 Hz timer's work, public so tests can run it with fake times.
    void tick(juce::int64 nowMs);

    // Public so the editor can attach controls to the parameters.
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    juce::AudioProcessorValueTreeState params;

  private:
    enum class Key : std::uint8_t
    {
        none,
        play,
        stop
    };
    enum class Source : std::uint8_t
    {
        none,
        host,
        internal
    };
    struct Transport
    {
        Source source;
        double ppq;
        double bpm;
    };

    void timerCallback() override;
    void transportKey(Key key);
    [[nodiscard]] md::Settings currentSettings() const;
    // Keeps the pinned lanes of the current idea, unless keepPins is false (a loaded state).
    bool regenerate(std::uint32_t seed, const md::LaneSeeds& laneSeeds, bool keepPins = true);
    bool regenerate() { return regenerate(seed_, laneSeeds_); }
    // Makes an idea the current one and publishes it.
    void adopt(md::Idea idea);
    [[nodiscard]] bool usableSaved(const md::Idea& saved, std::uint32_t seed) const;

    Transport readTransport();
    void releaseAllNotes(juce::MidiBuffer& midi);
    void enterMain();
    void enterSender(md::LaneId lane);
    void mainBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi,
                   const md::PlaybackPattern* pattern, const Transport& t);
    void senderBlock(juce::MidiBuffer& midi, int numSamples, const md::PlaybackPattern* pattern,
                     const Transport& t);
    void muteSentLanes();
    void updateMutes(juce::MidiBuffer& midi, bool sender);
    void updateOctaves(juce::MidiBuffer& midi, bool sender);
    void releaseLane(juce::MidiBuffer& midi, unsigned lane);
    // The pitch a lane's note goes out at: its octave applied, not yet range-checked.
    [[nodiscard]] int shiftedNote(unsigned lane, int note) const;
    void startAudition(double bpm);
    void endAudition();
    void renderVoices(juce::AudioBuffer<float>& buffer, int start, int num);
    bool emitMidi(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset);
    void emitEvent(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset);
    md::Instrument& voiceFor(int channel);

    juce::Random random_;
    std::optional<md::Bank> bank_;
    const std::atomic<float>* volume_;
    const std::atomic<float>* chordsLevel_;
    const std::atomic<float>* bassLevel_;
    const std::atomic<float>* melodyLevel_;
    std::array<const std::atomic<float>*, 3> mutes_;
    std::array<const std::atomic<float>*, 3> octaves_;
    std::array<const std::atomic<float>*, 3> pins_;
    std::atomic<InstanceMode> mode_{InstanceMode::main};
    std::atomic<md::LaneId> senderLane_{md::LaneId::chords};
    std::optional<md::Idea> idea_;
    std::optional<std::string> lastError_;
    std::uint32_t seed_ = 0;
    md::LaneSeeds laneSeeds_{};
    md::Style style_ = md::Settings{}.style;
    int keyRoot_ = md::Settings{}.keyRoot;
    int bars_ = md::Settings{}.bars;

    md::Settings generated_;
    md::Settings seen_;
    juce::int64 changedAtMs_ = 0;

    // Audio thread only, except the atomics.
    std::atomic<Key> keyCommand_{Key::none};
    std::atomic<std::uint64_t> auditionRequest_{0};
    std::atomic<bool> releasePending_{false};
    std::atomic<bool> previewPlaying_{false};
    std::atomic<bool> hostPlaying_{false};
    juce::int64 followUntilMs_ = 0; // message thread only; 0 when there is no notice
    std::atomic<double> previewPpq_{0.0};
    std::atomic<double> hostBpm_{0.0};
    md::HousePiano piano_;
    md::AuditionSynth subBass_{md::kSubBass};
    md::LeadSynth lead_;
    md::InternalClock internal_;
    md::SoundingNotes sounding_;
    const md::PlaybackPattern* lastPattern_ = nullptr;
    Source lastSource_ = Source::none;
    std::array<int, 3> octave_{};    // per lane, this block; a sender follows its main instance
    unsigned mutedMask_ = 0;         // lane bits, this block; a sender follows its main instance
    std::array<bool, 3> laneSent_{}; // a sender plays the lane, so the built-in sound is muted
    double expectedPpq_ = 0.0;
    double bpm_ = md::kDefaultTempo;
    double sampleRate_ = 48000.0;
    std::uint64_t auditionPacked_ = 0; // see auditionChord
    int auditionSamplesLeft_ = 0;
};
