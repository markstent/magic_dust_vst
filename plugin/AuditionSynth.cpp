#include "plugin/AuditionSynth.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

namespace md
{

AuditionSynth::AuditionSynth(const VoiceRecipe& recipe) : recipe_(recipe) {}

void AuditionSynth::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    voices_.fill({});
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): interface fixed by the brief
void AuditionSynth::noteOn(int note, int velocity)
{
    const auto pool = std::span(voices_).first(static_cast<std::size_t>(recipe_.voices));
    const auto free = std::ranges::find_if(pool, [](const Voice& v) { return !v.active; });
    Voice& v = free != pool.end()
                   ? *free
                   : *std::ranges::min_element(pool, [](const Voice& a, const Voice& b)
                                               { return a.age < b.age; });
    v = {};
    v.active = true;
    v.note = note;
    v.age = ++noteCounter_;
    v.frequency = juce::MidiMessage::getMidiNoteInHertz(note);
    v.velocity = static_cast<float>(velocity) / 127.0F;
    for (std::size_t k = 0; k < recipe_.partials.size(); ++k)
    {
        if (v.frequency * static_cast<double>(k + 1) < sampleRate_ / 2)
        {
            v.partialCount = k + 1;
        }
    }
}

void AuditionSynth::release(Voice& v) const
{
    if (!v.active || v.releasing)
    {
        return;
    }
    const auto releaseSamples =
        std::max(1.0F, static_cast<float>(sampleRate_ * recipe_.releaseMs / 1000.0));
    v.releasing = true;
    v.releaseStep = v.env / releaseSamples;
}

void AuditionSynth::noteOff(int note)
{
    for (Voice& v : voices_)
    {
        if (v.note == note)
        {
            release(v);
        }
    }
}

void AuditionSynth::allNotesOff()
{
    for (Voice& v : voices_)
    {
        release(v);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): private helper, one call site
void AuditionSynth::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int start, int num,
                                float amp) const
{
    const float attackStep =
        1.0F / std::max(1.0F, static_cast<float>(sampleRate_ * recipe_.attackMs / 1000.0));
    const double step = v.frequency / sampleRate_;
    const int channels = buffer.getNumChannels();
    for (int i = start; i < start + num; ++i)
    {
        if (v.releasing)
        {
            v.env -= v.releaseStep;
            if (v.env <= 0.0F)
            {
                v.active = false;
                return;
            }
        }
        else
        {
            v.env = std::min(1.0F, v.env + attackStep);
        }
        double sum = 0.0;
        double harmonic = 1.0;
        for (const float partial : std::span(recipe_.partials).first(v.partialCount))
        {
            sum += static_cast<double>(partial) *
                   std::sin(2.0 * std::numbers::pi * v.phase * harmonic);
            harmonic += 1.0;
        }
        const float sample = amp * v.env * static_cast<float>(sum);
        for (int c = 0; c < channels; ++c)
        {
            buffer.addSample(c, i, sample);
        }
        v.phase += step;
        v.phase -= std::floor(v.phase);
    }
}

void AuditionSynth::render(juce::AudioBuffer<float>& buffer, int start, int num)
{
    for (Voice& v : voices_)
    {
        if (v.active)
        {
            renderVoice(v, buffer, start, num, recipe_.gain * v.velocity * level_.load());
        }
    }
}

} // namespace md
