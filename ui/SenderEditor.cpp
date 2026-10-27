#include "ui/SenderEditor.h"

#include <cstddef>

#include "plugin/PluginProcessor.h"
#include "ui/BodyMaterial.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr std::array<md::LaneId, 3> kLanes{md::LaneId::chords, md::LaneId::bass,
                                           md::LaneId::melody};
constexpr std::array<const char*, 3> kLaneNames{"chords", "bass", "melody"};

const std::array<juce::Colour, 3> kLaneColours{t::violet, t::magenta, t::cyan};

std::size_t laneIndex(md::LaneId lane) { return static_cast<std::size_t>(lane); }

} // namespace

LabelKey::LabelKey(const juce::String& label, std::optional<juce::Colour> led)
    : KeyButton(label, "", 0.0F, led)
{
}

void LabelKey::paintContent(juce::Graphics& g, juce::Rectangle<float> face) const
{
    g.setColour(t::ink);
    g.setFont(font(Face::monoRegular, t::keyLabelSize));
    g.drawText(getButtonText(), face, juce::Justification::centred, false);
}

SenderBody::SenderBody(MagicDustProcessor& processor)
    : processor_(processor), laneKeys_{LabelKey(kLaneNames.at(0), kLaneColours.at(0)),
                                       LabelKey(kLaneNames.at(1), kLaneColours.at(1)),
                                       LabelKey(kLaneNames.at(2), kLaneColours.at(2))},
      mainKey_("full magic dust", std::nullopt)
{
    setBounds(0, 0, t::layout::senderWidth, t::layout::senderHeight);
    int order = 1;
    for (std::size_t i = 0; i < laneKeys_.size(); ++i)
    {
        auto& key = laneKeys_.at(i);
        key.setComponentID(juce::String("senderKey-") + kLaneNames.at(i));
        key.setTitle(juce::String("send ") + kLaneNames.at(i));
        key.setExplicitFocusOrder(order++);
        key.setBounds(t::layout::senderKeyX + (static_cast<int>(i) * t::layout::senderKeyPitch),
                      t::layout::senderKeyY, t::layout::keySize, t::layout::keySize);
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
                       t::layout::senderMainKeyWidth, t::layout::keySize);
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
            ? juce::String("sending ") + kLaneNames.at(selected) + " from magic dust"
            : juce::String("no magic dust found - load magic dust on another track first");
    if (status != status_)
    {
        status_ = status;
        repaint(t::layout::senderStatus);
    }
}

void SenderBody::paint(juce::Graphics& g)
{
    paintBody(g, getLocalBounds().toFloat());

    g.setColour(t::ink);
    g.setFont(font(Face::groteskBold, t::wordmarkSize)
                  .withExtraKerningFactor(t::wordmarkSpacing / t::wordmarkSize));
    g.drawText("magic dust sender", t::layout::senderTitle, juce::Justification::centredLeft,
               false);

    g.setColour(t::inkStatus);
    g.setFont(font(Face::monoRegular, t::statusLineSize));
    g.drawText(status_, t::layout::senderStatus, juce::Justification::centredLeft, false);
}

} // namespace md::ui
