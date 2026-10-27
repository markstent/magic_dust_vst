#include "ui/LaneView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "md/ranges.hpp"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

struct LaneLook
{
    const char* label;
    juce::Colour colour;
    int low;
    int high;
};

LaneLook lookOf(md::LaneId lane)
{
    switch (lane)
    {
    case md::LaneId::chords:
        return {
            .label = "chords", .colour = t::violet, .low = md::kChordLow, .high = md::kChordHigh};
    case md::LaneId::bass:
        return {.label = "bass", .colour = t::magenta, .low = md::kBassLow, .high = md::kBassHigh};
    case md::LaneId::melody:
        break;
    }
    return {.label = "melody", .colour = t::cyan, .low = md::kMelodyLow, .high = md::kMelodyHigh};
}

} // namespace

PianoRoll::PianoRoll(md::LaneId lane) : lane_(lane) { setInterceptsMouseClicks(false, false); }

void PianoRoll::set(const std::vector<md::NoteEvent>& notes, int bars,
                    std::optional<float> playhead)
{
    if (notes == notes_ && bars == bars_ && playhead == playhead_)
    {
        return;
    }
    notes_ = notes;
    bars_ = bars;
    playhead_ = playhead;
    repaint();
}

std::vector<juce::Rectangle<float>> PianoRoll::noteRects() const
{
    const auto look = lookOf(lane_);
    const auto width = static_cast<float>(getWidth());
    const auto noteHeight = static_cast<float>(getHeight()) *
                            t::layout::noteHeightFraction.at(static_cast<std::size_t>(lane_));
    const auto travel = static_cast<float>(getHeight()) - noteHeight;
    const auto ticks = static_cast<float>(bars_ * md::kTicksPerBar);

    std::vector<juce::Rectangle<float>> rects;
    rects.reserve(notes_.size());
    for (const auto& note : notes_)
    {
        const auto pitch = std::clamp(note.pitch, look.low, look.high);
        const auto fromTop =
            static_cast<float>(look.high - pitch) / static_cast<float>(look.high - look.low);
        rects.emplace_back(static_cast<float>(note.start) / ticks * width, fromTop * travel,
                           static_cast<float>(note.length) / ticks * width, noteHeight);
    }
    return rects;
}

void PianoRoll::paint(juce::Graphics& g)
{
    const auto width = getWidth();
    const auto height = getHeight();

    g.setColour(t::screenAxis);
    g.fillRect(0, height / 2, width, 1);

    g.setColour(t::screenBar);
    const auto segments = std::max(1, bars_ / 2);
    for (int k = 1; k < segments; ++k)
    {
        g.fillRect(width * k / segments, 0, 1, height);
    }

    g.setColour(lookOf(lane_).colour);
    for (const auto& r : noteRects())
    {
        const auto left = std::round(r.getX());
        const auto right =
            std::max(std::round(r.getRight()), left + static_cast<float>(t::layout::noteMinWidth));
        const auto noteWidth = right - left;
        g.fillRoundedRectangle(
            left, r.getY(), noteWidth, r.getHeight(),
            std::min({t::layout::noteRadius, noteWidth / 2.0F, r.getHeight() / 2.0F}));
    }

    if (playhead_)
    {
        g.setColour(t::gold);
        g.fillRect(static_cast<int>(std::round(*playhead_ * static_cast<float>(width))), 0,
                   t::layout::playheadWidth, height);
    }
}

LaneView::LaneView(md::LaneId lane) : lane_(lane), roll_(lane)
{
    addAndMakeVisible(roll_);
    setInterceptsMouseClicks(false, true);
}

void LaneView::set(const md::Lane& lane, int bars, std::optional<float> playhead, bool selected)
{
    roll_.set(lane.events, bars, playhead);
    setTitle(juce::String(lookOf(lane_).label) + " lane, " + lane.pattern);
    if (selected != selected_)
    {
        selected_ = selected;
        repaint();
    }
}

void LaneView::resized()
{
    roll_.setBounds(t::layout::rollX - t::layout::laneX, t::layout::rollPadY, t::layout::rollWidth,
                    t::layout::rollHeight);
}

void LaneView::paint(juce::Graphics& g)
{
    if (selected_)
    {
        g.setColour(t::screenSelected);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), t::layout::laneRadius);
    }
    const auto look = lookOf(lane_);
    g.setFont(font(Face::monoRegular, t::laneLabelSize));
    g.setColour(selected_ ? t::screenInk : look.colour);
    g.drawText(look.label,
               juce::Rectangle<int>(t::layout::laneLabelPad, 0,
                                    t::layout::rollX - t::layout::laneX - t::layout::laneLabelPad,
                                    getHeight()),
               juce::Justification::centredLeft, false);
}

} // namespace md::ui
