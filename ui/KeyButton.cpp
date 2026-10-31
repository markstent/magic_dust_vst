#include "ui/KeyButton.h"

#include <algorithm>

#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kFrameHz = 60;

const KeyMaterial keycap{.top = t::console::keyTop,
                         .bottom = t::console::keyBottom,
                         .shadow = t::console::keyShadow,
                         .radius = t::layout::keyRadius,
                         .shadowDrop = t::layout::keyShadowDrop};

const KeyMaterial dark{.top = t::console::keyDarkTop,
                       .bottom = t::console::keyDarkBottom,
                       .shadow = t::console::keyDarkShadow,
                       .radius = t::layout::keyRadius,
                       .shadowDrop = t::layout::keyShadowDrop};

float easeOut(float x) { return 1.0F - ((1.0F - x) * (1.0F - x)); }

// value moved toward target by at most step.
float approach(float value, float target, float step)
{
    return std::clamp(target, value - step, value + step);
}

} // namespace

KeyButton::KeyButton(const juce::String& label, std::optional<juce::Colour> led, KeyIcon icon)
    : KeyButton(label, icon == KeyIcon::none ? keycap : dark, led)
{
    icon_ = icon;
}

KeyButton::KeyButton(const juce::String& label, const KeyMaterial& material,
                     std::optional<juce::Colour> led)
    : juce::Button(label), material_(material), label_(label), led_(led)
{
    // The shadow, legend and LED are drawn outside the key's bounds.
    setPaintingIsUnclipped(true);
}

bool KeyButton::keyPressed(const juce::KeyPress& key)
{
    // juce::Button answers Enter but not Space, and neither shows the key going down.
    if (!isActivateKey(key))
    {
        return false;
    }
    setState(juce::Button::buttonDown);
    const juce::Component::SafePointer<KeyButton> self(this);
    juce::Timer::callAfterDelay(static_cast<int>(t::keyPressMs),
                                [self]
                                {
                                    if (self != nullptr)
                                    {
                                        self->setState(juce::Button::buttonNormal);
                                    }
                                });
    if (getClickingTogglesState())
    {
        setToggleState(!getToggleState(), juce::sendNotificationSync);
    }
    else if (onClick)
    {
        onClick();
    }
    return true;
}

void KeyButton::clicked()
{
    if (getClickingTogglesState())
    {
        setLit(getToggleState());
    }
}

void KeyButton::setLit(bool lit)
{
    if (lit != lit_)
    {
        lit_ = lit;
        retarget();
    }
}

void KeyButton::setAnimated(bool animated)
{
    animated_ = animated;
    retarget();
}

void KeyButton::buttonStateChanged() { retarget(); }

void KeyButton::retarget()
{
    if (animated_)
    {
        if (juce::approximatelyEqual(press_, isDown() ? 1.0F : 0.0F) &&
            juce::approximatelyEqual(ledLevel_, lit_ ? 1.0F : 0.0F))
        {
            return;
        }
        lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(kFrameHz);
        return;
    }
    stopTimer();
    press_ = isDown() ? 1.0F : 0.0F;
    ledLevel_ = lit_ ? 1.0F : 0.0F;
    repaintWithMargin();
}

// Component::repaint clips to the key's own bounds, so the LED, shadow and legend outside them
// are invalidated through the parent.
void KeyButton::repaintWithMargin()
{
    const auto area = getBounds().expanded(0, t::layout::keyRepaintMargin);
    if (auto* parent = getParentComponent())
    {
        parent->repaint(area);
        return;
    }
    repaint();
}

void KeyButton::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = now - lastTickMs_;
    lastTickMs_ = now;
    const auto pressTarget = isDown() ? 1.0F : 0.0F;
    const auto ledTarget = lit_ ? 1.0F : 0.0F;
    press_ = approach(press_, pressTarget, static_cast<float>(elapsed / t::keyPressMs));
    ledLevel_ = approach(ledLevel_, ledTarget, static_cast<float>(elapsed / t::ledFadeMs));
    if (juce::approximatelyEqual(press_, pressTarget) &&
        juce::approximatelyEqual(ledLevel_, ledTarget))
    {
        stopTimer();
    }
    repaintWithMargin();
}

