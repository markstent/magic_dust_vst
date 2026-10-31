#include "plugin/HousePiano.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <span>

namespace md
{

namespace
{

// Overall gain, set so a 4-note chord at velocity 100 peaks about as high as the old organ did.
constexpr float kGain = 0.2F;
constexpr double kInharmonicity = 0.0004;
constexpr double kShimmerCents = 4.0;
constexpr float kShimmerAmp = 0.5F;
constexpr double kTau1Seconds = 1.8;
constexpr double kMiddleC = 261.6;
constexpr double kTauSpread = 0.35;
constexpr double kAttackMs = 1.0;
constexpr double kReleaseMs = 180.0;
constexpr double kHammerMs = 6.0;
constexpr double kHammerCutoffHz = 2000.0;
constexpr float kHammerPeak = 0.15F;
constexpr int kBrightFrom = 6;
constexpr double kBrightPower = 1.5;
constexpr double kGoldenRatio = 0.6180339887498949;
constexpr std::uint32_t kNoiseSeed = 0x9E3779B9U;
// A voice whose fundamental has fallen below this fraction of its start frees itself.
constexpr float kInaudible = 0.0005F;

// A xorshift generator, in [-1, 1).
float nextNoise(std::uint32_t& state)
{
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return (static_cast<float>(state) / 2147483648.0F) - 1.0F;
}

} // namespace

HousePiano::HousePiano()
{
    for (std::size_t i = 0; i < sineTable_.size(); ++i)
    {
        sineTable_.at(i) = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * static_cast<double>(i) / kSineTableSize));
    }
}

void HousePiano::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    voices_.fill({});
}

float HousePiano::sine(double phase) const
{
    const double x = phase * kSineTableSize;
    const auto i = static_cast<std::size_t>(x);
    const auto frac = static_cast<float>(x - static_cast<double>(i));
    return sineTable_.at(i) + (frac * (sineTable_.at(i + 1) - sineTable_.at(i)));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): interface fixed by the brief
void HousePiano::start(Voice& v, int note, int velocity) const
{
    const double f0 = juce::MidiMessage::getMidiNoteInHertz(note);
    const float vel = static_cast<float>(velocity) / 127.0F;
    const auto bright = static_cast<float>(std::pow(static_cast<double>(vel), kBrightPower));
    const double tau1 = kTau1Seconds * std::sqrt(kMiddleC / f0);
    const double shimmer = std::exp2(kShimmerCents / 1200.0);
    v.tail = 1.0F;
    v.partialCount = 0;
    float peak = 0.0F;
    for (int k = 1; k <= kPianoPartials; ++k)
    {
        const auto i = static_cast<std::size_t>(k - 1);
        const double fk = k * f0 * std::sqrt(1.0 + (kInharmonicity * k * k));
        const bool audible = fk * shimmer < sampleRate_ / 2;
        const float tone = 1.0F / static_cast<float>(std::pow(k, 0.9));
        v.gain.at(i) = audible ? kGain * vel * tone * (k > kBrightFrom ? bright : 1.0F) : 0.0F;
        v.decay.at(i) =
            static_cast<float>(std::exp(-(1.0 + (kTauSpread * (k - 1))) / (tau1 * sampleRate_)));
        v.partialCount += audible ? 1 : 0;
        peak += v.gain.at(i);
        // Fixed, spread-out start phases keep the attack from stacking into one spike.
        const double phase = std::fmod(k * kGoldenRatio, 1.0);
        v.phase.at(2 * i) = v.phase.at((2 * i) + 1) = phase;
        v.step.at(2 * i) = fk / shimmer / sampleRate_;
        v.step.at((2 * i) + 1) = fk * shimmer / sampleRate_;
    }
    v.hammerLeft = static_cast<int>(sampleRate_ * kHammerMs / 1000.0);
    v.hammerAmp = kHammerPeak * peak;
    v.hammerLow = 0.0F;
    // Seeded by the note, so the same note always gets the same hammer.
    v.noise = (kNoiseSeed ^ (static_cast<std::uint32_t>(note) * 0x01000193U) ^
               static_cast<std::uint32_t>(velocity)) |
              1U;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): interface fixed by the brief
