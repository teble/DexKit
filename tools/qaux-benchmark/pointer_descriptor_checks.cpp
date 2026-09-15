#include "pointer_descriptor_cache.h"
#include <cstdio>
#include <new>

namespace {
bool fail_next_allocation = false;
void *Allocate(size_t bytes) {
    if (fail_next_allocation) { fail_next_allocation = false; throw std::bad_alloc(); }
    if (auto *p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void Require(bool condition) { if (!condition) std::abort(); }
}
void *operator new(size_t bytes) { return Allocate(bytes); }
void *operator new[](size_t bytes) { return Allocate(bytes); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }

int main() {
    using dexkit::PointerDescriptorCache;
    { PointerDescriptorCache unused; }
    { PointerDescriptorCache empty; empty.Initialize(0); Require(empty.size() == 0); }
    {
        PointerDescriptorCache failed;
        bool caught = false;
        fail_next_allocation = true;
        try { failed.Initialize(2); } catch (const std::bad_alloc &) { caught = true; }
        Require(caught && failed.size() == 0);
        failed.Initialize(2);
        Require(!failed.TryGet(0) && !failed.TryGet(1));
        auto &empty = failed.Publish(0, std::make_unique<std::string>());
        Require(failed.TryGet(0) == &empty && empty.empty());
        caught = false;
        fail_next_allocation = true;
        try { failed.Publish(1, std::make_unique<std::string>(1024, 'x')); }
        catch (const std::bad_alloc &) { caught = true; }
        Require(caught && !failed.TryGet(1));
        failed.Publish(1, std::make_unique<std::string>(1024, 'x'));
        Require(failed.TryGet(0) == &empty && failed.TryGet(1)->size() == 1024);
    }
    {
        PointerDescriptorCache failed;
        bool caught = false;
        fail_next_allocation = true;
        try { failed.Initialize(3); } catch (const std::bad_alloc &) { caught = true; }
        Require(caught); // Destruction without a successful retry is safe too.
    }
    std::puts("CHECK_POINTER_DESCRIPTOR_COMPONENT {\"passed\":true}");
}
