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
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kBeatsPerBar = md::kTicksPerBar / md::kTicksPerQuarter;

juce::Rectangle<int> inScreen(juce::Rectangle<int> window)
{
    return window.translated(-t::layout::screenOuter.getX(), -t::layout::screenOuter.getY());
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
    : processor_(processor), topLine_(processor),
      bubbles_{ChordBubble(0), ChordBubble(1), ChordBubble(2), ChordBubble(3)},
      lanes_{LaneView(md::LaneId::chords), LaneView(md::LaneId::bass),
             LaneView(md::LaneId::melody)},
      auraPad_(processor.params), fields_(processor.params)
{
    setComponentID("screen");
    // Tab order: top line, bubbles, lanes, aura pad, then the fields.
    int order = 0;
    topLine_.setExplicitFocusOrder(++order);
    addAndMakeVisible(topLine_);
    for (std::size_t i = 0; i < bubbles_.size(); ++i)
    {
        auto& bubble = bubbles_.at(i);
        bubble.setComponentID("bubble" + juce::String(i));
        bubble.setExplicitFocusOrder(++order);
        bubble.onClick = [this, i]
        {
            setSelection({.kind = Selection::Kind::chord, .index = static_cast<int>(i)});
            processor_.auditionChord(i);
            report(std::nullopt);
        };
        bubble.setOnDrag([this, i] { startDrag(std::nullopt, i); });
        addChildComponent(bubble);
    }
    for (std::size_t i = 0; i < lanes_.size(); ++i)
    {
        auto& lane = lanes_.at(i);
        lane.setComponentID("lane" + juce::String(i));
        lane.roll().setComponentID("roll" + juce::String(i));
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
    auraPad_.setExplicitFocusOrder(++order);
    fields_.setExplicitFocusOrder(++order);
    auraPad_.setComponentID("auraPad");
    fields_.setComponentID("statusFields");
    readout_.setComponentID("readout");
    addAndMakeVisible(auraPad_);
    addAndMakeVisible(fields_);
    addAndMakeVisible(readout_);
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
    const auto file = md::writeDragFile(*idea, lane, slot, md::dragTempo(processor_.hostBpm()));
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
    topLine_.setBounds(inScreen(t::layout::topLine));
    for (std::size_t i = 0; i < bubbles_.size(); ++i)
    {
        bubbles_.at(i).setBounds(inScreen({t::layout::bubbleX.at(i), t::layout::bubbleY,
                                           t::layout::bubbleSize, t::layout::bubbleSize}));
    }
    for (std::size_t i = 0; i < lanes_.size(); ++i)
    {
        lanes_.at(i).setBounds(inScreen({t::layout::laneX, t::layout::laneY.at(i),
                                         t::layout::laneWidth, t::layout::laneHeight}));
    }
    auraPad_.setBounds(inScreen(t::layout::auraPad));
    fields_.setBounds(inScreen(t::layout::statusFields));
    readout_.setBounds(inScreen(t::layout::readout));
    topLine_.refresh();
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

    topLine_.refresh();
    for (std::size_t i = 0; i < bubbles_.size(); ++i)
    {
        const auto shown = idea != nullptr && i < idea->progression.size();
        bubbles_.at(i).setVisible(shown);
        if (shown)
        {
            const auto& chord = idea->progression.at(i);
            const auto inside = playing && barPosition >= chord.startBar &&
                                barPosition < chord.startBar + chord.bars;
            bubbles_.at(i).set(md::chordName(idea->key, chord.symbol), chord.feel, inside,
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
    auraPad_.set(static_cast<int>(processor_.params.getRawParameterValue("mood")->load()),
                 static_cast<int>(processor_.params.getRawParameterValue("energy")->load()));
    fields_.refresh();
    readout_.set(playing ? static_cast<int>(barPosition) + 1 : 0, bars, selectedText(selection_),
                 "nothing pinned");
}

void ScreenComponent::paint(juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    g.setColour(t::bezel);
    g.fillRoundedRectangle(outer, t::screenRadius);
    g.setColour(t::screen);
    g.fillRoundedRectangle(outer.reduced(t::bezelWidth), t::screenRadius - t::bezelWidth);
}

} // namespace md::ui
