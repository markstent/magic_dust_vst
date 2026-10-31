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
    // Also drops the owner's main registration.
    void remove(const void* owner);

    // The earliest still-registered main instance; the next oldest takes over when it goes.
    void registerMain(const void* owner);
    [[nodiscard]] const void* oldestMain() const { return oldest_.load(); }
    void addSender(md::LaneId lane);
    void removeSender(md::LaneId lane);
    [[nodiscard]] int sendersFor(md::LaneId lane) const;
    // Lane mute bits (bit = lane) of a published main instance, so its senders can follow. Both
    // are lock-free; a main that has published nothing reads as unmuted.
    void setMutes(const void* owner, unsigned mask);
    [[nodiscard]] unsigned mutes(const void* owner) const;
    // The octave (-2 to 2) of each lane of a published main instance, published the same way; a
    // main that has published nothing reads as 0.
    void setOctave(const void* owner, md::LaneId lane, int octave);
    [[nodiscard]] int octave(const void* owner, md::LaneId lane) const;

  private:
    struct Slot
    {
        std::atomic<const void*> owner{nullptr};
        std::atomic<const md::PlaybackPattern*> pattern{nullptr};
        std::atomic<unsigned> mutes{0};
        std::array<std::atomic<int>, 3> octaves{};
        std::shared_ptr<const md::PlaybackPattern> keep;
    };
    struct Retired
    {
        std::shared_ptr<const md::PlaybackPattern> pattern;
        std::chrono::steady_clock::time_point at;
    };

    Slot* find(const void* owner);
    void retireLocked(std::shared_ptr<const md::PlaybackPattern> pattern);
    void dropMainLocked(const void* owner);

    std::mutex mutex_;
    std::array<Slot, kCapacity> slots_;
    std::vector<Retired> retired_;
    std::vector<const void*> mains_; // registration order
    std::atomic<const void*> oldest_{nullptr};
    std::array<std::atomic<int>, 3> senders_{};
};
