#include "plugin/AuditionSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

namespace md
{

namespace
{

constexpr int kChannel = 1;

class Sound : public juce::SynthesiserSound
{
  public:
    bool appliesToNote([[maybe_unused]] int midiNoteNumber) override { return true; }
    bool appliesToChannel([[maybe_unused]] int midiChannel) override { return true; }
};

class Voice : public juce::SynthesiserVoice
{
  public:
    Voice(const VoiceRecipe& recipe, const std::atomic<float>& level)
        : recipe_(recipe), level_(&level)
    {
    }

    bool canPlaySound(juce::SynthesiserSound* s) override
    {
        return dynamic_cast<Sound*>(s) != nullptr;
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): fixed by juce::SynthesiserVoice
    void startNote(int note, float velocity, [[maybe_unused]] juce::SynthesiserSound* sound,
                   [[maybe_unused]] int currentPitchWheelPosition) override
    {
        const double rate = getSampleRate();
        frequency_ = juce::MidiMessage::getMidiNoteInHertz(note);
        phase_ = 0.0;
        env_ = 0.0F;
        releasing_ = false;
        velocity_ = velocity;
        attackStep_ = 1.0F / std::max(1.0F, static_cast<float>(rate * recipe_.attackMs / 1000.0));
        releaseSamples_ = std::max(1.0F, static_cast<float>(rate * recipe_.releaseMs / 1000.0));
        partialCount_ = 0;
        for (std::size_t k = 0; k < recipe_.partials.size(); ++k)
        {
            if (frequency_ * static_cast<double>(k + 1) < rate / 2)
            {
                partialCount_ = k + 1;
            }
        }
    }

    void stopNote([[maybe_unused]] float velocity, bool allowTailOff) override
    {
        if (!allowTailOff || env_ <= 0.0F)
        {
            clearCurrentNote();
            return;
        }
        releasing_ = true;
        releaseStep_ = env_ / releaseSamples_;
    }

    void pitchWheelMoved([[maybe_unused]] int newPitchWheelValue) override {}
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): fixed by juce::SynthesiserVoice
    void controllerMoved([[maybe_unused]] int controllerNumber,
                         [[maybe_unused]] int newControllerValue) override
    {
    }

    void renderNextBlock(juce::AudioBuffer<float>& buffer, int start, int num) override
    {
        const float amp = recipe_.gain * velocity_ * level_->load();
        const double step = frequency_ / getSampleRate();
        const int channels = buffer.getNumChannels();
        for (int i = start; i < start + num; ++i)
        {
            if (releasing_)
            {
                env_ -= releaseStep_;
                if (env_ <= 0.0F)
                {
                    clearCurrentNote();
                    return;
                }
            }
            else
            {
                env_ = std::min(1.0F, env_ + attackStep_);
            }
            double sum = 0.0;
            double harmonic = 1.0;
            for (const float partial : std::span(recipe_.partials).first(partialCount_))
            {
                sum += static_cast<double>(partial) *
                       std::sin(2.0 * std::numbers::pi * phase_ * harmonic);
                harmonic += 1.0;
            }
            const float sample = amp * env_ * static_cast<float>(sum);
            for (int c = 0; c < channels; ++c)
            {
                buffer.addSample(c, i, sample);
            }
            phase_ += step;
            phase_ -= std::floor(phase_);
        }
    }

  private:
    VoiceRecipe recipe_;
    const std::atomic<float>* level_;
    double frequency_ = 0.0;
    double phase_ = 0.0;
    float velocity_ = 0.0F;
    float env_ = 0.0F;
    float attackStep_ = 1.0F;
    float releaseStep_ = 0.0F;
    float releaseSamples_ = 1.0F;
    bool releasing_ = false;
    std::size_t partialCount_ = 0;
};

} // namespace

AuditionSynth::AuditionSynth(const VoiceRecipe& recipe) : recipe_(recipe)
{
    synth_.addSound(new Sound());
    for (int i = 0; i < recipe_.voices; ++i)
    {
        synth_.addVoice(new Voice(recipe_, level_));
    }
    synth_.setNoteStealingEnabled(true);
}

void AuditionSynth::prepare(double sampleRate) { synth_.setCurrentPlaybackSampleRate(sampleRate); }

void AuditionSynth::noteOn(int note, int velocity)
{
    synth_.noteOn(kChannel, note, static_cast<float>(velocity) / 127.0F);
}

void AuditionSynth::noteOff(int note) { synth_.noteOff(kChannel, note, 0.0F, true); }

void AuditionSynth::allNotesOff() { synth_.allNotesOff(kChannel, true); }

void AuditionSynth::render(juce::AudioBuffer<float>& buffer, int start, int num)
{
    synth_.renderNextBlock(buffer, noMidi_, start, num);
}

} // namespace md
