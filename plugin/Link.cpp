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
    if (owner == nullptr)
    {
        return nullptr;
    }
    for (const Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            return slot.pattern.load();
        }
    }
    return nullptr;
}

void Link::setMutes(const void* owner, unsigned mask)
{
    for (Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            slot.mutes.store(mask);
            return;
        }
    }
}

unsigned Link::mutes(const void* owner) const
{
    for (const Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            return slot.mutes.load();
        }
    }
    return 0;
}

void Link::setOctave(const void* owner, md::LaneId lane, int octave)
{
    for (Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            slot.octaves.at(static_cast<std::size_t>(lane)).store(octave);
            return;
        }
    }
}

int Link::octave(const void* owner, md::LaneId lane) const
{
    for (const Slot& slot : slots_)
    {
        if (slot.owner.load() == owner)
        {
            return slot.octaves.at(static_cast<std::size_t>(lane)).load();
        }
    }
    return 0;
}

void Link::retire(std::chrono::steady_clock::time_point now)
{
    const std::scoped_lock lock(mutex_);
    const auto cutoff = now - kRetireAfter;
    std::erase_if(retired_, [cutoff](const Retired& r) { return r.at < cutoff; });
}

void Link::dropMainLocked(const void* owner)
{
    std::erase(mains_, owner);
    oldest_.store(mains_.empty() ? nullptr : mains_.front());
}

void Link::registerMain(const void* owner)
{
    const std::scoped_lock lock(mutex_);
    if (std::ranges::find(mains_, owner) == mains_.end())
    {
        mains_.push_back(owner);
        oldest_.store(mains_.front());
    }
}

void Link::addSender(md::LaneId lane) { ++senders_.at(static_cast<std::size_t>(lane)); }

void Link::removeSender(md::LaneId lane) { --senders_.at(static_cast<std::size_t>(lane)); }

int Link::sendersFor(md::LaneId lane) const
{
    return senders_.at(static_cast<std::size_t>(lane)).load();
}

void Link::remove(const void* owner)
{
    const std::scoped_lock lock(mutex_);
    dropMainLocked(owner);
    if (Slot* slot = find(owner))
    {
        slot->owner.store(nullptr);
        slot->pattern.store(nullptr);
        slot->mutes.store(0);
        for (auto& octave : slot->octaves)
        {
            octave.store(0);
        }
        retireLocked(std::move(slot->keep));
        slot->keep.reset();
    }
}
