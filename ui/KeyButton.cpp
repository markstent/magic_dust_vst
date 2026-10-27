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

const KeyMaterial keycap{.top = t::keyTop,
                         .bottom = t::keyBottom,
                         .edge = t::keyEdge,
                         .base = t::keyBase,
                         .shadowBlur = t::layout::keycapShadowBlur,
                         .shadowAlpha = t::layout::keycapShadowAlpha};

float easeOut(float x) { return 1.0F - ((1.0F - x) * (1.0F - x)); }

// value moved toward target by at most step.
float approach(float value, float target, float step)
{
    return std::clamp(target, value - step, value + step);
}

float lerp(float from, float to, float amount) { return from + ((to - from) * amount); }

} // namespace

KeyButton::KeyButton(const juce::String& label, const char* iconPath, float iconSize,
                     std::optional<juce::Colour> led)
    : KeyButton(label, keycap, led)
{
    icon_ = juce::Drawable::parseSVGPath(iconPath);
    iconSize_ = iconSize;
}

KeyButton::KeyButton(const juce::String& label, std::optional<juce::Colour> led)
    : KeyButton(label, keycap, led)
{
}

KeyButton::KeyButton(const juce::String& label, const KeyMaterial& material,
                     std::optional<juce::Colour> led)
    : juce::Button(label), material_(material), label_(label), led_(led)
{
    // The base and soft shadow are drawn below the key's bounds.
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
    if (onClick)
    {
        onClick();
    }
    return true;
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
    repaint(getLocalBounds().expanded(
        0, static_cast<int>(t::layout::keyShadowDrop + material_.shadowBlur)));
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
    repaint(getLocalBounds().expanded(
        0, static_cast<int>(t::layout::keyShadowDrop + material_.shadowBlur)));
}

void KeyButton::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    const auto pressed = easeOut(press_);
    const auto face =
        getLocalBounds().toFloat().translated(0.0F, t::layout::keyPressDrop * pressed);
    const auto radius = t::layout::keyRadius;

    // The base and shadow sit below the moved face, so the base stays where it was.
    const auto baseDrop = lerp(t::layout::keyBaseDrop, t::layout::keyBasePressedDrop, pressed);
    g.setColour(material_.base);
    g.fillRoundedRectangle(face.translated(0.0F, baseDrop), radius);

    juce::Path shape;
    shape.addRoundedRectangle(face, radius);
    const auto shadowDrop =
        lerp(t::layout::keyShadowDrop, t::layout::keyShadowPressedDrop, pressed);
    const auto shadowBlur = lerp(material_.shadowBlur, t::layout::keyShadowPressedBlur, pressed);
    const auto shadowAlpha = lerp(material_.shadowAlpha, t::layout::keyShadowPressedAlpha, pressed);
    juce::DropShadow(juce::Colours::black.withAlpha(shadowAlpha), static_cast<int>(shadowBlur),
                     {0, static_cast<int>(shadowDrop)})
        .drawForPath(g, shape);

    g.setGradientFill(juce::ColourGradient::vertical(material_.top, face.getY(), material_.bottom,
                                                     face.getBottom()));
    g.fillRoundedRectangle(face, radius);
    g.setColour(material_.edge);
    g.drawRoundedRectangle(face.reduced(0.5F), radius - 0.5F, 1.0F);

    paintContent(g, face);

    if (hasKeyboardFocus(true))
    {
        const auto stroke = t::layout::focusRingStroke;
        g.setColour(t::focus);
        g.drawRoundedRectangle(face.expanded(t::layout::focusRingOffset + (stroke / 2.0F)),
                               radius + t::layout::focusRingOffset, stroke);
    }

    if (led_)
    {
        const auto size = t::layout::ledSize;
        const auto spot = juce::Rectangle<float>(face.getRight() - t::layout::ledInset - size,
                                                 face.getY() + t::layout::ledInset, size, size);
        if (ledLevel_ > 0.0F)
        {
            juce::Path dot;
            dot.addEllipse(spot);
            juce::DropShadow(led_->withAlpha(ledLevel_), static_cast<int>(t::layout::ledGlow),
                             {0, 0})
                .drawForPath(g, dot);
        }
        g.setColour(t::ledOff.interpolatedWith(*led_, ledLevel_));
        g.fillEllipse(spot);
    }
}

void KeyButton::paintContent(juce::Graphics& g, juce::Rectangle<float> face) const
{
    const auto labelFont = font(Face::monoRegular, t::keyLabelSize);
    const auto labelHeight = labelFont.getHeight();
    const auto top =
        face.getY() +
        ((face.getHeight() - (iconSize_ + t::layout::keyLabelGap + labelHeight)) / 2.0F);

    g.setColour(t::ink);
    g.fillPath(icon_, juce::AffineTransform::scale(iconSize_ / t::layout::keyIconGrid)
                          .translated(face.getCentreX() - (iconSize_ / 2.0F), top));
    g.setFont(labelFont);
    g.drawText(label_,
               juce::Rectangle<float>(face.getX(), top + iconSize_ + t::layout::keyLabelGap,
                                      face.getWidth(), labelHeight),
               juce::Justification::centred, false);
}

} // namespace md::ui
