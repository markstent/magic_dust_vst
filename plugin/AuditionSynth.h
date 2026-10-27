#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

namespace md
{

// The most voices any recipe may ask for.
inline constexpr int kMaxAuditionVoices = 8;

// partials[k] is the amplitude of harmonic k + 1.
struct VoiceRecipe
{
    std::array<float, 8> partials;
    float attackMs;
    // The time for a held note to fall to 1/e of its peak; 0 sustains at full level.
    float decayMs;
    float releaseMs;
    float gain;
    int voices;
};

inline constexpr VoiceRecipe kOrgan{.partials = {1.0F, 0.5F, 0.25F, 0.125F, 0.0F, 0.0F, 0.0F, 0.0F},
                                    .attackMs = 5.0F,
                                    .decayMs = 0.0F,
                                    .releaseMs = 60.0F,
                                    .gain = 0.2F,
                                    .voices = 8};

inline constexpr VoiceRecipe kSubBass{.partials = {1.0F, 0.15F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
                                      .attackMs = 3.0F,
                                      .decayMs = 0.0F,
                                      .releaseMs = 40.0F,
                                      .gain = 0.3F,
                                      .voices = 1};

// A bright saw-like pluck: a fast attack, then a decay to silence even while the note is held.
inline constexpr VoiceRecipe kPluck{.partials = {1.0F, 0.5F, 0.33F, 0.25F, 0.2F, 0.0F, 0.0F, 0.0F},
                                    .attackMs = 2.0F,
                                    .decayMs = 220.0F,
                                    .releaseMs = 60.0F,
                                    .gain = 0.18F,
                                    .voices = 4};

static_assert(kOrgan.voices >= 1 && kOrgan.voices <= kMaxAuditionVoices);
static_assert(kSubBass.voices >= 1 && kSubBass.voices <= kMaxAuditionVoices);
static_assert(kPluck.voices >= 1 && kPluck.voices <= kMaxAuditionVoices);

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
    // A velocity of 0 is a note-off.
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();
    // Adds into the buffer; start and num must lie within it.
    void render(juce::AudioBuffer<float>& buffer, int start, int num);
    void setLevel(float level) { level_.store(std::clamp(level, 0.0F, 1.0F)); }

  private:
    struct Voice
    {
        bool active = false;
        bool releasing = false;
        bool decaying = false;
        int note = 0;
        std::uint64_t age = 0;
        double frequency = 0.0;
        double phase = 0.0;
        float velocity = 0.0F;
        float env = 0.0F;
        float releaseStep = 0.0F;
        std::size_t partialCount = 0;
    };

    void release(Voice& v) const;
    void renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int start, int num,
                     float amp) const;

    VoiceRecipe recipe_;
    std::atomic<float> level_{0.8F};
    double sampleRate_ = 48000.0;
    std::uint64_t noteCounter_ = 0;
    std::array<Voice, kMaxAuditionVoices> voices_{};
};

} // namespace md
