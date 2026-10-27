#include "PluginEditor.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "PluginProcessor.h"
#include "plugin/DragFiles.h"
#include "ui/Faceplate.h"
#include "ui/Fonts.h"
#include "ui/KeyButton.h"
#include "ui/Knob.h"
#include "ui/ScreenComponent.h"
#include "ui/SprinkleKey.h"
#include "ui/Tokens.h"
#include "ui/TopLine.h"

namespace t = md::tokens;

namespace
{

// The knobs' cap colour: the console's neutral grey.
const juce::Colour kKnobCap{0xFF8A8F96};

constexpr int kContinuousKnobHeight = 100;
constexpr int kSteppedKnobHeight = 112;

md::ui::KnobLook knobLook(const char* legend, juce::StringArray rangeWords,
                          juce::StringArray stepWords)
{
    return {.legend = legend,
            .rangeWords = std::move(rangeWords),
            .stepWords = std::move(stepWords),
            .cap = kKnobCap,
            .diameter = t::layout::knobDiameter,
            .resetValue = std::nullopt};
}

} // namespace

// The instrument body, drawn at the logical 1040 x 600 size; the editor scales it as a whole.
class MagicDustEditor::Body : public juce::Component
{
  public:
    explicit Body(MagicDustProcessor& processor)
        : processor_(processor), topLine_(processor), screen_(processor),
          mood_(processor.params, "mood", knobLook("MOOD", {"DARK", "BRIGHT"}, {})),
          energy_(processor.params, "energy", knobLook("ENERGY", {"LOW", "HIGH"}, {})),
          groove_(processor.params, "groove", knobLook("GROOVE", {}, {"STR", "54", "58", "62"})),
          flavour_(processor.params, "flavour",
                   knobLook("FLAVOUR", {}, {"SIMPLE", "SOULFUL", "JAZZY"})),
          playKey_("play", "M7 4.5l13 7.5-13 7.5z", t::layout::keyIconSize, t::gold),
          stopKey_("stop", "M5 7a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2z",
                   t::layout::stopIconSize, std::nullopt)
    {
        setBounds(0, 0, t::layout::windowWidth, t::layout::windowHeight);
        topLine_.setBounds(t::layout::header);
        topLine_.setExplicitFocusOrder(1);
        addAndMakeVisible(topLine_);
        screen_.setBounds(t::layout::display);
        screen_.setExplicitFocusOrder(2);
        screen_.setOnStatus([this](std::optional<std::string> status)
                            { setFileError(std::move(status)); });
        addAndMakeVisible(screen_);
        addKnob(mood_, "moodKnob", 0, 0, 3);
        addKnob(energy_, "energyKnob", 1, 0, 4);
        addKnob(groove_, "grooveKnob", 0, 1, 5);
        addKnob(flavour_, "flavourKnob", 1, 1, 6);

        addKey(playKey_,
               {.id = "playKey",
                .title = "play the preview",
                .bounds = t::layout::playKey,
                .focusOrder = 7},
               [this] { processor_.play(); });
        addKey(stopKey_,
               {.id = "stopKey",
                .title = "stop the preview",
                .bounds = t::layout::stopKey,
                .focusOrder = 8},
               [this] { processor_.stop(); });
        addKey(sprinkleKey_,
               {.id = "sprinkleKey",
                .title = "sprinkle a new idea",
                .bounds = t::layout::sprinkleKey,
                .focusOrder = 9},
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
        topLine_.refresh();
        screen_.refresh();
    }

    void paint(juce::Graphics& g) override
    {
        md::ui::paintFaceplate(g, getLocalBounds().toFloat());

        const auto group = t::layout::knobGroup.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.12F));
        g.fillRoundedRectangle(group, t::layout::knobGroupRadius);
        g.setColour(juce::Colours::white.withAlpha(0.12F));
        g.drawRoundedRectangle(group.reduced(0.5F), t::layout::knobGroupRadius, 1.0F);

