#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "plugin/AuditionSynth.h"

namespace
{

constexpr double kRate = 48000.0;

float expectedRms(const md::VoiceRecipe& r)
{
    float sum = 0.0F;
    for (const float p : r.partials)
    {
        sum += p * p;
    }
    return r.gain * std::sqrt(sum / 2.0F);
}

double rms(const juce::AudioBuffer<float>& b, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
    {
        sum += static_cast<double>(b.getSample(0, i)) * static_cast<double>(b.getSample(0, i));
    }
    return std::sqrt(sum / (to - from));
}

bool allZero(const juce::AudioBuffer<float>& b)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        if (b.getSample(0, i) != 0.0F || b.getSample(1, i) != 0.0F)
        {
            return false;
        }
    }
    return true;
}

int releaseSamples(const md::VoiceRecipe& r)
{
    return static_cast<int>(kRate * r.releaseMs / 1000.0) + 2;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double maxJump(const juce::AudioBuffer<float>& b, int from, int to)
{
    double most = 0.0;
    for (int i = from + 1; i < to; ++i)
    {
        most = std::max(most,
                        static_cast<double>(std::abs(b.getSample(0, i) - b.getSample(0, i - 1))));
    }
    return most;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double steadyRms(const md::VoiceRecipe& r, float level, int velocity = 127, double rate = kRate,
                 int note = 69)
{
    md::AuditionSynth synth(r);
    synth.prepare(rate);
    synth.setLevel(level);
    juce::AudioBuffer<float> buf(2, 9600);
    buf.clear();
    synth.noteOn(note, velocity);
    synth.render(buf, 0, 9600);
    return rms(buf, 4800, 9600);
}

} // namespace

TEST_CASE("audition voices", "[audition]")
{
    for (const md::VoiceRecipe* r : {&md::kOrgan, &md::kSubBass, &md::kSoftLead})
    {
        SECTION("silent with no notes")
        {
            md::AuditionSynth synth(*r);
            synth.prepare(kRate);
            juce::AudioBuffer<float> buf(2, 512);
            buf.clear();
            synth.render(buf, 0, 512);
            CHECK(allZero(buf));
        }

        SECTION("steady state level, and level scales it")
        {
            const double full = steadyRms(*r, 1.0F);
            CHECK(full == Catch::Approx(expectedRms(*r)).epsilon(0.02));
            CHECK(steadyRms(*r, 0.5F) == Catch::Approx(full * 0.5).epsilon(0.02));
        }

        SECTION("exactly zero after the release")
        {
            md::AuditionSynth synth(*r);
            synth.prepare(kRate);
            juce::AudioBuffer<float> buf(2, 4800);
            buf.clear();
            synth.noteOn(69, 127);
            synth.render(buf, 0, 4800);
            synth.noteOff(69);
            buf.setSize(2, releaseSamples(*r) + 64);
            buf.clear();
            synth.render(buf, 0, releaseSamples(*r));
            buf.clear();
            synth.render(buf, 0, 64);
            CHECK(allZero(buf));
        }

        SECTION("allNotesOff silences within the release time")
        {
            md::AuditionSynth synth(*r);
            synth.prepare(kRate);
            juce::AudioBuffer<float> buf(2, 4800);
            buf.clear();
            synth.noteOn(69, 127);
            synth.render(buf, 0, 4800);
            synth.allNotesOff();
            buf.setSize(2, releaseSamples(*r) + 64);
            buf.clear();
            synth.render(buf, 0, releaseSamples(*r));
            buf.clear();
            synth.render(buf, 0, 64);
            CHECK(allZero(buf));
        }
    }
}

TEST_CASE("sub bass is mono and steals without a click", "[audition]")
{
    md::AuditionSynth synth(md::kSubBass);
    synth.prepare(kRate);
    synth.setLevel(1.0F);
    juce::AudioBuffer<float> buf(2, 14400);
    buf.clear();
    synth.noteOn(33, 127);
    synth.render(buf, 0, 4800);
    synth.noteOn(45, 127);
    synth.render(buf, 4800, 4800);
    CHECK(maxJump(buf, 4800 - 16, 4800 + 16) <= 2.0 * maxJump(buf, 2400, 4800));
    synth.noteOff(33);
    synth.render(buf, 9600, 4800);
    CHECK(rms(buf, 9600, 14400) == Catch::Approx(expectedRms(md::kSubBass)).epsilon(0.02));
}

TEST_CASE("organ caps at eight voices", "[audition]")
{
    md::AuditionSynth synth(md::kOrgan);
    synth.prepare(kRate);
    synth.setLevel(1.0F);
    juce::AudioBuffer<float> buf(2, 28800);
    buf.clear();
    for (const int note : {40, 45, 50, 55, 60, 65, 70, 75, 80})
    {
        synth.noteOn(note, 127);
    }
    synth.render(buf, 0, 28800);
    CHECK(rms(buf, 4800, 28800) ==
          Catch::Approx(std::sqrt(8.0) * expectedRms(md::kOrgan)).epsilon(0.03));
}

TEST_CASE("velocity scales the level", "[audition]")
{
    CHECK(steadyRms(md::kOrgan, 1.0F, 64) ==
          Catch::Approx(expectedRms(md::kOrgan) * 64.0 / 127.0).epsilon(0.02));
}

TEST_CASE("velocity 0 is a note off", "[audition]")
{
    md::AuditionSynth synth(md::kOrgan);
    synth.prepare(kRate);
    juce::AudioBuffer<float> buf(2, 4800);
    buf.clear();
    synth.noteOn(69, 127);
    synth.render(buf, 0, 4800);
    synth.noteOn(69, 0);
    buf.clear();
    synth.render(buf, 0, releaseSamples(md::kOrgan));
    buf.clear();
    synth.render(buf, 0, 64);
    CHECK(allZero(buf));
}

TEST_CASE("partials at or above nyquist are skipped", "[audition]")
{
    // Note 96 is 2093 Hz: at 8000 Hz only the fundamental is below 4000 Hz.
    CHECK(steadyRms(md::kOrgan, 1.0F, 127, 8000.0, 96) ==
          Catch::Approx(md::kOrgan.gain * std::sqrt(0.5)).epsilon(0.02));
}

TEST_CASE("note off for a note not sounding changes nothing", "[audition]")
{
    md::AuditionSynth synth(md::kOrgan);
    synth.prepare(kRate);
    synth.setLevel(1.0F);
    juce::AudioBuffer<float> buf(2, 9600);
    buf.clear();
    synth.noteOn(69, 127);
    synth.render(buf, 0, 4800);
    synth.noteOff(70);
    synth.render(buf, 4800, 4800);
    CHECK(rms(buf, 4800, 9600) == Catch::Approx(expectedRms(md::kOrgan)).epsilon(0.02));
}
