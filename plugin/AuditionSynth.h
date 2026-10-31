#pragma once

#include <array>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "plugin/Instrument.h"

namespace md
{

// The most voices any recipe may ask for.
inline constexpr int kMaxAuditionVoices = 8;

// partials[k] is the amplitude of harmonic k + 1.
struct VoiceRecipe
{
    std::array<float, 8> partials;
    float attackMs;
    float releaseMs;
    float gain;
    int voices;
};

inline constexpr VoiceRecipe kSubBass{.partials = {1.0F, 0.15F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F},
                                      .attackMs = 3.0F,
                                      .releaseMs = 40.0F,
                                      .gain = 0.3F,
                                      .voices = 1};

static_assert(kSubBass.voices >= 1 && kSubBass.voices <= kMaxAuditionVoices);

// The sub bass: a small additive synth driven by a recipe.
class AuditionSynth final : public Instrument
{
  public:
    explicit AuditionSynth(const VoiceRecipe& recipe);

    void prepare(double sampleRate) override;
    void noteOn(int note, int velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void render(juce::AudioBuffer<float>& buffer, int start, int num) override;

  private:
    struct Voice
    {
        bool active = false;
        bool releasing = false;
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
    double sampleRate_ = 48000.0;
    std::uint64_t noteCounter_ = 0;
    std::array<Voice, kMaxAuditionVoices> voices_{};
};

} // namespace md