        md::ui::paintWordmark(g);
    }

  private:
    struct KeySpec
    {
        const char* id;
        const char* title;
        juce::Rectangle<int> bounds;
        int focusOrder;
    };

    // Places a knob by its column and row in the knob group.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void addKnob(md::ui::Knob& knob, const char* id, std::size_t column, std::size_t row,
                 int focusOrder)
    {
        knob.setComponentID(id);
        knob.setExplicitFocusOrder(focusOrder);
        knob.setSize(t::layout::knobWidth, row == 0 ? kContinuousKnobHeight : kSteppedKnobHeight);
        knob.setTopLeftPosition(t::layout::knobColumnX.at(column) - (t::layout::knobWidth / 2),
                                t::layout::knobFaceY.at(row) - knob.faceBounds().getY());
        addAndMakeVisible(knob);
    }

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
            screen_.setError(error_);
        }
    }

    MagicDustProcessor& processor_;
    md::ui::TopLine topLine_;
    md::ui::ScreenComponent screen_;
    md::ui::Knob mood_;
    md::ui::Knob energy_;
    md::ui::Knob groove_;
    md::ui::Knob flavour_;
    md::ui::KeyButton playKey_;
    md::ui::KeyButton stopKey_;
    md::ui::SprinkleKey sprinkleKey_;
    std::optional<std::string> fileError_;
    std::optional<std::string> error_;
};

MagicDustEditor::MagicDustEditor(MagicDustProcessor& owner)
    : AudioProcessorEditor(owner), processor_(owner)
{
    setResizable(true, false);
    showView();
    startTimerHz(30);
}

MagicDustEditor::~MagicDustEditor()
{
    if (body_ != nullptr)
    {
        body_->setLookAndFeel(nullptr);
    }
    if (sender_ != nullptr)
    {
        sender_->setLookAndFeel(nullptr);
    }
}

// Builds the view for the instance's mode, and sizes and limits the window to suit it.
void MagicDustEditor::showView()
{
    const auto sender = processor_.mode() == InstanceMode::sender;
    if (body_ != nullptr)
    {
        body_->setLookAndFeel(nullptr);
    }
    if (sender_ != nullptr)
    {
        sender_->setLookAndFeel(nullptr);
    }
    body_.reset();
    sender_.reset();
    if (sender)
    {
        sender_ = std::make_unique<md::ui::SenderBody>(processor_);
        sender_->setLookAndFeel(&lookAndFeel_);
        addAndMakeVisible(*sender_);
    }
    else
    {
        body_ = std::make_unique<Body>(processor_);
        body_->setLookAndFeel(&lookAndFeel_);
        addAndMakeVisible(*body_);
    }

    const auto width = sender ? t::layout::senderWidth : t::layout::windowWidth;
    const auto height = sender ? t::layout::senderHeight : t::layout::windowHeight;
    setResizeLimits(static_cast<int>(static_cast<float>(width) * t::layout::minScale),
                    static_cast<int>(static_cast<float>(height) * t::layout::minScale),
                    static_cast<int>(static_cast<float>(width) * t::layout::maxScale),
                    static_cast<int>(static_cast<float>(height) * t::layout::maxScale));
    getConstrainer()->setFixedAspectRatio(static_cast<double>(width) / height);
    setSize(width, height);
    updateScale(); // setSize does not call resized() when the size is unchanged
}

void MagicDustEditor::sync()
{
    if ((processor_.mode() == InstanceMode::sender) != (sender_ != nullptr))
    {
        showView();
    }
}

void MagicDustEditor::paint(juce::Graphics& g) { g.fillAll(t::body); }

void MagicDustEditor::resized() { updateScale(); }

// The view is laid out at its logical size; the editor scales it as a whole.
void MagicDustEditor::updateScale()
{
    const auto logicalWidth = sender_ != nullptr ? t::layout::senderWidth : t::layout::windowWidth;
    const auto transform = juce::AffineTransform::scale(static_cast<float>(getWidth()) /
                                                        static_cast<float>(logicalWidth));
    if (sender_ != nullptr)
    {
        sender_->setTransform(transform);
    }
    else if (body_ != nullptr)
    {
        body_->setTransform(transform);
    }
}

void MagicDustEditor::timerCallback()
{
    sync();
    if (sender_ != nullptr)
    {
        sender_->refresh();
    }
    else
    {
        body_->refresh();
    }
}
