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
#include "ui/OctaveRocker.h"
#include "ui/ScreenComponent.h"
#include "ui/SprinkleKey.h"
#include "ui/Tokens.h"
#include "ui/TopLine.h"

namespace t = md::tokens;

namespace
{

constexpr float kWordmarkRingRadius = 3.0F;
constexpr const char* kFollowMessage = "FOLLOWING YOUR DAW - USE ITS TRANSPORT";

constexpr std::array<md::LaneId, 3> kLanes{md::LaneId::chords, md::LaneId::bass,
                                           md::LaneId::melody};

constexpr std::array<const char*, 3> kMuteIds{"chordsMute", "bassMute", "melodyMute"};
constexpr std::array<const char*, 3> kPinIds{"chordsPin", "bassPin", "melodyPin"};
constexpr std::array<const char*, 3> kOctaveIds{"chordsOctave", "bassOctave", "melodyOctave"};

md::ui::KnobLook knobLook(const char* legend, juce::StringArray rangeWords,
                          juce::StringArray stepWords)
{
    // A stepped knob's step words already lift its legend; the others get the same lift.
    const auto gap = stepWords.isEmpty() ? t::layout::knobLegendGap : 0.0F;
    return {.legend = legend,
            .rangeWords = std::move(rangeWords),
            .stepWords = std::move(stepWords),
            .cap = t::console::knobCap,
            .diameter = t::layout::knobDiameter,
            .resetValue = std::nullopt,
            .legendGap = gap,
            .subLegend = {}};
}

md::ui::KnobLook levelLook(md::LaneId lane)
{
    return {.legend = juce::String(md::ui::laneLook(lane).label).toUpperCase(),
            .rangeWords = {},
            .stepWords = {},
            .cap = md::ui::laneLook(lane).colour,
            .diameter = t::layout::levelKnobDiameter,
            .resetValue = 1.0,
            .legendGap = t::layout::levelLegendGap,
            .subLegend = "LEVEL"};
}

md::ui::KnobLook volumeLook()
{
    return {.legend = "VOLUME",
            .rangeWords = {"MIN", "MAX"},
            .stepWords = {},
            .cap = t::console::knobCap,
            .diameter = t::layout::knobDiameter,
            .resetValue = 0.8,
            .legendGap = t::layout::knobLegendGap,
            .subLegend = {}};
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
          muteKeys_{md::ui::KeyButton(md::ui::laneLook(kLanes.at(0)).label, t::console::ledOn),
                    md::ui::KeyButton(md::ui::laneLook(kLanes.at(1)).label, t::console::ledOn),
                    md::ui::KeyButton(md::ui::laneLook(kLanes.at(2)).label, t::console::ledOn)},
          pinKeys_{md::ui::KeyButton(md::ui::laneLook(kLanes.at(0)).label,
                                     md::ui::laneLook(kLanes.at(0)).colour),
                   md::ui::KeyButton(md::ui::laneLook(kLanes.at(1)).label,
                                     md::ui::laneLook(kLanes.at(1)).colour),
                   md::ui::KeyButton(md::ui::laneLook(kLanes.at(2)).label,
                                     md::ui::laneLook(kLanes.at(2)).colour)},
          rockers_{md::ui::OctaveRocker(processor.params, kOctaveIds.at(0), "chords", 0),
                   md::ui::OctaveRocker(processor.params, kOctaveIds.at(1), "bass", 1),
                   md::ui::OctaveRocker(processor.params, kOctaveIds.at(2), "melody", 2)},
          playKey_("play", t::console::ledOn, md::ui::KeyIcon::play),
          stopKey_("stop", std::nullopt, md::ui::KeyIcon::stop)
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
        const auto& centreX = t::layout::knobCentreX;
        const auto centreY = t::layout::knobCentreY;
        addKnob(mood_, "moodKnob", {centreX.at(0), centreY}, 4);
        addKnob(energy_, "energyKnob", {centreX.at(1), centreY}, 5);
        addKnob(groove_, "grooveKnob", {centreX.at(2), centreY}, 6);
        addKnob(flavour_, "flavourKnob", {centreX.at(3), centreY}, 7);
        addLevelKnobs();
        addKnob(volume_, "volumeKnob", t::layout::volumeCentre, 11);
        addMuteKeys();

        addKey(playKey_,
               {.id = "playKey",
                .title = "play the preview",
                .bounds = {t::layout::transportKeyX.at(0), t::layout::keyY, t::layout::keyWidth,
                           t::layout::keyHeight},
                .focusOrder = 21},
               [this] { processor_.play(); });
        addKey(stopKey_,
               {.id = "stopKey",
                .title = "stop the preview",
                .bounds = {t::layout::transportKeyX.at(1), t::layout::keyY, t::layout::keyWidth,
                           t::layout::keyHeight},
                .focusOrder = 22},
               [this] { processor_.stop(); });
        addKey(sprinkleKey_,
               {.id = "sprinkleKey",
                .title = "sprinkle a new idea",
                .bounds = t::layout::sprinkleKey,
                .focusOrder = 23},
               [this]
               {
                   processor_.sprinkle();
                   screen_.refresh();
               });
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
        for (auto& rocker : rockers_)
        {
            rocker.refresh();
        }
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

        paintGroupPanel(g, t::layout::knobPanel, t::layout::knobPanelRadius);
        g.setColour(juce::Colours::white.withAlpha(t::layout::dividerAlpha));
        for (const auto x : t::layout::dividerX)
        {
            g.fillRect(x, t::layout::dividerY, 1, t::layout::dividerHeight);
        }
        paintVerticalLegend(g, "MUTE", t::layout::muteLegendCentreX);
        paintVerticalLegend(g, "PIN", t::layout::pinLegendCentreX);
        paintOctaveLegend(g);

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

    // Places a knob so that its face is centred on the given point.
    void addKnob(md::ui::Knob& knob, const char* id, juce::Point<int> faceCentre, int focusOrder)
    {
        knob.setComponentID(id);
        knob.setExplicitFocusOrder(focusOrder);
        knob.setSize(t::layout::knobWidth, t::layout::knobHeight);
        knob.setTopLeftPosition(faceCentre.x - (knob.getWidth() / 2),
                                faceCentre.y - knob.faceBounds().getCentreY());
        addAndMakeVisible(knob);
    }

    void addLevelKnobs()
    {
        for (std::size_t i = 0; i < levels_.size(); ++i)
        {
            auto& knob = levels_.at(i);
            knob.setTitle(juce::String(md::ui::laneLook(kLanes.at(i)).label) + " level");
            knob.setSize(t::layout::levelKnobWidth, t::layout::levelKnobHeight);
            knob.setComponentID("level" + juce::String(i));
            knob.setExplicitFocusOrder(8 + static_cast<int>(i));
            knob.setTopLeftPosition(t::layout::levelCentreX.at(i) - (knob.getWidth() / 2),
                                    t::layout::levelCentreY - knob.faceBounds().getCentreY());
            addAndMakeVisible(knob);
        }
    }

    void addMuteKeys()
    {
        for (std::size_t i = 0; i < muteKeys_.size(); ++i)
        {
            auto& mute = muteKeys_.at(i);
            mute.setComponentID("mute" + juce::String(i));
            mute.setTitle(juce::String(md::ui::laneLook(kLanes.at(i)).label) + " mute");
            mute.setExplicitFocusOrder(12 + (3 * static_cast<int>(i)));
            mute.setClickingTogglesState(true);
            mute.setBounds(t::layout::muteKeyX.at(i), t::layout::keyY, t::layout::keyWidth,
                           t::layout::keyHeight);
            muteAttachments_.at(i) = std::make_unique<juce::ButtonParameterAttachment>(
                *processor_.params.getParameter(kMuteIds.at(i)), mute);
            addAndMakeVisible(mute);

            auto& rocker = rockers_.at(i);
            rocker.setExplicitFocusOrder(13 + (3 * static_cast<int>(i)));
            rocker.setTopLeftPosition(t::layout::muteKeyX.at(i), t::layout::octaveY);
            addAndMakeVisible(rocker);

            auto& pin = pinKeys_.at(i);
            pin.setComponentID("pin" + juce::String(i));
            pin.setTitle("pin " + juce::String(md::ui::laneLook(kLanes.at(i)).label));
            pin.setExplicitFocusOrder(14 + (3 * static_cast<int>(i)));
            pin.setClickingTogglesState(true);
            pin.setBounds(t::layout::pinKeyX.at(i), t::layout::keyY, t::layout::pinKeyWidth,
                          t::layout::keyHeight);
            pinAttachments_.at(i) = std::make_unique<juce::ButtonParameterAttachment>(
                *processor_.params.getParameter(kPinIds.at(i)), pin);
            addAndMakeVisible(pin);
        }
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

    // "MUTE" or "PIN" reading bottom to top, centred left of its keys, in the key legend style.
    static void paintVerticalLegend(juce::Graphics& g, const char* text, int centreX)
    {
        const auto centre = juce::Point<float>(
            static_cast<float>(centreX), static_cast<float>(t::layout::keyY) +
                                             (static_cast<float>(t::layout::keyHeight) / 2.0F));
        const juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                       centre.x, centre.y));
        g.setColour(t::console::legend);
        g.setFont(md::ui::font(md::ui::Face::groteskMedium, t::layout::legendSize)
                      .withExtraKerningFactor(t::layout::legendSpacing));
        g.drawText(text,
                   juce::Rectangle<float>(static_cast<float>(t::layout::legendHeight) * 5.0F,
                                          static_cast<float>(t::layout::legendHeight))
                       .withCentre(centre),
                   juce::Justification::centred, false);
    }

    // "OCT" left of the first rocker, centred on the same line as the MUTE legend above it.
    static void paintOctaveLegend(juce::Graphics& g)
    {
        g.setColour(t::console::legend);
        g.setFont(md::ui::font(md::ui::Face::groteskMedium, t::layout::legendSize)
                      .withExtraKerningFactor(t::layout::legendSpacing));
        const auto centre = juce::Point<int>(t::layout::octaveLegendCentreX,
                                             t::layout::octaveY + (t::layout::octaveHeight / 2));
        g.drawText("OCT",
                   juce::Rectangle<int>(t::layout::legendHeight * 3, t::layout::legendHeight)
                       .withCentre(centre),
                   juce::Justification::centred, false);
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
    std::array<md::ui::KeyButton, 3> pinKeys_;
    std::array<std::unique_ptr<juce::ButtonParameterAttachment>, 3> pinAttachments_;
    std::array<md::ui::OctaveRocker, 3> rockers_;
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
