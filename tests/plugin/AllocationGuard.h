#pragma once

#include <catch2/catch_test_macros.hpp>

// Counts every global operator new on the calling thread while armed. Counting rather than
// aborting lets a test prove the guard itself works.
namespace md_test
{

// The sanitizer builds keep their own malloc, so only operator new is counted there.
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
inline constexpr bool kCountsMalloc = false;
#else
inline constexpr bool kCountsMalloc = true;
#endif

void armAllocationCount();
// Disarms and returns how many allocations happened since armAllocationCount.
int disarmAllocationCount();
// Makes one allocation the optimiser cannot remove.
void allocateOnce();
// Makes one allocation through JUCE's malloc wrapper, as its containers do.
void mallocOnce();

// Fails the test if anything allocates in its scope.
struct ScopedNoAllocation
{
    ScopedNoAllocation() { armAllocationCount(); }
    ScopedNoAllocation(const ScopedNoAllocation&) = delete;
    ScopedNoAllocation& operator=(const ScopedNoAllocation&) = delete;
    ScopedNoAllocation(ScopedNoAllocation&&) = delete;
    ScopedNoAllocation& operator=(ScopedNoAllocation&&) = delete;
    ~ScopedNoAllocation() { CHECK(disarmAllocationCount() == 0); }
};

} // namespace md_test
