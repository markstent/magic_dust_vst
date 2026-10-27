#include "PluginEditor.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "PluginProcessor.h"
#include "plugin/DragFiles.h"
#include "ui/AboutPanel.h"
#include "ui/Boilerplate.h"
#include "ui/Faceplate.h"
#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/KeyButton.h"
#include "ui/Knob.h"
#include "ui/LaneView.h"
#include "ui/ScreenComponent.h"
#include "ui/SprinkleKey.h"
#include "ui/Tokens.h"
#include "ui/TopLine.h"

namespace t = md::tokens;

namespace
{

constexpr float kWordmarkRingRadius = 3.0F;
constexpr int kContinuousKnobHeight = 100;
constexpr int kSteppedKnobHeight = 112;
constexpr const char* kFollowMessage = "FOLLOWING YOUR DAW - USE ITS TRANSPORT";

constexpr std::array<md::LaneId, 3> kLanes{md::LaneId::chords, md::LaneId::bass,
                                           md::LaneId::melody};

constexpr std::array<const char*, 3> kMuteIds{"chordsMute", "bassMute", "melodyMute"};

md::ui::KnobLook knobLook(const char* legend, juce::StringArray rangeWords,
                          juce::StringArray stepWords)
{
    return {.legend = legend,
            .rangeWords = std::move(rangeWords),
            .stepWords = std::move(stepWords),
            .cap = t::console::knobCap,
            .diameter = t::layout::knobDiameter,
            .resetValue = std::nullopt};
}

md::ui::KnobLook levelLook(md::LaneId lane)
{
    return {.legend = "LEVEL",
            .rangeWords = {},
            .stepWords = {},
            .cap = md::ui::laneLook(lane).colour,
            .diameter = t::layout::levelKnobDiameter,
            .resetValue = 1.0};
}

md::ui::KnobLook volumeLook()
{
    return {.legend = "VOLUME",
            .rangeWords = {"MIN", "MAX"},
            .stepWords = {},
            .cap = t::console::knobCap,
            .diameter = t::layout::volumeKnobDiameter,
            .resetValue = 0.8,
            .legendGap = t::layout::volumeLegendGap};
}

// The wordmark as a button: it only draws its focus ring, the body paints the lettering.
class WordmarkButton : public juce::Button
{
  public:
    WordmarkButton() : juce::Button("about magic dust")
    {
        setComponentID("wordmark");
        setTitle("about magic dust");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/) override
    {
        if (hasKeyboardFocus(true))
        {
            md::ui::drawFocusRing(g, getLocalBounds().toFloat(), kWordmarkRingRadius,
                                  t::console::legend);
        }
    }

    MD_COMPONENT_BOILERPLATE(WordmarkButton)
};

} // namespace

