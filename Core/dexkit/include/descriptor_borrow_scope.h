#pragma once

#if DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS
#include <cstdlib>

namespace dexkit {
class DexKit;

// A context is borrowed from an admitted operation, not a new admission. The
// operation must drain its tasks and all returned views before its guard ends.
class DescriptorBorrowScope {
    friend class DexKit;
    const void *owner_;
    DescriptorBorrowScope *previous_;
    inline static thread_local DescriptorBorrowScope *current_ = nullptr;

    explicit DescriptorBorrowScope(const void *owner)
            : owner_(owner), previous_(current_) { current_ = this; }

public:
    class Context {
        friend class DescriptorBorrowScope;
        const void *owner_;
        explicit Context(const void *owner) : owner_(owner) {}
    };
    explicit DescriptorBorrowScope(Context context) : DescriptorBorrowScope(context.owner_) {}
    DescriptorBorrowScope(const DescriptorBorrowScope &) = delete;
    DescriptorBorrowScope &operator=(const DescriptorBorrowScope &) = delete;
    ~DescriptorBorrowScope() {
        if (current_ != this) std::abort();
        current_ = previous_;
    }
    static Context Capture() { return Context(current_ ? current_->owner_ : nullptr); }
    static bool Contains(const void *owner) {
        for (auto *scope = current_; scope; scope = scope->previous_)
            if (scope->owner_ == owner) return true;
        return false;
    }
    static void Require(const void *owner) {
        if (!owner || !Contains(owner)) std::abort();
    }
};
} // namespace dexkit
#endif
