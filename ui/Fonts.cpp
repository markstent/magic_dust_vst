#include "ui/Fonts.h"

#include <array>
#include <cstddef>

#include "MdFontData.h"

namespace md::ui
{

namespace
{

juce::Typeface::Ptr load(const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor(data, static_cast<std::size_t>(size));
}

} // namespace

juce::Font font(Face face, float size)
{
    static const std::array<juce::Typeface::Ptr, 5> faces{
        load(MdFontData::SpaceGrotesk500_ttf, MdFontData::SpaceGrotesk500_ttfSize),
        load(MdFontData::SpaceGrotesk700_ttf, MdFontData::SpaceGrotesk700_ttfSize),
        load(MdFontData::SpaceMonoRegular_ttf, MdFontData::SpaceMonoRegular_ttfSize),
        load(MdFontData::SpaceMonoBold_ttf, MdFontData::SpaceMonoBold_ttfSize),
        load(MdFontData::LibreBaskervilleBold_ttf, MdFontData::LibreBaskervilleBold_ttfSize)};
    const auto& typeface = faces.at(static_cast<std::size_t>(face));
    if (typeface == nullptr)
    {
        return {juce::FontOptions(size).withPointHeight(size)};
    }
    return {juce::FontOptions(typeface).withPointHeight(size)};
}

juce::String ellipsize(const juce::Font& font, const juce::String& text, float maxWidth)
{
    if (juce::GlyphArrangement::getStringWidth(font, text) <= maxWidth)
    {
        return text;
    }
    auto cut = text;
    while (cut.isNotEmpty() && juce::GlyphArrangement::getStringWidth(font, cut + "...") > maxWidth)
    {
        cut = cut.dropLastCharacters(1);
    }
    return cut + "...";
}

} // namespace md::ui
