#include "ui/AboutPanel.h"

#include <array>

#include "ui/Faceplate.h"
#include "ui/Focus.h"
#include "ui/Fonts.h"
#include "ui/Tokens.h"

namespace md::ui
{

namespace t = md::tokens;
namespace c = md::tokens::console;

namespace
{

constexpr const char* kName = "MAGIC DUST";
constexpr const char* kOwner = "Mark Stent Digital";
constexpr const char* kLicence = "open source under AGPLv3";
constexpr const char* kLinkText = "github.com/markstent/magic_dust_vst";
constexpr const char* kLinkUrl = "https://github.com/markstent/magic_dust_vst";
constexpr float kLineHeightFactor = 1.3F;
constexpr int kLinkPad = 4;

juce::String version() { return juce::String("version ") + JucePlugin_VersionString; }

juce::Font nameFont()
{
    return font(Face::serifBold, t::layout::aboutTitleSize)
        .withExtraKerningFactor(t::layout::wordmarkFaceSpacing);
}

juce::Font versionFont() { return font(Face::monoRegular, t::layout::aboutVersionSize); }
juce::Font ownerFont() { return font(Face::groteskMedium, t::layout::aboutOwnerSize); }
juce::Font smallFont() { return font(Face::monoRegular, t::layout::aboutSmallSize); }

int lineHeight(const juce::Font& f) { return juce::roundToInt(f.getHeight() * kLineHeightFactor); }

// The five lines' heights, top to bottom, and the block they make with the gaps between.
std::array<int, 5> lineHeights()
{
    return {lineHeight(nameFont()), lineHeight(versionFont()), lineHeight(ownerFont()),
            lineHeight(smallFont()), lineHeight(smallFont())};
}

int blockHeight()
{
    int total = t::layout::aboutLineGap * 4;
    for (const auto h : lineHeights())
    {
        total += h;
    }
    return total;
}

} // namespace

AboutPanel::AboutPanel() : link_(kLinkText, juce::URL(kLinkUrl))
{
    setComponentID("aboutPanel");
    setTitle(juce::String("about magic dust, ") + version().toLowerCase() + ", " +
             juce::String(kOwner).toLowerCase());
    setWantsKeyboardFocus(true);
    setVisible(false);

    link_.setComponentID("aboutLink");
    link_.setTitle(kLinkText);
    auto linkFont = smallFont();
    linkFont.setUnderline(true);
    link_.setFont(linkFont, false, juce::Justification::centred);
    link_.setColour(juce::HyperlinkButton::textColourId, c::ink);
    addAndMakeVisible(link_);
}

void AboutPanel::open()
{
    setVisible(true);
    toFront(false);
    grabKeyboardFocus();
}

juce::StringArray AboutPanel::lines() { return {kName, version(), kOwner, kLicence, kLinkText}; }

void AboutPanel::dismiss()
{
    if (!isVisible())
    {
        return;
    }
    setVisible(false);
    if (onClose_)
    {
        onClose_();
    }
}

void AboutPanel::paint(juce::Graphics& g)
{
    // The same cream as the display, whose bezel this covers the inside of.
    paintCreamWindow(g, getLocalBounds().toFloat().expanded(t::layout::displayBorder),
                     t::layout::displayBorder, t::layout::displayRadius, c::displayGlowColour,
                     t::layout::displayGlow);

    const auto heights = lineHeights();
    const std::array<juce::Font, 4> fonts{nameFont(), versionFont(), ownerFont(), smallFont()};
    const auto text = lines();
    int y = (getHeight() - blockHeight()) / 2;
    g.setColour(c::ink);
    for (std::size_t i = 0; i < fonts.size(); ++i)
    {
        g.setFont(fonts.at(i));
        g.drawText(text.getReference(static_cast<int>(i)), 0, y, getWidth(), heights.at(i),
                   juce::Justification::centred, false);
        y += heights.at(i) + t::layout::aboutLineGap;
    }

    if (hasKeyboardFocus(false))
    {
        drawFocusRing(g, getLocalBounds().toFloat(), t::layout::displayRadius, c::ink);
    }
}

void AboutPanel::resized()
{
    const auto heights = lineHeights();
    int y = (getHeight() - blockHeight()) / 2;
    for (std::size_t i = 0; i + 1 < heights.size(); ++i)
    {
        y += heights.at(i) + t::layout::aboutLineGap;
    }
    const auto width =
        juce::GlyphArrangement::getStringWidthInt(smallFont(), kLinkText) + (2 * kLinkPad);
    link_.setBounds((getWidth() - width) / 2, y, width, heights.back());
}

bool AboutPanel::keyPressed(const juce::KeyPress& key)
{
    if (key != juce::KeyPress::escapeKey)
    {
        return false;
    }
    dismiss();
    return true;
}

void AboutPanel::mouseDown(const juce::MouseEvent& /*event*/) { dismiss(); }

void AboutPanel::focusOfChildComponentChanged(FocusChangeType /*cause*/)
{
    // Focus went to something else in the editor: close without pulling it back.
    if (isVisible() && !hasKeyboardFocus(true))
    {
        setVisible(false);
    }
}

} // namespace md::ui
