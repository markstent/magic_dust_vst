#include "md/ideacode.hpp"

#include "md/ranges.hpp"

#include <cctype>
#include <cstddef>

namespace md
{
namespace
{
constexpr std::string_view kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::size_t kCodeLength = 13;
constexpr unsigned kFieldBits = 62; // padded with 3 zero bits to 65 = 13 * 5

// Packs fields most significant first.
class BitWriter
{
  public:
    template <unsigned Width> void put(std::uint32_t value)
    {
        bits_ = (bits_ << Width) | (value & ((1U << Width) - 1U));
    }
    [[nodiscard]] std::uint64_t bits() const { return bits_; }

  private:
    std::uint64_t bits_ = 0;
};

class BitReader
{
  public:
    explicit BitReader(std::uint64_t bits) : bits_(bits) {}
    template <unsigned Width> std::uint32_t get()
    {
        remaining_ -= Width;
        return static_cast<std::uint32_t>((bits_ >> remaining_) & ((1U << Width) - 1U));
    }

  private:
    std::uint64_t bits_;
    unsigned remaining_ = kFieldBits;
};

std::optional<int> barsFromCode(std::uint32_t code)
{
    switch (code)
    {
    case 0:
        return 4;
    case 1:
        return 8;
    case 2:
        return 16;
    default:
        return std::nullopt;
    }
}

std::uint32_t barsToCode(int bars)
{
    if (bars == 4)
    {
        return 0;
    }
    return bars == 16 ? 2U : 1U;
}
} // namespace

std::string encodeIdea(std::uint32_t seed, const Settings& settings, std::uint64_t bankVersion)
{
    BitWriter w;
    w.put<2>(0);
    w.put<17>(seed);
    w.put<4>(static_cast<std::uint32_t>(settings.keyRoot));
    w.put<3>(static_cast<std::uint32_t>(settings.style));
    w.put<7>(static_cast<std::uint32_t>(settings.mood));
    w.put<7>(static_cast<std::uint32_t>(settings.energy));
    w.put<2>(static_cast<std::uint32_t>(settings.groove));
    w.put<2>(static_cast<std::uint32_t>(settings.flavour));
    w.put<2>(barsToCode(settings.bars));
    w.put<16>(static_cast<std::uint32_t>(bankVersion));

    // Character i holds bits 64-5i down to 60-5i of the 65-bit value (the 62 field bits then 000),
    // which is the field value shifted right by 57-5i (left by 3 for the last character).
    std::string code;
    for (std::size_t i = 0; i < kCodeLength; ++i)
    {
        const int shift = static_cast<int>(kFieldBits) - 5 - (5 * static_cast<int>(i));
        const std::uint64_t group = shift >= 0 ? w.bits() >> static_cast<unsigned>(shift)
                                               : w.bits() << static_cast<unsigned>(-shift);
        code += kAlphabet.at(group & 31U);
    }
    return code;
}

std::optional<DecodedIdea> decodeIdea(std::string_view code)
{
    if (code.size() != kCodeLength)
    {
        return std::nullopt;
    }
    // 12 full characters give 60 bits; the last one holds 2 field bits and 3 padding bits.
    std::uint64_t acc = 0;
    std::uint64_t last = 0;
    for (const char raw : code)
    {
        const auto upper = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        const auto value = kAlphabet.find(upper);
        if (value == std::string_view::npos)
        {
            return std::nullopt;
        }
        acc = (acc << 5U) | last;
        last = value;
    }
    BitReader r((acc << 2U) | (last >> 3U));

    if (r.get<2>() != 0)
    {
        return std::nullopt;
    }
    DecodedIdea out{};
    out.seed = r.get<17>();
    const auto keyRoot = r.get<4>();
    const auto style = r.get<3>();
    const auto mood = r.get<7>();
    const auto energy = r.get<7>();
    const auto groove = r.get<2>();
    const auto flavour = r.get<2>();
    const auto bars = barsFromCode(r.get<2>());
    out.bankVersion16 = static_cast<std::uint16_t>(r.get<16>());

    if (out.seed > static_cast<std::uint32_t>(kSeedMax) || keyRoot > 11 ||
        style > static_cast<std::uint32_t>(Style::nyGroove) || mood > 100 || energy > 100 ||
        flavour > static_cast<std::uint32_t>(Flavour::jazzy) || !bars)
    {
        return std::nullopt;
    }

    out.settings.keyRoot = static_cast<int>(keyRoot);
    out.settings.style = static_cast<Style>(style);
    out.settings.mood = static_cast<int>(mood);
    out.settings.energy = static_cast<int>(energy);
    out.settings.groove = static_cast<Groove>(groove);
    out.settings.flavour = static_cast<Flavour>(flavour);
    out.settings.bars = *bars;
    return out;
}
} // namespace md
