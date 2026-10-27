#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>

#include "md/generate.hpp"
#include "md/style.hpp"
#include "md/types.hpp"
#include "plugin/AuditionSynth.h"
#include "plugin/Playback.h"

class MagicDustProcessor : public juce::AudioProcessor, private juce::Timer
{
  public:
    MagicDustProcessor();
    // For tests: runs on the given bank instead of the bundled one; nullopt simulates a bank that
    // failed to load.
    explicit MagicDustProcessor(std::optional<md::Bank> bank);
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
    void play() { keyCommand_.store(Key::play); }
    void stop() { keyCommand_.store(Key::stop); }
    [[nodiscard]] bool previewPlaying() const { return previewPlaying_.load(); }
    [[nodiscard]] double previewPpq() const { return previewPpq_.load(); }
    [[nodiscard]] std::optional<double> hostBpm() const;
    void auditionChord(std::size_t slot);
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
    [[nodiscard]] md::Settings currentSettings() const;
    bool regenerate(std::uint32_t seed, const md::LaneSeeds& laneSeeds);
    bool regenerate() { return regenerate(seed_, laneSeeds_); }

    Transport readTransport();
    void releaseAllNotes(juce::MidiBuffer& midi);
    void startAudition(double bpm);
    void endAudition();
    void renderVoices(juce::AudioBuffer<float>& buffer, int start, int num);
    void emitEvent(const md::PlayEvent& e, juce::MidiBuffer& midi, int offset);
    md::AuditionSynth& voiceFor(int channel);

    std::optional<md::Bank> bank_;
    const std::atomic<float>* volume_;
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
    std::atomic<double> previewPpq_{0.0};
    std::atomic<double> hostBpm_{0.0};
    md::AuditionSynth organ_{md::kOrgan};
    md::AuditionSynth subBass_{md::kSubBass};
    md::AuditionSynth softLead_{md::kSoftLead};
    md::InternalClock internal_;
    md::SoundingNotes sounding_;
    const md::PlaybackPattern* lastPattern_ = nullptr;
    Source lastSource_ = Source::none;
    double expectedPpq_ = 0.0;
    double bpm_ = 120.0;
    double sampleRate_ = 48000.0;
    std::uint64_t auditionPacked_ = 0; // see auditionChord
    int auditionSamplesLeft_ = 0;
};
