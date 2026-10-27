#include "ui/SenderEditor.h"

#include <cstddef>

#include "plugin/PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/Fonts.h"
#include "ui/LaneView.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr std::array<md::LaneId, 3> kLanes{md::LaneId::chords, md::LaneId::bass,
                                           md::LaneId::melody};

constexpr const char* kTitle = "MAGIC DUST";
constexpr const char* kTagline = "SENDER";
constexpr int kTextSlack = 4; // room for the last letter's spacing

std::size_t laneIndex(md::LaneId lane) { return static_cast<std::size_t>(lane); }

} // namespace

SenderBody::SenderBody(MagicDustProcessor& processor)
    : processor_(processor),
      laneKeys_{KeyButton(laneLook(kLanes.at(0)).label, laneLook(kLanes.at(0)).colour),
                KeyButton(laneLook(kLanes.at(1)).label, laneLook(kLanes.at(1)).colour),
                KeyButton(laneLook(kLanes.at(2)).label, laneLook(kLanes.at(2)).colour)},
      mainKey_("full magic dust", std::nullopt)
{
    setBounds(0, 0, t::layout::senderWidth, t::layout::senderHeight);
    int order = 1;
    for (std::size_t i = 0; i < laneKeys_.size(); ++i)
    {
        auto& key = laneKeys_.at(i);
        key.setComponentID(juce::String("senderKey-") + laneLook(kLanes.at(i)).label);
        key.setTitle(juce::String("send ") + laneLook(kLanes.at(i)).label);
        key.setExplicitFocusOrder(order++);
        key.setBounds(t::layout::senderKeyX.at(i), t::layout::senderKeyY, t::layout::senderKeyWidth,
                      t::layout::senderKeyHeight);
        key.onClick = [this, i]
        {
            processor_.setSenderLane(kLanes.at(i));
            refresh();
        };
        addAndMakeVisible(key);
    }
    mainKey_.setComponentID("senderMainKey");
    mainKey_.setTitle("become a full magic dust");
    mainKey_.setExplicitFocusOrder(order);
    mainKey_.setBounds(t::layout::senderMainKeyX, t::layout::senderKeyY,
                       t::layout::senderMainKeyWidth, t::layout::senderKeyHeight);
    mainKey_.onClick = [this] { processor_.becomeMain(); };
    addAndMakeVisible(mainKey_);
    refresh();
}

void SenderBody::refresh()
{
    const auto selected = laneIndex(processor_.senderLane());
    for (std::size_t i = 0; i < laneKeys_.size(); ++i)
    {
        laneKeys_.at(i).setLit(i == selected);
    }
    const auto status =
        MagicDustProcessor::senderLinked()
            ? juce::String("sending ") + laneLook(processor_.senderLane()).label +
                  " from magic dust"
            : juce::String("no magic dust found - load magic dust on another track first");
    if (status != status_)
    {
        status_ = status;
        repaint(t::layout::senderStatus);
    }
}

juce::String SenderBody::drawnStatus() const
{
    const auto inner = t::layout::readoutBorder + t::layout::readoutTextPad;
    return ellipsize(font(Face::monoRegular, t::layout::senderStatusSize), status_,
                     static_cast<float>(t::layout::senderStatus.getWidth()) - (2.0F * inner));
}

void SenderBody::paint(juce::Graphics& g)
{
    paintFaceplate(g, getLocalBounds().toFloat());

    const auto title = font(Face::serifBold, t::layout::senderTitleSize)
                           .withExtraKerningFactor(t::layout::wordmarkFaceSpacing);
    const auto tagline = font(Face::groteskMedium, t::layout::senderTaglineSize)
                             .withExtraKerningFactor(t::layout::taglineSpacing);
    const auto titleWidth = juce::GlyphArrangement::getStringWidthInt(title, kTitle);
    const auto line = [](int x, int width)
    {
        constexpr int height = 28;
        return juce::Rectangle<int>(x, t::layout::senderTitleCentreY - (height / 2), width, height);
    };
    g.setFont(title);
    g.setColour(t::console::legend);
    g.drawText(kTitle, line(t::layout::senderTitleX, titleWidth + kTextSlack),
               juce::Justification::centredLeft, false);
    g.setFont(tagline);
    g.setColour(t::console::tagline);
    const auto taglineX = t::layout::senderTitleX + titleWidth + t::layout::senderTaglineGap;
    g.drawText(
        kTagline,
        line(taglineX, juce::GlyphArrangement::getStringWidthInt(tagline, kTagline) + kTextSlack),
        juce::Justification::centredLeft, false);

    const auto window = t::layout::senderStatus.toFloat();
    paintCreamWindow(g, window, t::layout::readoutBorder, t::layout::readoutRadius,
                     t::console::readoutGlowColour, t::layout::readoutGlow);
    g.setFont(font(Face::monoRegular, t::layout::senderStatusSize));
    g.setColour(t::console::ink);
    g.drawText(drawnStatus(),
               window.reduced(t::layout::readoutBorder + t::layout::readoutTextPad, 0.0F),
               juce::Justification::centredLeft, false);
}

} // namespace md::ui
