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
#include "plugin/AuditionSynth.h"
#include "plugin/HousePiano.h"
#include "plugin/LeadSynth.h"
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

struct LeadTraits
{
    using Type = md::LeadSynth;
    static constexpr double kReleaseMs = 120.0;
    static constexpr float kPeakBound = 0.4F;
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

// The centroid of the power within span (a fraction) of guess, over [fromMs, toMs). Two detuned
// saws beat, so their pitch is the middle of the pair, which a peak search would miss.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
double centroidHz(const juce::AudioBuffer<float>& b, double guess, double span, double fromMs,
                  double toMs)
{
    double weighted = 0.0;
    double total = 0.0;
    const int steps = static_cast<int>(guess * 2.0 * span / 0.5);
    for (int i = 0; i <= steps; ++i)
    {
        const double f = (guess * (1.0 - span)) + (0.5 * i);
        const double p = power(b, f, samplesFor(fromMs), samplesFor(toMs));
        weighted += f * p;
        total += p;
    }
    return weighted / total;
}

double cents(double hz, double target) { return 1200.0 * std::log2(hz / target); }

} // namespace

TEMPLATE_TEST_CASE("no notes make no sound", "[instruments]", PianoTraits, LeadTraits)
{
    typename TestType::Type synth;
    synth.prepare(kRate);
    auto buf = blank(512);
    synth.render(buf, 0, 512);
    CHECK(peakOf(buf) == 0.0F);
}

TEMPLATE_TEST_CASE("every note and velocity is finite and bounded", "[instruments]", PianoTraits,
                   LeadTraits)
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
                   PianoTraits, LeadTraits)
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

TEMPLATE_TEST_CASE("the same input gives the same output", "[instruments]", PianoTraits, LeadTraits)
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

TEMPLATE_TEST_CASE("note on, note off and render do not allocate", "[instruments]", PianoTraits,
                   LeadTraits)
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
                   "[instruments]", PianoTraits, LeadTraits)
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

