#include "plugin/LeadSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace md
{

namespace
{

// Gain, set so a held note at velocity 100 peaks about as high as the old pluck did.
constexpr float kGain = 0.11F;
constexpr double kDetuneCents = 7.0;
constexpr double kSubLevel = 0.25;
constexpr double kMiddleC = 261.6;
constexpr double kBaseCutoffHz = 700.0;
constexpr double kEnvAmountMinHz = 2500.0;
constexpr double kEnvAmountVelocityHz = 3500.0;
constexpr double kMaxCutoffOfNyquist = 0.45;
constexpr double kMinCutoffHz = 20.0;
// The damping of the state-variable filter, the inverse of a Q of 1.
constexpr double kDamping = 1.0;
constexpr double kGlideMs = 45.0;
constexpr double kVibratoHz = 5.5;
constexpr double kVibratoCents = 12.0;
constexpr double kVibratoDelayMs = 250.0;
constexpr double kVibratoFullMs = 600.0;

double polyBlep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - (t * t) - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (t * t) + t + t + 1.0;
    }
    return 0.0;
}

double saw(double t, double dt) { return (2.0 * t) - 1.0 - polyBlep(t, dt); }

double square(double t, double dt)
{
    const double other = t + 0.5 - (t >= 0.5 ? 1.0 : 0.0);
    return (t < 0.5 ? 1.0 : -1.0) + polyBlep(t, dt) - polyBlep(other, dt);
}

float samplesFor(double sampleRate, double ms)
{
    return std::max(1.0F, static_cast<float>(sampleRate * ms / 1000.0));
}

} // namespace

void LeadSynth::Envelope::configure(const EnvelopeTimes& times, double sampleRate)
{
    attackStep = 1.0F / samplesFor(sampleRate, times.attackMs);
    decayStep = (1.0F - times.sustain) / samplesFor(sampleRate, times.decayMs);
    sustain = times.sustain;
    releaseSamples = samplesFor(sampleRate, times.releaseMs);
}

void LeadSynth::Envelope::release()
{
    if (stage != Stage::idle && stage != Stage::release)
    {
        stage = Stage::release;
        releaseStep = level / releaseSamples;
    }
}

float LeadSynth::Envelope::next()
{
    switch (stage)
    {
    case Stage::attack:
        level += attackStep;
        if (level >= 1.0F)
        {
            level = 1.0F;
            stage = Stage::decay;
        }
        break;
    case Stage::decay:
        level -= decayStep;
        if (level <= sustain)
        {
            level = sustain;
            stage = Stage::sustain;
        }
        break;
    case Stage::release:
        level -= releaseStep;
        if (level <= 0.0F)
        {
            level = 0.0F;
            stage = Stage::idle;
        }
        break;
    case Stage::sustain:
    case Stage::idle:
        break;
    }
    return level;
}

void LeadSynth::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    glideSamples_ = static_cast<int>(samplesFor(sampleRate_, kGlideMs));
    held_ = false;
    note_ = 0;
    velocity_ = 0.0F;
    pitch_ = 0.0;
    pitchStep_ = 0.0;
    glideLeft_ = 0;
    samplesSinceTrigger_ = 0;
    lfoPhase_ = 0.0;
    phase_.fill(0.0);
    ic1_ = ic2_ = 0.0;
    amp_ = {};
    filter_ = {};
    amp_.configure(kAmpTimes, sampleRate_);
    filter_.configure(kFilterTimes, sampleRate_);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): interface fixed by the brief
void LeadSynth::noteOn(int note, int velocity)
{
    if (velocity == 0)
    {
        noteOff(note);
        return;
    }
    if (held_ && note != note_)
    {
        // Legato: glide to the new pitch, keep the envelopes and the velocity.
        glideLeft_ = glideSamples_;
        pitchStep_ = (static_cast<double>(note) - pitch_) / glideSamples_;
    }
    else
    {
        if (amp_.stage == Envelope::Stage::idle)
        {
            ic1_ = ic2_ = 0.0;
        }
        pitch_ = note;
        glideLeft_ = 0;
        velocity_ = static_cast<float>(velocity) / 127.0F;
        samplesSinceTrigger_ = 0;
        lfoPhase_ = 0.0;
        amp_.trigger();
        filter_.trigger();
    }
    held_ = true;
    note_ = note;
}

void LeadSynth::noteOff(int note)
{
    if (held_ && note == note_)
    {
        held_ = false;
        amp_.release();
        filter_.release();
    }
}

void LeadSynth::allNotesOff()
{
    held_ = false;
    amp_.release();
    filter_.release();
}

float LeadSynth::nextSample()
{
    const float ampLevel = amp_.next();
    const float filterLevel = filter_.next();
    if (amp_.stage == Envelope::Stage::idle)
    {
        return 0.0F;
    }
    if (glideLeft_ > 0)
    {
        pitch_ += pitchStep_;
        if (--glideLeft_ == 0)
        {
            pitch_ = note_;
        }
    }
    const double sinceMs = 1000.0 * samplesSinceTrigger_++ / sampleRate_;
    const double fade =
        std::clamp((sinceMs - kVibratoDelayMs) / (kVibratoFullMs - kVibratoDelayMs), 0.0, 1.0);
    const double vibrato =
        fade * kVibratoCents / 100.0 * std::sin(2.0 * std::numbers::pi * lfoPhase_);
    lfoPhase_ += kVibratoHz / sampleRate_;
    lfoPhase_ -= lfoPhase_ >= 1.0 ? 1.0 : 0.0;

    const double f0 =
        juce::MidiMessage::getMidiNoteInHertz(0) * std::exp2((pitch_ + vibrato) / 12.0);
    const double detune = std::exp2(kDetuneCents / 1200.0);
    const std::array<double, 3> freq{f0 / detune, f0 * detune, f0 / 2.0};
    double mix = 0.0;
    for (std::size_t i = 0; i < freq.size(); ++i)
    {
        const double dt = freq.at(i) / sampleRate_;
        mix += i < 2 ? saw(phase_.at(i), dt) : kSubLevel * square(phase_.at(i), dt);
        phase_.at(i) += dt;
        phase_.at(i) -= phase_.at(i) >= 1.0 ? 1.0 : 0.0;
    }

    const double amount = kEnvAmountMinHz + (kEnvAmountVelocityHz * velocity_);
    const double cutoff =
        std::clamp((kBaseCutoffHz * std::sqrt(f0 / kMiddleC)) + (amount * filterLevel),
                   kMinCutoffHz, kMaxCutoffOfNyquist * sampleRate_ / 2.0);
    // A topology-preserving transform state-variable filter, low-pass output.
    const double g = std::tan(std::numbers::pi * cutoff / sampleRate_);
    const double a1 = 1.0 / (1.0 + (g * (kDamping + g)));
    const double a2 = g * a1;
    const double v3 = mix - ic2_;
    const double v1 = (a1 * ic1_) + (a2 * v3);
    const double v2 = ic2_ + (g * v1);
    ic1_ = (2.0 * v1) - ic1_;
    ic2_ = (2.0 * v2) - ic2_;
    return static_cast<float>(v2) * ampLevel * velocity_;
}

void LeadSynth::render(juce::AudioBuffer<float>& buffer, int start, int num)
{
    if (amp_.stage == Envelope::Stage::idle)
    {
        return;
    }
    const float scale = kGain * level();
    const int channels = buffer.getNumChannels();
    for (int n = start; n < start + num; ++n)
    {
        const float sample = scale * nextSample();
        for (int c = 0; c < channels; ++c)
        {
            buffer.addSample(c, n, sample);
        }
    }
}

} // namespace md
