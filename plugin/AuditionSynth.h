#pragma once

#include <array>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

namespace md
{

// partials[k] is the amplitude of harmonic k + 1.
struct VoiceRecipe
{
    std::array<float, 8> partials;
    float attackMs;
    float releaseMs;
    float gain;
    int voices;
};

inline constexpr VoiceRecipe kOrgan{.partials = {1.0F, 0.5F, 0.25F, 0.125F, 0.0F, 0.0F, 0.0F, 0.0F},
                                    .attackMs = 5.0F,
                                    .releaseMs = 60.0F,
                                    .gain = 0.2F,
                                    .voices = 8};

inline constexpr VoiceRecipe kSubBass{.partials = {1.0F, 0.15F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
                                      .attackMs = 3.0F,
                                      .releaseMs = 40.0F,
                                      .gain = 0.3F,
                                      .voices = 1};

// A soft triangle: odd harmonics at 1 / n^2.
inline constexpr VoiceRecipe kSoftLead{
    .partials = {1.0F, 0.0F, 1.0F / 9, 0.0F, 1.0F / 25, 0.0F, 1.0F / 49, 0.0F},
    .attackMs = 10.0F,
    .releaseMs = 120.0F,
    .gain = 0.15F,
    .voices = 2};

// One built-in sound. Notes take effect at the next render call. noteOn, noteOff and render never
// allocate after prepare.
class AuditionSynth
{
  public:
    explicit AuditionSynth(const VoiceRecipe& recipe);
    AuditionSynth(const AuditionSynth&) = delete;
    AuditionSynth& operator=(const AuditionSynth&) = delete;
    AuditionSynth(AuditionSynth&&) = delete;
    AuditionSynth& operator=(AuditionSynth&&) = delete;
    ~AuditionSynth() = default;

    void prepare(double sampleRate);
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();
    // Adds into the buffer.
    void render(juce::AudioBuffer<float>& buffer, int start, int num);
    void setLevel(float level) { level_.store(level); }

  private:
    VoiceRecipe recipe_;
    std::atomic<float> level_{0.8F};
    juce::Synthesiser synth_;
    juce::MidiBuffer noMidi_;
};

} // namespace md
