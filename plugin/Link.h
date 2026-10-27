#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include "plugin/Playback.h"

// Process-wide pattern publishing. publish, retire and remove run on a message thread; current is
// the only call the audio thread makes and it never locks or allocates. A replaced pattern lives
// at least a second more, so a block that is still reading it is never left dangling.
class Link
{
  public:
    static constexpr std::size_t kCapacity = 16;

    static Link& instance();

    void publish(const void* owner, std::shared_ptr<const md::PlaybackPattern> pattern);
    [[nodiscard]] const md::PlaybackPattern* current(const void* owner) const;
    // now is a parameter so tests need not wait out the second.
    void retire(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void remove(const void* owner);

  private:
    struct Slot
    {
        std::atomic<const void*> owner{nullptr};
        std::atomic<const md::PlaybackPattern*> pattern{nullptr};
        std::shared_ptr<const md::PlaybackPattern> keep;
    };
    struct Retired
    {
        std::shared_ptr<const md::PlaybackPattern> pattern;
        std::chrono::steady_clock::time_point at;
    };

    Slot* find(const void* owner);
    void retireLocked(std::shared_ptr<const md::PlaybackPattern> pattern);

    std::mutex mutex_;
    std::array<Slot, kCapacity> slots_;
    std::vector<Retired> retired_;
};
