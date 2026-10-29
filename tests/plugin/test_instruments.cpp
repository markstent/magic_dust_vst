#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "AllocationGuard.h"
#include "SeedHelper.h"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "plugin/HousePiano.h"
#include "plugin/PluginProcessor.h"

namespace
{

constexpr double kRate = 48000.0;

struct PianoTraits
{
    using Type = md::HousePiano;
    static constexpr double kReleaseMs = 180.0;
    // The loudest single note, at velocity 127, stays below this.
    static constexpr float kPeakBound = 0.65F;
};

int samplesFor(double ms) { return static_cast<int>(kRate * ms / 1000.0); }

juce::AudioBuffer<float> blank(int samples)
{
    juce::AudioBuffer<float> buf(2, samples);
    buf.clear();
    return buf;
}

float peakOf(const juce::AudioBuffer<float>& b) { return b.getMagnitude(0, 0, b.getNumSamples()); }

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double rms(const juce::AudioBuffer<float>& b, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
    {
        sum += static_cast<double>(b.getSample(0, i)) * static_cast<double>(b.getSample(0, i));
    }
    return std::sqrt(sum / (to - from));
}

bool finite(const juce::AudioBuffer<float>& b)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        if (!std::isfinite(b.getSample(0, i)) || !std::isfinite(b.getSample(1, i)))
        {
            return false;
        }
    }
    return true;
}

// Plays a note for ms and returns what it rendered.
template <typename Synth>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
juce::AudioBuffer<float> playNote(Synth& synth, int note, int velocity, double ms)
{
    auto buf = blank(samplesFor(ms));
    synth.noteOn(note, velocity);
    synth.render(buf, 0, buf.getNumSamples());
    return buf;
}

// The power of the buffer at frequency, over [from, to), through a Hann window.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double power(const juce::AudioBuffer<float>& b, double frequency, int from, int to)
{
    double re = 0.0;
    double im = 0.0;
    const int n = to - from;
    for (int i = 0; i < n; ++i)
    {
        const double hann = 0.5 - (0.5 * std::cos(2.0 * std::numbers::pi * i / n));
        const double x = hann * static_cast<double>(b.getSample(0, from + i));
        const double angle = 2.0 * std::numbers::pi * frequency * i / kRate;
        re += x * std::cos(angle);
        im += x * std::sin(angle);
    }
    return (re * re) + (im * im);
}

} // namespace

TEMPLATE_TEST_CASE("no notes make no sound", "[instruments]", PianoTraits)
{
    typename TestType::Type synth;
    synth.prepare(kRate);
    auto buf = blank(512);
    synth.render(buf, 0, 512);
    CHECK(peakOf(buf) == 0.0F);
}

TEMPLATE_TEST_CASE("every note and velocity is finite and bounded", "[instruments]", PianoTraits)
{
    typename TestType::Type synth;
    synth.prepare(kRate);
    synth.setLevel(1.0F);
    float loudest = 0.0F;
    for (int note = 36; note <= 96; ++note)
    {
        for (const int velocity : {1, 64, 127})
        {
            auto buf = playNote(synth, note, velocity, 300.0);
            synth.noteOff(note);
            REQUIRE(finite(buf));
            loudest = std::max(loudest, peakOf(buf));
        }
    }
    CHECK(loudest <= TestType::kPeakBound);
    CHECK(loudest > 0.05F);
}

TEMPLATE_TEST_CASE("a released note is silent after its release and 50 ms", "[instruments]",
                   PianoTraits)
{
    for (const bool all : {false, true})
    {
        typename TestType::Type synth;
        synth.prepare(kRate);
        playNote(synth, 69, 100, 1000.0);
        if (all)
        {
            synth.allNotesOff();
        }
        else
        {
            synth.noteOff(69);
        }
        auto release = blank(samplesFor(TestType::kReleaseMs));
        synth.render(release, 0, release.getNumSamples());
        CHECK(peakOf(release) > 0.0F);
        auto tail = blank(samplesFor(50.0));
        synth.render(tail, 0, tail.getNumSamples());
        CHECK(peakOf(tail) < 1e-5F);
    }
}

TEMPLATE_TEST_CASE("the same input gives the same output", "[instruments]", PianoTraits)
{
    typename TestType::Type a;
    typename TestType::Type b;
    a.prepare(kRate);
    b.prepare(kRate);
    for (auto* synth : {&a, &b})
    {
        synth->noteOn(60, 90);
        synth->noteOn(67, 110);
    }
    auto outA = blank(9600);
    auto outB = blank(9600);
    a.render(outA, 0, 9600);
    b.render(outB, 0, 9600);
    for (int i = 0; i < 9600; ++i)
    {
        REQUIRE(outA.getSample(0, i) == outB.getSample(0, i));
    }
}

