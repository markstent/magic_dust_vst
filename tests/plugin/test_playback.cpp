#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "md/bank.hpp"
#include "md/generate.hpp"
#include "md/midifile.hpp"

#include "plugin/Playback.h"

namespace
{

std::vector<md::ScheduledEvent> run(const md::PlaybackPattern& p, double startPpq, double bpm,
                                    double sr, int n)
{
    std::vector<md::ScheduledEvent> out;
    md::schedule(p, startPpq, bpm, sr, n, [&](const md::ScheduledEvent& e) { out.push_back(e); });
    return out;
}

md::PlayEvent ev(int tick, int channel, int note, int velocity)
{
    return {.tick = tick,
            .channel = static_cast<std::uint8_t>(channel),
            .note = static_cast<std::uint8_t>(note),
            .velocity = static_cast<std::uint8_t>(velocity),
            .on = velocity > 0};
}

md::PlaybackPattern onePattern()
{
    return {.events = {ev(480, 0, 60, 100), ev(960, 0, 60, 0)}, .lengthTicks = 3840};
}

} // namespace

TEST_CASE("a note schedules once at the right sample across block sizes and rates")
{
    for (const double sr : {44100.0, 48000.0, 96000.0})
    {
        const double expected = 0.25 * sr; // an 8th at 120 bpm
        for (const int block : {32, 64, 512, 2048})
        {
            const auto p = onePattern();
            int ons = 0;
            double pos = 0;
            double ppq = 0.0;
            while (pos < 2.0 * sr)
            {
                for (const auto& e : run(p, ppq, 120.0, sr, block))
                {
                    if (e.event.on)
                    {
                        ++ons;
                        CHECK(std::abs(pos + e.sampleOffset - expected) <= 1.0);
                    }
                }
                pos += block;
                ppq += block * 120.0 / 60.0 / sr;
            }
            CHECK(ons == 1);
        }
    }
}

TEST_CASE("the loop wraps with offs before the restart")
{
    const md::PlaybackPattern p{
        .events = {ev(0, 0, 60, 100), ev(0, 1, 40, 90), ev(100, 0, 60, 0), ev(8 * 3840, 1, 40, 0)},
        .lengthTicks = 8 * 3840};
    const auto out = run(p, 31.9, 120.0, 44100.0, 4410);
    REQUIRE(out.size() == 3);
    CHECK_FALSE(out.at(0).event.on);
    CHECK(out.at(0).event.tick == 8 * 3840);
    CHECK(out.at(1).event.on);
    CHECK(out.at(2).event.on);
    CHECK(out.at(0).sampleOffset <= out.at(1).sampleOffset);
}

TEST_CASE("the internal clock drives every event once across the loop wrap")
{
    constexpr int loop = 8 * 3840;
    std::vector<md::PlayEvent> pattern;
    pattern.reserve(9);
    for (int channel = 0; channel < 3; ++channel)
    {
        pattern.push_back(ev(0, channel, 60 + channel, 100));
    }
    for (int channel = 0; channel < 3; ++channel)
    {
        pattern.push_back(ev(100, channel, 60 + channel, 0));
    }
    for (int channel = 0; channel < 3; ++channel)
    {
        pattern.push_back(ev(loop, channel, 60 + channel, 0));
    }
    const md::PlaybackPattern p{.events = pattern, .lengthTicks = loop};

    std::vector<md::PlayEvent> expected;
    for (int pass = 0; pass < 3; ++pass)
    {
        const bool last = pass == 2;
        expected.insert(expected.end(), pattern.begin(), pattern.begin() + (last ? 6 : 9));
    }
    constexpr int total = 2048 * 700; // 65 ppq at 120 bpm and 44100 Hz
    for (const int block : {32, 64, 512, 2048})
    {
        md::InternalClock clock;
        clock.start();
        std::vector<md::PlayEvent> got;
        for (int done = 0; done < total; done += block)
        {
            md::schedule(p, clock.ppq(), 120.0, 44100.0, block,
                         [&](const md::ScheduledEvent& e) { got.push_back(e.event); });
            clock.advance(block, 120.0, 44100.0);
        }
        REQUIRE(got.size() == expected.size());
        for (std::size_t i = 0; i < got.size(); ++i)
        {
            CHECK(got.at(i).channel == expected.at(i).channel);
            CHECK(got.at(i).on == expected.at(i).on);
            CHECK(got.at(i).tick == expected.at(i).tick);
        }
    }
}