void HousePiano::noteOn(int note, int velocity)
{
    if (velocity == 0)
    {
        noteOff(note);
        return;
    }
    // A retriggered note reuses its voice. Stealing prefers a releasing voice, then the oldest.
    auto* target = std::ranges::find_if(voices_, [note](const Voice& v)
                                        { return v.active && v.note == note; });
    if (target == voices_.end())
    {
        target = std::ranges::find_if(voices_, [](const Voice& v) { return !v.active; });
    }
    if (target == voices_.end())
    {
        target = std::ranges::min_element(
            voices_, [](const Voice& a, const Voice& b)
            { return a.releasing != b.releasing ? a.releasing : a.age < b.age; });
    }
    Voice& v = *target;
    if (!v.active)
    {
        v = {};
    }
    v.active = true;
    v.releasing = false;
    v.note = note;
    v.age = ++noteCounter_;
    start(v, note, velocity);
}

void HousePiano::release(Voice& v) const
{
    if (!v.active || v.releasing)
    {
        return;
    }
    v.releasing = true;
    v.releaseStep = v.env / std::max(1.0F, static_cast<float>(sampleRate_ * kReleaseMs / 1000.0));
}

void HousePiano::noteOff(int note)
{
    for (Voice& v : voices_)
    {
        if (v.note == note)
        {
            release(v);
        }
    }
}

void HousePiano::allNotesOff()
{
    for (Voice& v : voices_)
    {
        release(v);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): private helper, one call site
void HousePiano::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int start, int num,
                             float scale)
{
    const float attackStep =
        1.0F / std::max(1.0F, static_cast<float>(sampleRate_ * kAttackMs / 1000.0));
    const auto hammerLength = static_cast<float>(sampleRate_ * kHammerMs / 1000.0);
    const auto hammerCoeff =
        static_cast<float>(1.0 - std::exp(-2.0 * std::numbers::pi * kHammerCutoffHz / sampleRate_));
    const int channels = buffer.getNumChannels();
    const auto count = static_cast<std::size_t>(v.partialCount);
    for (int n = start; n < start + num; ++n)
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
        v.tail *= v.decay.front();
        if (v.tail < kInaudible)
        {
            v.active = false;
            return;
        }
        float sum = 0.0F;
        for (std::size_t k = 0; k < count; ++k)
        {
            float pair = 0.0F;
            for (const std::size_t o : {2 * k, (2 * k) + 1})
            {
                pair += sine(v.phase.at(o));
                v.phase.at(o) += v.step.at(o);
                v.phase.at(o) -= v.phase.at(o) >= 1.0 ? 1.0 : 0.0;
            }
            sum += v.gain.at(k) * pair * kShimmerAmp;
            v.gain.at(k) *= v.decay.at(k);
        }
        float hammer = 0.0F;
        if (v.hammerLeft > 0)
        {
            const float x = nextNoise(v.noise);
            v.hammerLow += hammerCoeff * (x - v.hammerLow);
            hammer =
                (x - v.hammerLow) * v.hammerAmp * static_cast<float>(v.hammerLeft) / hammerLength;
            --v.hammerLeft;
        }
        const float sample = scale * ((v.env * sum) + hammer);
        for (int c = 0; c < channels; ++c)
        {
            buffer.addSample(c, n, sample);
        }
    }
}

void HousePiano::render(juce::AudioBuffer<float>& buffer, int start, int num)
{
    const float scale = level();
    for (Voice& v : voices_)
    {
        if (v.active)
        {
            renderVoice(v, buffer, start, num, scale);
        }
    }
}

} // namespace md
