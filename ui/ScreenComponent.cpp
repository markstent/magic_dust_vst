#include "ui/ScreenComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/types.hpp"
#include "plugin/DragFiles.h"
#include "plugin/PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kBeatsPerBar = md::kTicksPerBar / md::kTicksPerQuarter;

juce::Rectangle<int> inDisplay(juce::Rectangle<int> window)
{
    return window.translated(-t::layout::display.getX(), -t::layout::display.getY());
}

juce::String selectedText(Selection selection)
{
    switch (selection.kind)
    {
    case Selection::Kind::chord:
        return "selected: chord " + juce::String(selection.index + 1);
    case Selection::Kind::lane:
        return juce::String("selected: ") +
               laneLook(static_cast<md::LaneId>(selection.index)).label;
    case Selection::Kind::none:
        break;
    }
    return "selected: nothing";
}

} // namespace

ScreenComponent::ScreenComponent(MagicDustProcessor& processor)
    : processor_(processor), cells_{ChordCell(0), ChordCell(1), ChordCell(2), ChordCell(3)},
      lanes_{LaneView(md::LaneId::chords, processor.params),
             LaneView(md::LaneId::bass, processor.params),
             LaneView(md::LaneId::melody, processor.params)}
{
    setComponentID("screen");
    // Tab order: the chord cells, then the lanes.
    int order = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i)
    {
        auto& cell = cells_.at(i);
        cell.setComponentID("cell" + juce::String(i));
        cell.setExplicitFocusOrder(++order);
        cell.onClick = [this, i]
        {
            setSelection({.kind = Selection::Kind::chord, .index = static_cast<int>(i)});
            processor_.auditionChord(i);
            report(std::nullopt);
        };
        cell.setOnDrag([this, i] { startDrag(std::nullopt, i); });
        addChildComponent(cell);
    }
    for (std::size_t i = 0; i < lanes_.size(); ++i)
    {
        auto& lane = lanes_.at(i);
        lane.setComponentID("lane" + juce::String(i));
        lane.roll().setComponentID("roll" + juce::String(i));
        lane.levelSlider().setComponentID("level" + juce::String(i));
        lane.setExplicitFocusOrder(++order);
        lane.setOnClick(
            [this, i]
            {
                setSelection({.kind = Selection::Kind::lane, .index = static_cast<int>(i)});
                report(std::nullopt);
            });
        lane.setOnDrag([this, i] { startDrag(static_cast<md::LaneId>(i), std::nullopt); });
        addAndMakeVisible(lane);
    }
    bottomLine_.setComponentID("bottomLine");
    addAndMakeVisible(bottomLine_);
}

void ScreenComponent::report(std::optional<std::string> status)
{
    if (onStatus_)
    {
        onStatus_(std::move(status));
    }
}

void ScreenComponent::startDrag(std::optional<md::LaneId> lane, std::optional<std::size_t> slot)
{
    const auto* idea = processor_.idea();
    if (idea == nullptr)
    {
        return;
    }
    const auto file =
        md::writeDragFile(*idea, lane, slot, md::dragTempo(processor_.hostBpm()), dragFolder_);
    if (!file)
    {
        report("couldn't write the midi file");
        return;
    }
    report(std::nullopt);
    juce::DragAndDropContainer::performExternalDragDropOfFiles({file->getFullPathName()}, false,
                                                               this);
}

void ScreenComponent::setSelection(Selection selection)
{
    selection_ = selection;
    refresh();
}

void ScreenComponent::resized()
{
    for (std::size_t i = 0; i < cells_.size(); ++i)
    {
        cells_.at(i).setBounds(
            inDisplay({t::layout::cellX + (static_cast<int>(i) * t::layout::cellPitch),
                       t::layout::cellY, t::layout::cellPitch, t::layout::cellHeight}));
    }
    for (std::size_t i = 0; i < lanes_.size(); ++i)
    {
        lanes_.at(i).setBounds(inDisplay({t::layout::laneX, t::layout::laneY.at(i),
                                          t::layout::laneWidth, t::layout::laneHeight}));
    }
    bottomLine_.setBounds(inDisplay(t::layout::bottomLine));
}

void ScreenComponent::refresh()
{
    const auto* idea = processor_.idea();
    const auto playing = processor_.previewPlaying();
    const auto bars = idea != nullptr ? idea->settings.bars : md::Settings{}.bars;
    // A count-in before bar 1 reports a negative position.
    const auto barPosition = std::fmod(std::max(0.0, processor_.previewPpq()) / kBeatsPerBar, bars);
    const std::optional<float> playhead =
        playing ? std::optional<float>(static_cast<float>(barPosition / bars)) : std::nullopt;

    for (std::size_t i = 0; i < cells_.size(); ++i)
    {
        const auto shown = idea != nullptr && i < idea->progression.size();
        cells_.at(i).setVisible(shown);
        if (shown)
        {
            const auto& chord = idea->progression.at(i);
            const auto inside = playing && barPosition >= chord.startBar &&
                                barPosition < chord.startBar + chord.bars;
            cells_.at(i).set(md::chordName(idea->key, chord.symbol), chord.feel, inside,
                             selection_.kind == Selection::Kind::chord &&
                                 std::cmp_equal(selection_.index, i));
        }
    }
    for (std::size_t i = 0; i < lanes_.size(); ++i)
    {
        if (idea != nullptr)
        {
            lanes_.at(i).set(idea->lanes.at(i), bars, playhead,
                             selection_.kind == Selection::Kind::lane &&
                                 std::cmp_equal(selection_.index, i));
        }
    }
    bottomLine_.set(playing ? static_cast<int>(barPosition) + 1 : 0, bars, selectedText(selection_),
                    "nothing pinned");
}

void ScreenComponent::paint(juce::Graphics& g)
{
    paintCreamWindow(g, getLocalBounds().toFloat(), t::layout::displayBorder,
                     t::layout::displayRadius, juce::Colour(0xFFAA6E1E).withAlpha(0.45F),
                     t::layout::displayGlow);
}

} // namespace md::ui