TEST_CASE("a block starting on the loop boundary fires tick 0 once and not the end offs")
{
    constexpr int loop = 8 * 3840;
    const md::PlaybackPattern p{.events = {ev(0, 0, 60, 100), ev(0, 1, 40, 90), ev(0, 2, 72, 80),
                                           ev(loop, 0, 60, 0), ev(loop, 1, 40, 0),
                                           ev(loop, 2, 72, 0)},
                                .lengthTicks = loop};
    const auto out = run(p, 32.0, 120.0, 44100.0, 512);
    REQUIRE(out.size() == 3);
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        CHECK(out.at(i).event.on);
        CHECK(out.at(i).event.channel == i);
    }
}

TEST_CASE("a tempo change moves events with that block's tempo")
{
    const auto p = onePattern();
    const auto a = run(p, 0.25, 120.0, 44100.0, 44100);
    REQUIRE(a.size() == 2);
    CHECK(a.at(0).sampleOffset == 5512);
    const auto b = run(p, 0.25, 128.0, 44100.0, 44100);
    REQUIRE(!b.empty());
    CHECK(b.at(0).sampleOffset == static_cast<int>(0.25 / (128.0 / 60.0 / 44100.0)));
}

TEST_CASE("sounding notes release only what is held")
{
    md::SoundingNotes s;
    s.apply(ev(0, 2, 60, 100));
    s.apply(ev(0, 2, 64, 100));
    s.apply(ev(0, 2, 60, 0));
    std::vector<std::pair<int, int>> off;
    s.releaseAll([&](int c, int n) { off.emplace_back(c, n); });
    REQUIRE(off.size() == 1);
    CHECK(off.at(0) == std::pair<int, int>{2, 64});
}

TEST_CASE("makePattern sorts offs first and skips muted lanes")
{
    md::Idea idea{};
    idea.settings.bars = 4;
    idea.lanes.at(0).events = {{.pitch = 60, .start = 0, .length = 960, .velocity = 100}};
    idea.lanes.at(1).events = {{.pitch = 40, .start = 960, .length = 480, .velocity = 90}};
    idea.lanes.at(2).events = {{.pitch = 72, .start = 0, .length = 480, .velocity = 80}};
    idea.lanes.at(2).muted = true;
    const auto p = md::makePattern(idea);
    CHECK(p.lengthTicks == 4 * 3840);
    REQUIRE(p.events.size() == 4);
    // The default groove lays the chords back 16 ticks; the bass stays on the grid.
    CHECK(p.events.at(0).on);
    CHECK(p.events.at(0).tick == 16);
    CHECK(p.events.at(1).tick == 960);
    CHECK(p.events.at(1).on);
    CHECK(p.events.at(1).channel == 1);
    CHECK(p.events.at(2).tick == 976);
    CHECK_FALSE(p.events.at(2).on);
    CHECK(p.events.at(3).tick == 1440);
    CHECK_FALSE(p.events.at(3).on);
}

TEST_CASE("internal clock counts ppq")
{
    md::InternalClock c;
    c.start();
    c.advance(44100, 120.0, 44100.0);
    CHECK(c.ppq() == 2.0);
    c.stop();
    CHECK_FALSE(c.running());
}

TEST_CASE("makePattern plays exactly the notes the MIDI export writes")
{
    for (const md::Groove groove :
         {md::Groove::straight, md::Groove::swing54, md::Groove::ukShuffle58, md::Groove::heavy62})
    {
        md::Settings settings;
        settings.groove = groove;
        const md::Idea idea = md::generate(md::loadBundledBank(), settings, 4242);
        const auto p = md::makePattern(idea);
        for (std::uint8_t channel = 0; channel < 3; ++channel)
        {
            std::vector<std::pair<int, int>> played;
            for (const md::PlayEvent& e : p.events)
            {
                if (e.on && e.channel == channel)
                {
                    played.emplace_back(e.tick, e.note);
                }
            }
            std::vector<std::pair<int, int>> exported;
            for (const md::NoteEvent& n :
                 md::laneTrack(idea, static_cast<md::LaneId>(channel)).notes)
            {
                exported.emplace_back(n.start, n.pitch);
            }
            std::ranges::sort(played);
            std::ranges::sort(exported);
            CHECK(played == exported);
        }
    }
}
