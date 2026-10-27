#include "AllocationGuard.h"

#include <cstdlib>
#include <malloc/malloc.h>
#include <new>

#include <juce_core/juce_core.h>

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
    return malloc_zone_malloc(malloc_default_zone(), size == 0 ? 1 : size);
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

[[gnu::noinline]] void mallocOnce()
{
    juce::HeapBlock<char> block;
    block.malloc(64);
}

} // namespace md_test

#if !__has_feature(address_sanitizer) && !__has_feature(thread_sanitizer)
// JUCE containers allocate through malloc, realloc and calloc, not operator new. The test binary
// defines them itself so code linked into it is counted; the real work goes to the default zone.
// Frameworks bind to the system's malloc and are not counted. The sanitizer builds keep their own.
extern "C"
{
    void* malloc(std::size_t size)
    {
        if (armed)
        {
            ++count;
        }
        return malloc_zone_malloc(malloc_default_zone(), size);
    }
    void* calloc(std::size_t n, std::size_t size)
    {
        if (armed)
        {
            ++count;
        }
        return malloc_zone_calloc(malloc_default_zone(), n, size);
    }
    void* realloc(void* p, std::size_t size)
    {
        if (armed)
        {
            ++count;
        }
        return malloc_zone_realloc(malloc_default_zone(), p, size);
    }
}
#endif

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