void KeyButton::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    const bool disabled = !isEnabled();
    if (disabled)
    {
        g.beginTransparencyLayer(t::layout::disabledAlpha);
    }
    const auto pressed = easeOut(press_);
    const auto rest = getLocalBounds().toFloat();
    const auto face = rest.translated(0.0F, t::layout::keyPressDrop * pressed);
    const auto radius = material_.radius;

    // The shadow's lower edge stays put while the face drops onto it.
    g.setColour(material_.shadow);
    g.fillRoundedRectangle(
        face.translated(0.0F, material_.shadowDrop - (t::layout::keyPressDrop * pressed)), radius);

    g.setGradientFill(juce::ColourGradient::vertical(material_.top, face.getY(), material_.bottom,
                                                     face.getBottom()));
    g.fillRoundedRectangle(face, radius);

    paintContent(g, face);

    const auto legend = font(Face::groteskMedium, t::layout::legendSize)
                            .withExtraKerningFactor(t::layout::legendSpacing);
    g.setColour(t::console::legend);
    g.setFont(legend);
    g.drawText(label_.toUpperCase(), legendBounds(), juce::Justification::centred, false);

    if (hasKeyboardFocus(true))
    {
        const auto stroke = t::layout::focusRingStroke;
        g.setColour(t::console::legend);
        g.drawRoundedRectangle(face.expanded(t::layout::focusRingOffset + (stroke / 2.0F)),
                               radius + t::layout::focusRingOffset, stroke);
    }

    if (led_)
    {
        const auto size = t::layout::ledSize;
        const auto spot =
            juce::Rectangle<float>(rest.getCentreX() - (size / 2.0F),
                                   rest.getY() - t::layout::keyLedGap - size, size, size);
        if (ledLevel_ > 0.0F)
        {
            juce::Path dot;
            dot.addEllipse(spot);
            juce::DropShadow(led_->withAlpha(ledLevel_), static_cast<int>(t::layout::ledGlow),
                             {0, 0})
                .drawForPath(g, dot);
        }
        g.setColour(t::console::ledOff.interpolatedWith(*led_, ledLevel_));
        g.fillEllipse(spot);
    }
    if (disabled)
    {
        g.endTransparencyLayer();
    }
}

juce::Rectangle<float> KeyButton::legendBounds() const
{
    const auto rest = getLocalBounds().toFloat();
    return {rest.getX() - t::layout::keyLegendOverhang, rest.getBottom() + t::layout::keyLegendGap,
            rest.getWidth() + (2.0F * t::layout::keyLegendOverhang),
            static_cast<float>(t::layout::legendHeight)};
}

void KeyButton::paintContent(juce::Graphics& g, juce::Rectangle<float> face) const
{
    if (icon_ == KeyIcon::none)
    {
        return;
    }
    g.setColour(t::console::legend);
    if (icon_ == KeyIcon::arrowDown || icon_ == KeyIcon::arrowUp)
    {
        const auto arrow =
            juce::Rectangle<float>(t::layout::octaveArrowWidth, t::layout::octaveArrowHeight)
                .withCentre(face.getCentre());
        juce::Path triangle;
        const bool down = icon_ == KeyIcon::arrowDown;
        triangle.addTriangle(down ? arrow.getTopLeft() : arrow.getBottomLeft(),
                             down ? arrow.getTopRight() : arrow.getBottomRight(),
                             {arrow.getCentreX(), down ? arrow.getBottom() : arrow.getY()});
        g.fillPath(triangle);
        return;
    }
    const auto size = t::layout::keyIconSize;
    const auto box = juce::Rectangle<float>(size, size).withCentre(face.getCentre());
    if (icon_ == KeyIcon::stop)
    {
        g.fillRect(box);
        return;
    }
    juce::Path triangle;
    triangle.addTriangle(box.getTopLeft(), box.getBottomLeft(), {box.getRight(), box.getCentreY()});
    g.fillPath(triangle);
}

} // namespace md::ui