TEMPLATE_TEST_CASE("note on, note off and render do not allocate", "[instruments]", PianoTraits)
{
    typename TestType::Type synth;
    synth.prepare(kRate);
    auto buf = blank(512);
    {
        const md_test::ScopedNoAllocation guard;
        synth.setLevel(1.5F);
        for (const int note : {48, 55, 60, 64, 67, 72, 76, 79, 84})
        {
            synth.noteOn(note, 100);
            synth.render(buf, 0, 512);
        }
        synth.noteOff(60);
        synth.render(buf, 0, 512);
        synth.allNotesOff();
        synth.render(buf, 0, 512);
    }
    CHECK(finite(buf));
}

TEMPLATE_TEST_CASE("level 0 is silent, level 2 doubles the peak and levels clamp at 4",
                   "[instruments]", PianoTraits)
{
    const auto peakAt = [](float level)
    {
        typename TestType::Type synth;
        synth.prepare(kRate);
        synth.setLevel(level);
        return peakOf(playNote(synth, 64, 100, 200.0));
    };
    CHECK(peakAt(0.0F) == 0.0F);
    CHECK(peakAt(2.0F) == Catch::Approx(2.0F * peakAt(1.0F)).epsilon(0.001));
    CHECK(peakAt(9.0F) == Catch::Approx(peakAt(4.0F)).epsilon(0.001));
}

TEST_CASE("a piano chord peaks about as high as the old organ chord", "[instruments]")
{
    constexpr double kOldOrganChordPeak = 0.83; // 57 61 64 69 at velocity 100, level 1
    md::HousePiano piano;
    piano.prepare(kRate);
    piano.setLevel(1.0F);
    for (const int note : {57, 61, 64, 69})
    {
        piano.noteOn(note, 100);
    }
    auto buf = blank(samplesFor(1000.0));
    piano.render(buf, 0, buf.getNumSamples());
    CHECK(peakOf(buf) == Catch::Approx(kOldOrganChordPeak).epsilon(0.15));
}

TEST_CASE("the piano plays A4 at 440 Hz", "[instruments]")
{
    md::HousePiano piano;
    piano.prepare(kRate);
    auto buf = playNote(piano, 69, 100, 1500.0);
    // The centroid of the power around the fundamental, over the sustained part.
    double weighted = 0.0;
    double total = 0.0;
    for (int i = 0; i <= 200; ++i)
    {
        const double f = 430.0 + (0.1 * i);
        const double p = power(buf, f, samplesFor(200.0), samplesFor(1200.0));
        weighted += f * p;
        total += p;
    }
    CHECK(weighted / total == Catch::Approx(440.0).margin(1.0));
}

TEST_CASE("a held piano note thins out over two seconds", "[instruments]")
{
    md::HousePiano piano;
    piano.prepare(kRate);
    auto buf = playNote(piano, 60, 100, 2000.0);
    const auto window = [&](double ms) { return rms(buf, samplesFor(ms), samplesFor(ms + 100.0)); };
    CHECK(window(100.0) > window(500.0));
    CHECK(window(500.0) > window(1000.0));
    CHECK(window(1000.0) > window(1900.0));
}

TEST_CASE("a hard piano note is brighter than a soft one", "[instruments]")
{
    const auto brightness = [](int velocity)
    {
        md::HousePiano piano;
        piano.prepare(kRate);
        auto buf = playNote(piano, 57, velocity, 150.0);
        double high = 0.0;
        double all = 0.0;
        for (int i = 0; i <= 198; ++i)
        {
            const double f = 100.0 + (50.0 * i);
            const double p = power(buf, f, 0, buf.getNumSamples());
            all += p;
            high += f >= 2000.0 ? p : 0.0;
        }
        return high / all;
    };
    CHECK(brightness(127) > 1.5 * brightness(40));
}

