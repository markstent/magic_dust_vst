#include "PluginEditor.h"

#include <optional>
#include <string>

#include "PluginProcessor.h"
#include "ui/Fonts.h"
#include "ui/ScreenComponent.h"
#include "ui/Tokens.h"

namespace t = md::tokens;

// The instrument body, drawn at the logical 1040 x 740 size; the editor scales it as a whole.
class MagicDustEditor::Body : public juce::Component
{
  public:
    explicit Body(MagicDustProcessor& processor) : processor_(processor), screen_(processor)
    {
        setBounds(0, 0, t::layout::windowWidth, t::layout::windowHeight);
        screen_.setBounds(t::layout::screenOuter);
        addAndMakeVisible(screen_);
        refresh();
    }

    void refresh()
    {
        const auto error = processor_.lastError();
        if (error != error_)
        {
            error_ = error;
            repaint(t::layout::statusLine);
        }
        screen_.refresh();
    }

    void paint(juce::Graphics& g) override
    {
        const auto outer = getLocalBounds().toFloat();
        const auto inner = outer.reduced(1.0F);
        const auto radius = t::bodyRadius - 1.0F;

        g.setColour(t::bodyEdge);
        g.fillRoundedRectangle(outer, t::bodyRadius);
        g.setColour(t::body);
        g.fillRoundedRectangle(inner, radius);

        juce::Path padding;
        padding.addRoundedRectangle(inner, radius);
        g.saveState();
        g.reduceClipRegion(padding, {});
        paintBrushing(g, inner);
        paintHighlight(g, inner, radius);
        g.restoreState();

        if (error_)
        {
            g.setFont(md::ui::font(md::ui::Face::monoRegular, t::statusLineSize));
            g.setColour(t::inkStatus);
            g.drawText(*error_, t::layout::statusLine, juce::Justification::centredLeft, false);
        }
    }

  private:
    // Repeating from the bottom edge: 1 px white, then 2 px black.
    static void paintBrushing(juce::Graphics& g, juce::Rectangle<float> inner)
    {
        const auto left = static_cast<int>(inner.getX());
        const auto width = static_cast<int>(inner.getWidth());
        for (int bottom = static_cast<int>(inner.getBottom()); bottom > 0; bottom -= t::brushPeriod)
        {
            g.setColour(juce::Colours::white.withAlpha(t::brushWhiteAlpha));
            g.fillRect(left, bottom - 1, width, 1);
            g.setColour(juce::Colours::black.withAlpha(t::brushBlackAlpha));
            g.fillRect(left, bottom - t::brushPeriod, width, t::brushPeriod - 1);
        }
    }

    // An inset 1 px white line along the top edge: the padding box minus itself moved down 1 px.
    static void paintHighlight(juce::Graphics& g, juce::Rectangle<float> inner, float radius)
    {
        juce::Path crescents;
        crescents.setUsingNonZeroWinding(false);
        crescents.addRoundedRectangle(inner, radius);
        crescents.addRoundedRectangle(inner.translated(0.0F, 1.0F), radius);
        g.setColour(juce::Colours::white.withAlpha(t::highlightAlpha));
        g.fillPath(crescents);
    }

    MagicDustProcessor& processor_;
    md::ui::ScreenComponent screen_;
    std::optional<std::string> error_;
};

MagicDustEditor::MagicDustEditor(MagicDustProcessor& owner)
    : AudioProcessorEditor(owner), body_(std::make_unique<Body>(owner))
{
    body_->setLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(*body_);

    const auto width = static_cast<int>(t::layout::windowWidth * t::layout::minScale);
    const auto height = static_cast<int>(t::layout::windowHeight * t::layout::minScale);
    setResizable(true, false);
    setResizeLimits(width, height, static_cast<int>(t::layout::windowWidth * t::layout::maxScale),
                    static_cast<int>(t::layout::windowHeight * t::layout::maxScale));
    getConstrainer()->setFixedAspectRatio(static_cast<double>(t::layout::windowWidth) /
                                          t::layout::windowHeight);
    setSize(t::layout::windowWidth, t::layout::windowHeight);
    startTimerHz(30);
}

MagicDustEditor::~MagicDustEditor() { body_->setLookAndFeel(nullptr); }

void MagicDustEditor::resized()
{
    body_->setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) /
                                                     static_cast<float>(t::layout::windowWidth)));
}

void MagicDustEditor::timerCallback() { body_->refresh(); }