TEST_CASE("a held lead note peaks about as high as the old pluck", "[instruments]")
{
    constexpr double kOldPluckPeak = 0.224; // any note at velocity 100, level 1
    for (const int note : {48, 60, 69, 81})
    {
        md::LeadSynth lead;
        lead.prepare(kRate);
        lead.setLevel(1.0F);
        const auto buf = playNote(lead, note, 100, 1000.0);
        CHECK(peakOf(buf) == Catch::Approx(kOldPluckPeak).epsilon(0.2));
    }
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

TEST_CASE("two overlapping lead notes give one pitch", "[instruments]")
{
    md::LeadSynth lead;
    lead.prepare(kRate);
    auto buf = blank(samplesFor(240.0));
    lead.noteOn(60, 100);
    lead.render(buf, 0, samplesFor(100.0));
    lead.noteOn(67, 100);
    lead.render(buf, samplesFor(100.0), samplesFor(140.0));
    const int from = samplesFor(150.0);
    const int to = samplesFor(240.0);
    const double oldNote = power(buf, juce::MidiMessage::getMidiNoteInHertz(60), from, to);
    const double newNote = power(buf, juce::MidiMessage::getMidiNoteInHertz(67), from, to);
    CHECK(oldNote < 1e-3 * newNote);
}

TEST_CASE("a legato lead glides to the new note within 2 cents in 45 ms", "[instruments]")
{
    md::LeadSynth lead;
    lead.prepare(kRate);
    auto buf = blank(samplesFor(240.0));
    lead.noteOn(60, 100);
    lead.render(buf, 0, samplesFor(100.0));
    lead.noteOn(67, 100);
    lead.render(buf, samplesFor(100.0), samplesFor(140.0));
    const double target = juce::MidiMessage::getMidiNoteInHertz(67);
    CHECK(std::abs(cents(centroidHz(buf, target, 0.15, 150.0, 240.0), target)) < 2.0);
    // Halfway through the glide the pitch is between the two notes.
    const double mid = centroidHz(buf, 330.0, 0.3, 105.0, 135.0);
    CHECK(mid > juce::MidiMessage::getMidiNoteInHertz(60) * 1.05);
    CHECK(mid < target * 0.95);
}

TEST_CASE("only the sounding lead note can be released", "[instruments]")
{
    md::LeadSynth lead;
    lead.prepare(kRate);
    auto buf = blank(samplesFor(600.0));
    lead.noteOn(60, 100);
    lead.noteOn(67, 100);
    lead.noteOff(60);
    lead.noteOff(65);
    lead.render(buf, 0, samplesFor(300.0));
    CHECK(rms(buf, samplesFor(200.0), samplesFor(300.0)) > 0.0);
    lead.noteOff(67);
    lead.render(buf, samplesFor(300.0), samplesFor(300.0));
    CHECK(rms(buf, samplesFor(580.0), samplesFor(600.0)) == 0.0);
}

TEST_CASE("a separate lead note retriggers its envelope and a legato one does not", "[instruments]")
{
    // A retriggered filter opens again, so the high band is far stronger just after the note on
    // than at the sustain.
    const auto burst = [](bool legato)
    {
        md::LeadSynth lead;
        lead.prepare(kRate);
        auto first = blank(samplesFor(1200.0));
        lead.noteOn(60, 100);
        lead.render(first, 0, first.getNumSamples());
        if (!legato)
        {
            lead.noteOff(60);
            auto gap = blank(samplesFor(300.0));
            lead.render(gap, 0, gap.getNumSamples());
        }
        auto buf = blank(samplesFor(600.0));
        lead.noteOn(62, 100);
        lead.render(buf, 0, buf.getNumSamples());
        const auto band = [&](double fromMs, double toMs)
        {
            double sum = 0.0;
            for (int i = 0; i <= 30; ++i)
            {
                const double f = 3000.0 + (100.0 * i);
                sum += power(buf, f, samplesFor(fromMs), samplesFor(toMs));
            }
            return sum / (toMs - fromMs);
        };
        return band(5.0, 35.0) / band(500.0, 600.0);
    };
    CHECK(burst(false) > 3.0);
    CHECK(burst(true) < 1.5);
}

TEST_CASE("the lead vibrato is absent for the first 200 ms and present later", "[instruments]")
{
    md::LeadSynth lead;
    lead.prepare(kRate);
    const auto buf = playNote(lead, 69, 100, 1000.0);
    const double target = juce::MidiMessage::getMidiNoteInHertz(69);
    const auto at = [&](double fromMs, double toMs)
    { return cents(centroidHz(buf, target, 0.15, fromMs, toMs), target); };
    CHECK(std::abs(at(20.0, 120.0)) < 3.0);
    CHECK(std::abs(at(100.0, 200.0)) < 3.0);
    // Later the pitch swings by up to 12 cents each way.
    const double swing = std::abs(at(724.0, 824.0) - at(815.0, 915.0));
    CHECK(swing > 8.0);
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
    md::AuditionSynth bass(md::kSubBass);
    md::LeadSynth lead;
    for (md::Instrument* synth :
         {static_cast<md::Instrument*>(&piano), static_cast<md::Instrument*>(&bass),
          static_cast<md::Instrument*>(&lead)})
    {
        synth->prepare(kRate);
        synth->setLevel(kDefault);
    }

    auto chords = blank(total);
    play(piano, edgesOf(processor, 0, kBpm), chords);
    writeWav(out, "piano-chords.wav", chords);

    auto melody = blank(total);
    play(lead, edgesOf(processor, 2, kBpm), melody);
    writeWav(out, "lead-melody.wav", melody);

    auto bassLine = blank(total);
    play(bass, edgesOf(processor, 1, kBpm), bassLine);

    auto full = blank(total);
    for (const auto* part : {&chords, &bassLine, &melody})
    {
        for (int c = 0; c < 2; ++c)
        {
            full.addFrom(c, 0, *part, c, 0, total);
        }
    }
    writeWav(out, "full-idea.wav", full);

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

    // A slow legato line: the notes overlap, so each one glides, and the long holds show the
    // vibrato.
    lead.prepare(kRate);
    lead.setLevel(1.0F);
    std::vector<Edge> line;
    at = 0;
    for (const int note : {60, 67, 64, 72, 69, 62, 65, 60})
    {
        line.push_back({.sample = at, .note = note, .velocity = 100});
        line.push_back({.sample = at + samplesFor(1400.0), .note = note, .velocity = 0});
        at += samplesFor(1200.0);
    }
    auto legato = blank(at + samplesFor(800.0));
    play(lead, line, legato);
    writeWav(out, "lead-legato.wav", legato);
}

// Prints the time to render 512 samples with every voice busy; does nothing unless asked.
TEST_CASE("time the instruments", "[.measure]")
{
    md::HousePiano piano;
    md::LeadSynth lead;
    piano.prepare(kRate);
    lead.prepare(kRate);
    for (const int note : {36, 38, 40, 41, 43, 45, 47, 48})
    {
        piano.noteOn(note, 127);
    }
    lead.noteOn(40, 127);
    auto buf = blank(512);
    constexpr int kBlocks = 200;
    for (md::Instrument* synth :
         {static_cast<md::Instrument*>(&piano), static_cast<md::Instrument*>(&lead)})
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kBlocks; ++i)
        {
            synth->render(buf, 0, 512);
        }
        const auto us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                .count();
        std::cout << "render per 512-sample block: " << us / kBlocks << " us (budget "
                  << 512.0 / kRate * 1e6 << " us)\n";
    }
}
