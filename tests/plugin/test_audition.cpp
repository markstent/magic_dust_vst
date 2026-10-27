#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

double steadyRms(const md::VoiceRecipe& r, float level)
{
    md::AuditionSynth synth(r);
    synth.prepare(kRate);
    synth.setLevel(level);
    juce::AudioBuffer<float> buf(2, 9600);
    buf.clear();
    synth.noteOn(69, 127);
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

TEST_CASE("sub bass is mono", "[audition]")
{
    md::AuditionSynth synth(md::kSubBass);
    synth.prepare(kRate);
    juce::AudioBuffer<float> buf(2, 9600);
    buf.clear();
    synth.noteOn(33, 127);
    synth.render(buf, 0, 4800);
    synth.noteOn(45, 127);
    synth.render(buf, 4800, 4800);
    synth.noteOff(33);
    buf.setSize(2, 4800, false, true, false);
    buf.clear();
    synth.render(buf, 0, 4800);
    CHECK(rms(buf, 0, 4800) > 0.1);
    synth.noteOff(45);
    buf.clear();
    synth.render(buf, 0, 4800);
    buf.clear();
    synth.render(buf, 0, 64);
    CHECK(allZero(buf));
}
