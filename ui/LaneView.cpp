#include "ui/LaneView.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "md/ranges.hpp"
#include "ui/Drag.h"
#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

LaneLook laneLook(md::LaneId lane)
{
    switch (lane)
    {
    case md::LaneId::chords:
        return {.label = "chords",
                .colour = t::console::capChords,
                .low = md::kChordLow,
                .high = md::kChordHigh};
    case md::LaneId::bass:
        return {.label = "bass",
                .colour = t::console::capBass,
                .low = md::kBassLow,
                .high = md::kBassHigh};
    case md::LaneId::melody:
        break;
    }
    return {.label = "melody",
            .colour = t::console::capMelody,
            .low = md::kMelodyLow,
            .high = md::kMelodyHigh};
}

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
    const auto look = laneLook(lane_);
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
    const auto ink = t::console::ink;

    g.setColour(ink.withAlpha(t::layout::barLineAlpha));
    for (int k = 1; k < bars_; ++k)
    {
        g.fillRect(width * k / bars_, 0, 1, height);
    }

    g.setColour(ink);
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
        g.fillRect(static_cast<int>(std::round(*playhead_ * static_cast<float>(width))), 0,
                   t::layout::playheadWidth, height);
    }
}

LaneView::LaneView(md::LaneId lane) : lane_(lane), roll_(lane)
{
    addAndMakeVisible(roll_);
    setWantsKeyboardFocus(true);
}

void LaneView::set(const md::Lane& lane, int bars, std::optional<float> playhead, bool selected)
{
    roll_.set(lane.events, bars, playhead);
    const auto pattern = juce::String(lane.pattern);
    if (pattern != pattern_ || selected != selected_)
    {
        pattern_ = pattern;
        selected_ = selected;
        setTitle(juce::String(laneLook(lane_).label) + " lane, " + pattern_);
        repaint();
    }
}

void LaneView::resized()
{
    roll_.setBounds(t::layout::rollX - t::layout::laneX, t::layout::rollPadY, t::layout::rollWidth,
                    t::layout::rollHeight);
}

// The label column, where the lane name and pattern name are drawn.
juce::Rectangle<int> LaneView::labelBounds() const
{
    return {t::layout::laneLabelPad, 0, t::layout::laneLabelWidth, getHeight()};
}

juce::String LaneView::drawnPattern() const
{
    return ellipsize(font(Face::monoRegular, t::layout::lanePatternSize), pattern_.toUpperCase(),
                     static_cast<float>(labelBounds().getWidth()));
}

void LaneView::mouseDown(const juce::MouseEvent& /*e*/)
{
    // The OS drag can swallow the mouse-up, so every press starts afresh.
    dragging_ = false;
}

void LaneView::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging_ && exceedsDragThreshold(e.getMouseDownPosition(), e.getPosition()))
    {
        dragging_ = true;
        if (onDrag_)
        {
            onDrag_();
        }
    }
}

void LaneView::mouseUp(const juce::MouseEvent& e)
{
    const auto wasDragging = std::exchange(dragging_, false);
    if (!wasDragging && getLocalBounds().contains(e.getPosition()) && onClick_)
    {
        onClick_();
    }
}

bool LaneView::keyPressed(const juce::KeyPress& key)
{
    if (isActivateKey(key) && onClick_)
    {
        onClick_();
        return true;
    }
    return false;
}

void LaneView::paint(juce::Graphics& g)
{
    const auto ink = t::console::ink;
    if (selected_)
    {
        g.setColour(ink.withAlpha(t::layout::laneSelectedAlpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), t::layout::laneRadius);
    }
    const auto nameFont = font(Face::groteskBold, t::layout::laneNameSize)
                              .withExtraKerningFactor(t::layout::laneNameSpacing);
    const auto patternFont = font(Face::monoRegular, t::layout::lanePatternSize);
    const auto label = labelBounds().toFloat();
    const auto top =
        label.getY() +
        ((label.getHeight() - (nameFont.getHeight() + patternFont.getHeight())) / 2.0F);
    g.setColour(ink);
    g.setFont(nameFont);
    g.drawText(juce::String(laneLook(lane_).label).toUpperCase(),
               juce::Rectangle<float>(label.getX(), top, label.getWidth(), nameFont.getHeight()),
               juce::Justification::centredLeft, true);
    g.setFont(patternFont);
    g.drawText(drawnPattern(),
               juce::Rectangle<float>(label.getX(), top + nameFont.getHeight(), label.getWidth(),
                                      patternFont.getHeight()),
               juce::Justification::centredLeft, false);
    if (hasKeyboardFocus(true))
    {
        drawFocusRing(g, getLocalBounds().toFloat(), t::layout::laneRadius, ink);
    }
}

} // namespace md::ui
