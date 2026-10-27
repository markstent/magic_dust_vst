#include "PluginEditor.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "PluginProcessor.h"
#include "plugin/DragFiles.h"
#include "ui/BodyMaterial.h"
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
        md::ui::paintBody(g, getLocalBounds().toFloat());

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

    MagicDustProcessor& processor_;
    md::ui::ScreenComponent screen_;
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
}

// Builds the view for the instance's mode, and sizes and limits the window to suit it.
void MagicDustEditor::showView()
{
    const auto sender = processor_.mode() == InstanceMode::sender;
    if (body_ != nullptr)
    {
        body_->setLookAndFeel(nullptr);
    }
    body_.reset();
    sender_.reset();
    if (sender)
    {
        sender_ = std::make_unique<md::ui::SenderBody>(processor_);
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
    updateScale();
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
