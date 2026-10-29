#pragma once

#include <array>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "plugin/Instrument.h"

namespace md
{

// A monophonic synth lead: two detuned saws and a sub square through a resonant low-pass, with
// glide between overlapping notes and a vibrato that fades in.
class LeadSynth final : public Instrument
{
  public:
    LeadSynth() = default;

    void prepare(double sampleRate) override;
    void noteOn(int note, int velocity) override;
    void noteOff(int note) override;
    void allNotesOff() override;
    void render(juce::AudioBuffer<float>& buffer, int start, int num) override;

  private:
    struct EnvelopeTimes
    {
        double attackMs;
        double decayMs;
        float sustain;
        double releaseMs;
    };

    static constexpr double kReleaseMs = 120.0;
    static constexpr EnvelopeTimes kAmpTimes{
        .attackMs = 4.0, .decayMs = 180.0, .sustain = 0.75F, .releaseMs = kReleaseMs};
    static constexpr EnvelopeTimes kFilterTimes{
        .attackMs = 2.0, .decayMs = 280.0, .sustain = 0.25F, .releaseMs = kReleaseMs};

    // Attack and decay are linear, the release falls linearly from wherever the envelope is.
    struct Envelope
    {
        enum class Stage : std::uint8_t
        {
            idle,
            attack,
            decay,
            sustain,
            release
        };

        Stage stage = Stage::idle;
        float level = 0.0F;
        float attackStep = 0.0F;
        float decayStep = 0.0F;
        float sustain = 0.0F;
        float releaseSamples = 1.0F;
        float releaseStep = 0.0F;

        void configure(const EnvelopeTimes& times, double sampleRate);
        void trigger() { stage = Stage::attack; }
        void release();
        float next();
    };

    float nextSample();

    double sampleRate_ = 48000.0;
    bool held_ = false;
    int note_ = 0;
    float velocity_ = 0.0F;
    // Pitch in semitones, gliding linearly to the target.
    double pitch_ = 0.0;
    double pitchStep_ = 0.0;
    int glideLeft_ = 0;
    int samplesSinceTrigger_ = 0;
    double lfoPhase_ = 0.0;
    std::array<double, 3> phase_{};
    // The state-variable filter's two integrators.
    double ic1_ = 0.0;
    double ic2_ = 0.0;
    Envelope amp_;
    Envelope filter_;
    int glideSamples_ = 0;
};

} // namespace md
