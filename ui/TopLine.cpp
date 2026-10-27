#include "ui/TopLine.h"

#include <cmath>
#include <cstddef>
#include <string>

#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/types.hpp"
#include "plugin/PluginProcessor.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kStyleCount = static_cast<int>(md::Style::nyGroove) + 1;

const char* const kUnknown = "---";

// Style names are spelt with hyphens in the engine and with spaces on screen.
juce::String styleText(md::Style style)
{
    return juce::String(std::string(md::styleName(style))).replaceCharacter('-', ' ');
}

juce::Font topFont() { return font(Face::monoRegular, t::topLineSize); }

int widthOf(const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidthInt(topFont(), text);
}

} // namespace

juce::String keyText(const md::Key& key)
{
    const auto root = juce::String(std::string(md::rootName(key.root))).toLowerCase();
    switch (key.mode)
    {
    case md::Mode::aeolian:
        return root + " min";
    case md::Mode::ionian:
        return root + " maj";
    case md::Mode::phrygian:
    case md::Mode::dorian:
    case md::Mode::mixolydian:
        break;
    }
    return root + " " + juce::String(std::string(md::modeName(key.mode)));
}

MenuText::MenuText(const juce::String& id) : juce::Button(id)
{
    setComponentID(id);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void MenuText::setText(const juce::String& text)
{
    if (text != text_)
    {
        text_ = text;
        setTitle(getComponentID() + ", " + text_);
        repaint();
    }
}

void MenuText::paintButton(juce::Graphics& g, bool highlighted, bool /*down*/)
{
    g.setFont(topFont());
    g.setColour(highlighted ? t::screenInk : t::screenQuiet);
    g.drawText(text_, getLocalBounds(), juce::Justification::centredRight, false);
}

TopLine::TopLine(MagicDustProcessor& processor)
    : processor_(processor), key_("keyMenu"), style_("styleMenu")
{
    setComponentID("topLine");
    key_.onClick = [this] { openKeyMenu(); };
    style_.onClick = [this] { openStyleMenu(); };
    addAndMakeVisible(key_);
    addAndMakeVisible(style_);
}

void TopLine::refresh()
{
    const auto* idea = processor_.idea();
    const auto bpm = processor_.hostBpm();
    spell_ = "spell " + (idea != nullptr ? juce::String(idea->seed) : juce::String(kUnknown));
    tempo_ =
        (bpm ? juce::String(static_cast<int>(std::lround(*bpm))) : juce::String(kUnknown)) + " bpm";
    key_.setText(idea != nullptr ? keyText(idea->key) : juce::String(kUnknown));
    style_.setText(idea != nullptr ? styleText(idea->settings.style) : juce::String(kUnknown));
    layoutRight();
    repaint();
}

void TopLine::layoutRight()
{
    const auto gap = widthOf(" ");
    const auto height = getHeight();
    auto right = getWidth();

    const auto styleWidth = widthOf(style_.getText());
    style_.setBounds(right - styleWidth, 0, styleWidth, height);
    right -= styleWidth + gap;

    const auto tempoWidth = widthOf(tempo_);
    tempoArea_ = juce::Rectangle<int>(right - tempoWidth, 0, tempoWidth, height);
    right -= tempoWidth + gap;

    const auto keyWidth = widthOf(key_.getText());
    key_.setBounds(right - keyWidth, 0, keyWidth, height);
}

void TopLine::paint(juce::Graphics& g)
{
    g.setFont(topFont());
    g.setColour(t::gold);
    g.drawText(spell_, getLocalBounds(), juce::Justification::centredLeft, false);

    const auto unit = t::layout::chevronSize / t::layout::chevronGrid;
    const auto origin =
        juce::Point<float>(static_cast<float>(widthOf(spell_)) + t::layout::chevronGap,
                           (static_cast<float>(getHeight()) - t::layout::chevronSize) / 2.0F);
    juce::Path chevron;
    for (std::size_t i = 0; i < t::layout::chevronPoints.size(); ++i)
    {
        const auto& point = t::layout::chevronPoints.at(i);
        const auto at = origin + (juce::Point<float>(point.at(0), point.at(1)) * unit);
        if (i == 0)
        {
            chevron.startNewSubPath(at);
        }
        else
        {
            chevron.lineTo(at);
        }
    }
    g.strokePath(chevron,
                 juce::PathStrokeType(t::layout::chevronStroke, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    g.setFont(topFont());
    g.setColour(t::screenQuiet);
    g.drawText(tempo_, tempoArea_, juce::Justification::centredRight, false);
}

void TopLine::openKeyMenu()
{
    const auto* idea = processor_.idea();
    juce::PopupMenu menu;
    for (int root = 0; root < md::kPitchClasses; ++root)
    {
        menu.addItem(root + 1, juce::String(std::string(md::rootName(root))).toLowerCase(), true,
                     idea != nullptr && idea->key.root == root);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&key_),
                       [this](int chosen)
                       {
                           if (chosen > 0)
                           {
                               processor_.setKeyRoot(chosen - 1);
                           }
                       });
}

void TopLine::openStyleMenu()
{
    const auto* idea = processor_.idea();
    juce::PopupMenu menu;
    for (int style = 0; style < kStyleCount; ++style)
    {
        menu.addItem(style + 1, styleText(static_cast<md::Style>(style)), true,
                     idea != nullptr && idea->settings.style == static_cast<md::Style>(style));
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&style_),
                       [this](int chosen)
                       {
                           if (chosen > 0)
                           {
                               processor_.setStyle(static_cast<md::Style>(chosen - 1));
                           }
                       });
}

} // namespace md::ui