// The instrument body, drawn at the logical 1040 x 600 size; the editor scales it as a whole.
class MagicDustEditor::Body : public juce::Component
{
  public:
    explicit Body(MagicDustProcessor& processor)
        : processor_(processor), topLine_(processor), screen_(processor),
          mood_(processor.params, "mood", knobLook("MOOD", {"DARK", "BRIGHT"}, {})),
          energy_(processor.params, "energy", knobLook("ENERGY", {"LOW", "HIGH"}, {})),
          groove_(processor.params, "groove",
                  knobLook("GROOVE", {}, {"STRAIGHT", "54", "58", "62"})),
          flavour_(processor.params, "flavour",
                   knobLook("FLAVOUR", {}, {"SIMPLE", "SOULFUL", "JAZZY"})),
          levels_{md::ui::Knob(processor.params, "chordsLevel", levelLook(kLanes.at(0))),
                  md::ui::Knob(processor.params, "bassLevel", levelLook(kLanes.at(1))),
                  md::ui::Knob(processor.params, "melodyLevel", levelLook(kLanes.at(2)))},
          volume_(processor.params, "volume", volumeLook()),
          muteKeys_{md::ui::KeyButton("mute", t::console::ledOn),
                    md::ui::KeyButton("mute", t::console::ledOn),
                    md::ui::KeyButton("mute", t::console::ledOn)},
          playKey_("play", t::console::ledOn), stopKey_("stop", std::nullopt)
    {
        setBounds(0, 0, t::layout::windowWidth, t::layout::windowHeight);
        wordmark_.setBounds(md::ui::wordmarkBounds());
        wordmark_.setExplicitFocusOrder(1);
        wordmark_.onClick = [this] { about_.open(); };
        addAndMakeVisible(wordmark_);
        topLine_.setBounds(t::layout::header);
        topLine_.setExplicitFocusOrder(2);
        addAndMakeVisible(topLine_);
        screen_.setBounds(t::layout::display);
        screen_.setExplicitFocusOrder(3);
        screen_.setOnStatus([this](std::optional<std::string> status)
                            { setFileError(std::move(status)); });
        addAndMakeVisible(screen_);
        addKnob(mood_, "moodKnob", 0, 0, 4);
        addKnob(energy_, "energyKnob", 1, 0, 5);
        addKnob(groove_, "grooveKnob", 0, 1, 6);
        addKnob(flavour_, "flavourKnob", 1, 1, 7);

        addLaneBandControls();

        addKey(playKey_,
               {.id = "playKey",
                .title = "play the preview",
                .bounds = t::layout::playKey,
                .focusOrder = 14},
               [this] { processor_.play(); });
        addKey(stopKey_,
               {.id = "stopKey",
                .title = "stop the preview",
                .bounds = t::layout::stopKey,
                .focusOrder = 15},
               [this] { processor_.stop(); });
        addKey(sprinkleKey_,
               {.id = "sprinkleKey",
                .title = "sprinkle a new idea",
                .bounds = t::layout::sprinkleKey,
                .focusOrder = 16},
               [this]
               {
                   processor_.sprinkle();
                   screen_.refresh();
               });
        volume_.setComponentID("volumeKnob");
        volume_.setExplicitFocusOrder(17);
        volume_.setBounds(t::layout::volumeKnobCentreX - (t::layout::volumeKnobWidth / 2),
                          t::layout::bandKnobCentreY - volume_.faceBounds().getCentreY(),
                          t::layout::volumeKnobWidth, t::layout::volumeKnobHeight);
        addAndMakeVisible(volume_);
        about_.setBounds(t::layout::aboutPanel);
        about_.setOnClose([this] { wordmark_.grabKeyboardFocus(); });
        addChildComponent(about_);
        addMouseListener(this, true); // a click on anything in the body closes the about panel
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

    void togglePreview()
    {
        setFileError(std::nullopt);
        if (processor_.previewPlaying())
        {
            processor_.stop();
        }
        else
        {
            processor_.play();
        }
        updateError();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.eventComponent != &about_ && !about_.isParentOf(event.eventComponent))
        {
            about_.dismiss();
        }
    }

