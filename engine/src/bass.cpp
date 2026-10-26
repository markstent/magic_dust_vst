#include "internal.hpp"

#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace md::detail
{
namespace
{
enum class Role : std::uint8_t
{
    root,
    octave,
    fifth,
};

struct Slot
{
    int step;
    int length;
    Role role;
};

constexpr Slot hit(int step, int length, Role role)
{
    return {.step = step, .length = length, .role = role};
}

constexpr Role R = Role::root;
constexpr Role O = Role::octave;
constexpr Role F = Role::fifth;

constexpr std::array kRolling = std::to_array<Slot>({
    hit(2, 1, R),
    hit(3, 1, R),
    hit(6, 1, R),
    hit(7, 1, R),
    hit(10, 1, R),
    hit(11, 1, R),
    hit(14, 1, R),
    hit(15, 1, R),
});
constexpr std::array kOffbeat =
    std::to_array<Slot>({hit(2, 2, R), hit(6, 2, R), hit(10, 2, R), hit(14, 2, R)});
constexpr std::array kBounce = std::to_array<Slot>({
    hit(2, 1, R),
    hit(3, 1, O),
    hit(6, 1, R),
    hit(7, 1, O),
    hit(10, 1, R),
    hit(11, 1, O),
    hit(14, 1, R),
    hit(15, 1, O),
});
constexpr std::array kGarage =
    std::to_array<Slot>({hit(3, 1, R), hit(6, 2, R), hit(11, 1, R), hit(14, 2, F)});
constexpr std::array kSustained = std::to_array<Slot>({hit(0, 16, R)});

std::span<const Slot> slotsFor(BassPattern pattern)
{
    switch (pattern)
    {
    case BassPattern::rollingOffbeat:
        return kRolling;
    case BassPattern::octaveBounce:
        return kBounce;
    case BassPattern::garageSkip:
        return kGarage;
    case BassPattern::sustained:
        return kSustained;
    default:
        return kOffbeat; // offbeat and pedal
    }
}

// The chord root in 33 to 44.
int rootNote(int rootPc) { return 33 + ((rootPc + 12 - (33 % 12)) % 12); }

int pitchFor(Role role, int root)
{
    switch (role)
    {
    case Role::octave:
        // Up an octave when that stays in range, otherwise down, otherwise (roots with pitch class
        // 1 to 3 have no octave pair in range) the fifth.
        if (root + 12 <= kBassHigh)
        {
            return root + 12;
        }
        return root - 12 >= kBassLow ? root - 12 : pitchFor(Role::fifth, root);
    case Role::fifth:
        return root + 7 <= kBassHigh ? root + 7 : root - 5;
    case Role::root:
        break;
    }
    return root;
}
} // namespace

BassPattern bassPatternFor(const StyleEntry& entry)
{
    const auto it = std::ranges::find_if(
        entry.bassPatterns, [](BassPattern pattern)
        { return pattern != BassPattern::riff && pattern != BassPattern::callResponse; });
    // The riff and call-and-response patterns arrive in a later phase.
    return it == entry.bassPatterns.end() ? BassPattern::offbeat : *it;
}

std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern)
{
    const int swing = swingPermille(idea.settings.groove);
    std::vector<NoteEvent> events;
    for (int bar = 0; bar < idea.settings.bars; ++bar)
    {
        const int rootPc =
            pattern == BassPattern::pedal
                ? idea.key.root
                : chordRootPc(idea.key, chordAt(idea.progression, bar * kTicksPerBar).symbol);
        const int root = rootNote(rootPc);
        for (const Slot& slot : slotsFor(pattern))
        {
            const NoteEvent note{
                .pitch = pitchFor(slot.role, root),
                .start = swung((bar * kTicksPerBar) + (slot.step * kTicksPerSixteenth), swing),
                .length = slot.length * kTicksPerSixteenth,
                .velocity = 0,
            };
            events.push_back(note);
        }
    }
    clampMonophonic(events, idea.settings.bars * kTicksPerBar);
    return events;
}
} // namespace md::detail
