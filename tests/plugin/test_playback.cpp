#include <catch2/catch_test_macros.hpp>

#include <vector>

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
    CHECK(p.events.at(0).on);
    CHECK(p.events.at(1).tick == 960);
    CHECK_FALSE(p.events.at(1).on);
    CHECK(p.events.at(2).on);
    CHECK(p.events.at(2).channel == 1);
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
