#include "AllocationGuard.h"

#include <cstdlib>
#include <new>

// This file is the test binary's allocator, so it manages memory by hand with thread-local state.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-no-malloc,hicpp-no-malloc,readability-named-parameter,hicpp-named-parameter,readability-inconsistent-declaration-parameter-name,misc-new-delete-overloads)
namespace
{

thread_local bool armed = false;
thread_local int count = 0;

void* allocate(std::size_t size)
{
    if (armed)
    {
        ++count;
    }
    return std::malloc(size == 0 ? 1 : size);
}

void* allocateAligned(std::size_t size, std::align_val_t alignment)
{
    if (armed)
    {
        ++count;
    }
    void* p = nullptr;
    const auto a = static_cast<std::size_t>(alignment);
    return posix_memalign(&p, a < sizeof(void*) ? sizeof(void*) : a, size == 0 ? 1 : size) == 0
               ? p
               : nullptr;
}

void* orThrow(void* p)
{
    if (p == nullptr)
    {
        throw std::bad_alloc();
    }
    return p;
}

} // namespace

namespace md_test
{

void armAllocationCount()
{
    count = 0;
    armed = true;
}

int disarmAllocationCount()
{
    armed = false;
    return count;
}

[[gnu::noinline]] void allocateOnce()
{
    void* volatile p = ::operator new(16);
    ::operator delete(p);
}

} // namespace md_test

void* operator new(std::size_t size) { return orThrow(allocate(size)); }
void* operator new[](std::size_t size) { return orThrow(allocate(size)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new(std::size_t size, std::align_val_t a)
{
    return orThrow(allocateAligned(size, a));
}
void* operator new[](std::size_t size, std::align_val_t a)
{
    return orThrow(allocateAligned(size, a));
}
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return allocateAligned(size, a);
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept
{
    return allocateAligned(size, a);
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { std::free(p); }
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-no-malloc,hicpp-no-malloc,readability-named-parameter,hicpp-named-parameter,readability-inconsistent-declaration-parameter-name,misc-new-delete-overloads)