    void paint(juce::Graphics& g) override
    {
        md::ui::paintFaceplate(g, getLocalBounds().toFloat());

        paintGroupPanel(g, t::layout::knobGroup, t::layout::knobGroupRadius);
        paintGroupPanel(g, t::layout::laneGroup, t::layout::laneGroupRadius);
        paintGroupPanel(g, t::layout::transportGroup, t::layout::laneGroupRadius);
        paintLaneLegends(g);

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

    // Places each lane column's level knob and mute key, after the four knobs in the focus order.
    void addLaneBandControls()
    {
        for (std::size_t i = 0; i < levels_.size(); ++i)
        {
            auto& knob = levels_.at(i);
            knob.setComponentID("level" + juce::String(i));
            knob.setTitle(juce::String(md::ui::laneLook(kLanes.at(i)).label) + " level");
            knob.setExplicitFocusOrder(8 + (2 * static_cast<int>(i)));
            knob.setSize(t::layout::levelKnobWidth, t::layout::levelKnobHeight);
            knob.setTopLeftPosition(laneColumnX(i) + t::layout::levelKnobOffsetX -
                                        (t::layout::levelKnobWidth / 2),
                                    t::layout::bandKnobCentreY - knob.faceBounds().getCentreY());
            addAndMakeVisible(knob);

            auto& mute = muteKeys_.at(i);
            mute.setComponentID("mute" + juce::String(i));
            mute.setTitle(juce::String(md::ui::laneLook(kLanes.at(i)).label) + " mute");
            mute.setExplicitFocusOrder(9 + (2 * static_cast<int>(i)));
            mute.setClickingTogglesState(true);
            mute.setBounds(laneColumnX(i) + t::layout::muteKeyOffsetX,
                           t::layout::bandKnobCentreY - (t::layout::muteKeyHeight / 2),
                           t::layout::muteKeyWidth, t::layout::muteKeyHeight);
            muteAttachments_.at(i) = std::make_unique<juce::ButtonParameterAttachment>(
                *processor_.params.getParameter(kMuteIds.at(i)), mute);
            addAndMakeVisible(mute);
        }
    }

    static int laneColumnX(std::size_t column)
    {
        return t::layout::laneColumnX + (static_cast<int>(column) * t::layout::laneColumnWidth);
    }

    // A darker panel with a light edge, behind a group of knobs.
    static void paintGroupPanel(juce::Graphics& g, juce::Rectangle<int> bounds, float radius)
    {
        const auto panel = bounds.toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.12F));
        g.fillRoundedRectangle(panel, radius);
        g.setColour(juce::Colours::white.withAlpha(0.12F));
        g.drawRoundedRectangle(panel.reduced(0.5F), radius, 1.0F);
    }

    // The lane names above each column's knobs, in the knobs' own legend style.
    static void paintLaneLegends(juce::Graphics& g)
    {
        g.setColour(t::console::legend);
        g.setFont(md::ui::font(md::ui::Face::groteskBold, t::layout::laneLegendSize)
                      .withExtraKerningFactor(t::layout::laneLegendSpacing));
        for (std::size_t i = 0; i < kLanes.size(); ++i)
        {
            g.drawText(
                juce::String(md::ui::laneLook(kLanes.at(i)).label).toUpperCase(),
                juce::Rectangle<int>(laneColumnX(i),
                                     t::layout::laneLegendCentreY - (t::layout::legendHeight / 2),
                                     t::layout::laneColumnWidth, t::layout::legendHeight),
                juce::Justification::centred, false);
        }
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
            updateError(); // a key pressed while the daw plays raises the follow notice
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
        std::optional<std::string> error = fileError_ ? fileError_ : processor_.lastError();
        if (processor_.followNotice())
        {
            error = kFollowMessage;
        }
        if (error != error_)
        {
            error_ = std::move(error);
            screen_.setError(error_);
        }
    }

    MagicDustProcessor& processor_;
    WordmarkButton wordmark_;
    md::ui::AboutPanel about_;
    md::ui::TopLine topLine_;
    md::ui::ScreenComponent screen_;
    md::ui::Knob mood_;
    md::ui::Knob energy_;
    md::ui::Knob groove_;
    md::ui::Knob flavour_;
    std::array<md::ui::Knob, 3> levels_;
    md::ui::Knob volume_;
    std::array<md::ui::KeyButton, 3> muteKeys_;
    std::array<std::unique_ptr<juce::ButtonParameterAttachment>, 3> muteAttachments_;
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
    setWantsKeyboardFocus(true); // a click on any non-focusable part lands here
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

void MagicDustEditor::paint(juce::Graphics& g) { g.fillAll(t::console::faceBottom); }

void MagicDustEditor::resized() { updateScale(); }

bool MagicDustEditor::keyPressed(const juce::KeyPress& key)
{
    if (key != juce::KeyPress::spaceKey || body_ == nullptr)
    {
        return false;
    }
    body_->togglePreview();
    return true;
}

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

void MagicDustEditor::timerCallback() { refresh(); }

void MagicDustEditor::refresh()
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
