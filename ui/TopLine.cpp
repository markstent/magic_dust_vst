#include "ui/TopLine.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/types.hpp"
#include "plugin/PluginProcessor.h"
#include "ui/Faceplate.h"
#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;

namespace
{

constexpr int kStyleCount = static_cast<int>(md::Style::nyGroove) + 1;

const char* const kUnknown = "----";

// Style names are spelt with hyphens in the engine and with spaces on screen.
juce::String styleText(md::Style style)
{
    return juce::String(std::string(md::styleName(style))).replaceCharacter('-', ' ');
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

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ReadoutWindow::ReadoutWindow(const juce::String& id, juce::String legend, bool interactive,
                             float textSize)
    : juce::Button(id), legend_(std::move(legend)), textSize_(textSize)
{
    setComponentID(id);
    if (interactive)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    else
    {
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
    }
}

void ReadoutWindow::setText(const juce::String& text)
{
    if (text != text_)
    {
        text_ = text;
        setTitle(legend_ + ", " + text_);
        repaint();
    }
}

void ReadoutWindow::paintButton(juce::Graphics& g, bool /*highlighted*/, bool /*down*/)
{
    const auto bounds = getLocalBounds().toFloat();
    paintCreamWindow(g, bounds, t::layout::readoutBorder, t::layout::readoutRadius,
                     t::console::readoutGlowColour, t::layout::readoutGlow);
    g.setFont(font(Face::monoBold, textSize_));
    g.setColour(t::console::ink);
    g.drawText(text_.toUpperCase(),
               bounds.reduced(t::layout::readoutBorder + t::layout::readoutTextPad, 0.0F),
               juce::Justification::centred, true);
    if (hasKeyboardFocus(true))
    {
        drawFocusRing(g, bounds, t::layout::readoutRadius, t::console::ink);
    }
}

TopLine::TopLine(MagicDustProcessor& processor)
    : processor_(processor), style_("styleMenu", "style", true), key_("keyMenu", "key", true),
      tempo_("tempoReadout", "tempo", false)
{
    setComponentID("topLine");
    style_.onClick = [this] { openStyleMenu(); };
    key_.onClick = [this] { openKeyMenu(); };
    const std::array<ReadoutWindow*, 3> windows{&style_, &key_, &tempo_};
    for (auto* window : windows)
    {
        addAndMakeVisible(*window);
    }
}

void TopLine::refresh()
{
    const auto* idea = processor_.idea();
    style_.setText(idea != nullptr ? styleText(idea->settings.style) : juce::String(kUnknown));
    key_.setText(idea != nullptr ? keyText(idea->key) : juce::String(kUnknown));
    tempo_.setText(juce::String(processor_.hostBpm().value_or(md::kDefaultTempo), 1));
}

void TopLine::resized()
{
    // The windows in header order: style, key, tempo.
    const std::array<ReadoutWindow*, 3> windows{&style_, &key_, &tempo_};
    for (std::size_t i = 0; i < windows.size(); ++i)
    {
        windows.at(i)->setBounds(t::layout::readoutX.at(i), t::layout::readoutY,
                                 t::layout::readoutWidth.at(i), t::layout::readoutHeight);
    }
}

void TopLine::paint(juce::Graphics& g)
{
    g.setFont(font(Face::groteskMedium, t::layout::legendSize)
                  .withExtraKerningFactor(t::layout::legendSpacing));
    g.setColour(t::console::legend);
    for (std::size_t i = 0; i < kLegends.size(); ++i)
    {
        g.drawText(kLegends.at(i),
                   juce::Rectangle<int>(t::layout::readoutX.at(i), 0, t::layout::readoutWidth.at(i),
                                        t::layout::legendHeight),
                   juce::Justification::centred, false);
    }
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