TEST_CASE("the ninth piano note steals the oldest voice", "[instruments]")
{
    md::HousePiano piano;
    piano.prepare(kRate);
    auto buf = blank(samplesFor(100.0));
    for (const int note : {40, 41, 42, 43, 44, 45, 46, 47})
    {
        piano.noteOn(note, 100);
        piano.render(buf, 0, 480);
    }
    piano.noteOn(80, 100);
    piano.noteOff(40);
    piano.allNotesOff();
    piano.render(buf, 0, samplesFor(100.0));
    piano.noteOn(80, 100);
    auto after = blank(samplesFor(50.0));
    piano.render(after, 0, after.getNumSamples());
    CHECK(finite(after));
    CHECK(peakOf(after) > 0.0F);
}

namespace
{

// A note held for a given number of samples.
struct Edge
{
    int sample;
    int note;
    int velocity;
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::vector<Edge> edgesOf(const MagicDustProcessor& processor, std::size_t lane, double bpm)
{
    const double samplesPerTick = kRate * 60.0 / bpm / md::kTicksPerQuarter;
    std::vector<Edge> edges;
    for (const auto& e : processor.idea()->lanes.at(lane).events)
    {
        edges.push_back({.sample = static_cast<int>(e.start * samplesPerTick),
                         .note = e.pitch,
                         .velocity = e.velocity});
        edges.push_back({.sample = static_cast<int>((e.start + e.length) * samplesPerTick),
                         .note = e.pitch,
                         .velocity = 0});
    }
    std::ranges::stable_sort(edges, {}, &Edge::sample);
    return edges;
}

void play(md::Instrument& synth, const std::vector<Edge>& edges, juce::AudioBuffer<float>& buf)
{
    int done = 0;
    for (const Edge& edge : edges)
    {
        synth.render(buf, done, edge.sample - done);
        done = edge.sample;
        synth.noteOn(edge.note, edge.velocity);
    }
    synth.render(buf, done, buf.getNumSamples() - done);
}

void writeWav(const juce::File& dir, const char* name, const juce::AudioBuffer<float>& buf)
{
    dir.createDirectory();
    const juce::File out = dir.getChildFile(name);
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(out);
    auto writer = wav.createWriterFor(
        stream,
        juce::AudioFormatWriterOptions{}.withSampleRate(kRate).withNumChannels(2).withBitsPerSample(
            24));
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples()));
    std::cout << name << " peak " << peakOf(buf) << '\n';
}

} // namespace

// Renders the owner's listening files into $MD_SOUND_DIR; does nothing when it is not set.
TEST_CASE("render the sound check files", "[.render]")
{
    const char* dir = std::getenv("MD_SOUND_DIR");
    if (dir == nullptr)
    {
        return;
    }
    const juce::File out(dir);
    juce::ScopedJuceInitialiser_GUI gui;
    MagicDustProcessor processor{md::loadBundledBank(), 7};
    setSeed(processor, 1);
    REQUIRE(processor.idea() != nullptr);
    constexpr double kBpm = 124.0;
    const int total = static_cast<int>(8 * 4 * 60.0 / kBpm * kRate) + samplesFor(1500.0);
    // The default levels: the volume's 0.8 times a lane level of 1.
    constexpr float kDefault = 0.8F;

    md::HousePiano piano;
    piano.prepare(kRate);
    piano.setLevel(kDefault);

    auto chords = blank(total);
    play(piano, edgesOf(processor, 0, kBpm), chords);
    writeWav(out, "piano-chords.wav", chords);

    piano.allNotesOff();
    piano.prepare(kRate);
    piano.setLevel(1.0F);
    std::vector<Edge> scale;
    int at = 0;
    for (int note = 48; note <= 84; ++note)
    {
        scale.push_back({.sample = at, .note = note, .velocity = 100});
        scale.push_back({.sample = at + samplesFor(800.0), .note = note, .velocity = 0});
        at += samplesFor(1000.0);
    }
    auto scaleBuf = blank(at + samplesFor(500.0));
    play(piano, scale, scaleBuf);
    writeWav(out, "piano-scale.wav", scaleBuf);
}

// Prints the time to render 512 samples with every voice busy; does nothing unless asked.
TEST_CASE("time the instruments", "[.measure]")
{
    md::HousePiano piano;
    piano.prepare(kRate);
    for (const int note : {36, 38, 40, 41, 43, 45, 47, 48})
    {
        piano.noteOn(note, 127);
    }
    auto buf = blank(512);
    constexpr int kBlocks = 200;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kBlocks; ++i)
    {
        piano.render(buf, 0, 512);
    }
    const auto us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "render per 512-sample block: " << us / kBlocks << " us (budget "
              << 512.0 / kRate * 1e6 << " us)\n";
}
