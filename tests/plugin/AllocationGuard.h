#pragma once

#include <catch2/catch_test_macros.hpp>

// Counts every global operator new on the calling thread while armed. Counting rather than
// aborting lets a test prove the guard itself works.
namespace md_test
{

void armAllocationCount();
// Disarms and returns how many allocations happened since armAllocationCount.
int disarmAllocationCount();
// Makes one allocation the optimiser cannot remove.
void allocateOnce();

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
