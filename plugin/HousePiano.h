#pragma once

#include <array>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "plugin/Instrument.h"

namespace md
{

inline constexpr int kPianoVoices = 8;
inline constexpr int kPianoPartials = 16;
// Each partial is two copies detuned by plus and minus a few cents.
inline constexpr int kPianoOscillators = 2 * kPianoPartials;
inline constexpr int kSineTableSize = 4096;

// An M1-style house piano made by additive synthesis: a bright hard attack, a body that thins
// quickly, a slight chorus shimmer and a noise burst for the hammer.
class HousePiano final : public Instrument
{
  public:
    HousePiano();

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
        float env = 0.0F;
        float releaseStep = 0.0F;
        // The fundamental's level since the note on, to free a voice that has died away.
        float tail = 1.0F;
        int partialCount = 0;
        int hammerLeft = 0;
        float hammerAmp = 0.0F;
        float hammerLow = 0.0F;
        std::uint32_t noise = 1;
        std::array<float, kPianoPartials> gain{};
        std::array<float, kPianoPartials> decay{};
        std::array<double, kPianoOscillators> phase{};
        std::array<double, kPianoOscillators> step{};
    };

    void start(Voice& v, int note, int velocity) const;
    void release(Voice& v) const;
    void renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int start, int num, float scale);
    float sine(double phase) const;

    double sampleRate_ = 48000.0;
    std::uint64_t noteCounter_ = 0;
    std::array<float, kSineTableSize + 1> sineTable_{};
    std::array<Voice, kPianoVoices> voices_{};
};

} // namespace md
