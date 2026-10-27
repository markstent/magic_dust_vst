#include "PluginEditor.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "PluginProcessor.h"
#include "plugin/DragFiles.h"
#include "ui/Fonts.h"
#include "ui/KeyButton.h"
#include "ui/ScreenComponent.h"
#include "ui/SprinkleKey.h"
#include "ui/Tokens.h"

namespace t = md::tokens;

// The instrument body, drawn at the logical 1040 x 740 size; the editor scales it as a whole.
class MagicDustEditor::Body : public juce::Component
{
  public:
    explicit Body(MagicDustProcessor& processor)
        : processor_(processor), screen_(processor),
          playKey_("play", "M7 4.5l13 7.5-13 7.5z", t::layout::keyIconSize, t::gold),
          stopKey_("stop", "M5 7a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2z",
                   t::layout::stopIconSize, std::nullopt)
    {
        setBounds(0, 0, t::layout::windowWidth, t::layout::windowHeight);
        screen_.setBounds(t::layout::screenOuter);
        screen_.setExplicitFocusOrder(1);
        screen_.setOnStatus([this](std::optional<std::string> status)
                            { setFileError(std::move(status)); });
        addAndMakeVisible(screen_);

        addKey(playKey_,
               {.id = "playKey",
                .title = "play the preview",
                .bounds = {t::layout::playKeyX, t::layout::keyY, t::layout::keySize,
                           t::layout::keySize},
                .focusOrder = 2},
               [this] { processor_.play(); });
        addKey(stopKey_,
               {.id = "stopKey",
                .title = "stop the preview",
                .bounds = {t::layout::stopKeyX, t::layout::keyY, t::layout::keySize,
                           t::layout::keySize},
                .focusOrder = 3},
               [this] { processor_.stop(); });
        addKey(sprinkleKey_,
               {.id = "sprinkleKey",
                .title = "sprinkle a new idea",
                .bounds = {t::layout::sprinkleKeyX, t::layout::keyY, t::layout::sprinkleKeyWidth,
                           t::layout::keySize},
                .focusOrder = 4},
               [this]
               {
                   processor_.sprinkle();
                   screen_.refresh();
               });
        md::cleanDragFolder();
        refresh();
    }

    void refresh()
    {
        updateError();
        playKey_.setLit(processor_.previewPlaying());
        screen_.refresh();
    }

    void paint(juce::Graphics& g) override
    {
        const auto outer = getLocalBounds().toFloat();
        const auto inner = outer.reduced(t::bodyBorder);
        const auto radius = t::bodyRadius - t::bodyBorder;

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
    struct KeySpec
    {
        const char* id;
        const char* title;
        juce::Rectangle<int> bounds;
        int focusOrder;
    };

    // Places a key in the keys row; its click clears the status line, then does its action.
    void addKey(md::ui::KeyButton& key, const KeySpec& spec, std::function<void()> onPress)
    {
        key.setComponentID(spec.id);
        key.setTitle(spec.title);
        key.setExplicitFocusOrder(spec.focusOrder);
        key.setBounds(spec.bounds);
        key.onClick = [this, action = std::move(onPress)]
        {
            setFileError(std::nullopt);
            action();
        };
        addAndMakeVisible(key);
    }

    void setFileError(std::optional<std::string> status)
    {
        fileError_ = std::move(status);
        updateError();
    }

    void updateError()
    {
        const auto error = fileError_ ? fileError_ : processor_.lastError();
        if (error != error_)
        {
            error_ = error;
            repaint(t::layout::statusLine);
        }
    }

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
    md::ui::KeyButton playKey_;
    md::ui::KeyButton stopKey_;
    md::ui::SprinkleKey sprinkleKey_;
    std::optional<std::string> fileError_;
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

void MagicDustEditor::paint(juce::Graphics& g) { g.fillAll(t::body); }

void MagicDustEditor::resized()
{
    body_->setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) /
                                                     static_cast<float>(t::layout::windowWidth)));
}

void MagicDustEditor::timerCallback() { body_->refresh(); }
