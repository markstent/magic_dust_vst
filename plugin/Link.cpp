#include "Link.h"

#include <algorithm>
#include <utility>

namespace
{

constexpr auto kRetireAfter = std::chrono::seconds(1);

} // namespace

Link& Link::instance()
{
    static Link link;
    return link;
}

Link::Slot* Link::find(const void* owner)
{
    for (Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            return &slot;
        }
    }
    return nullptr;
}

void Link::retireLocked(std::shared_ptr<const md::PlaybackPattern> pattern)
{
    if (pattern)
    {
        retired_.push_back({.pattern = std::move(pattern), .at = std::chrono::steady_clock::now()});
    }
}

void Link::publish(const void* owner, std::shared_ptr<const md::PlaybackPattern> pattern)
{
    const std::scoped_lock lock(mutex_);
    Slot* slot = find(owner);
    if (slot == nullptr)
    {
        slot = find(nullptr);
    }
    if (slot == nullptr)
    {
        return; // all slots taken: the extra instance stays silent
    }
    retireLocked(std::exchange(slot->keep, std::move(pattern)));
    slot->pattern.store(slot->keep.get());
    slot->owner.store(owner);
}

const md::PlaybackPattern* Link::current(const void* owner) const
{
    for (const Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            return slot.pattern.load();
        }
    }
    return nullptr;
}

void Link::retire()
{
    const std::scoped_lock lock(mutex_);
    const auto cutoff = std::chrono::steady_clock::now() - kRetireAfter;
    std::erase_if(retired_, [cutoff](const Retired& r) { return r.at < cutoff; });
}

void Link::remove(const void* owner)
{
    const std::scoped_lock lock(mutex_);
    if (Slot* slot = find(owner))
    {
        slot->owner.store(nullptr);
        slot->pattern.store(nullptr);
        retireLocked(std::move(slot->keep));
        slot->keep.reset();
    }
}
