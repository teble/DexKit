#pragma once

#include <thread>
#include <vector>

namespace dexkit {

void RegisterMatcherThreadLocalCache(
        std::thread::id thread_id,
        void *cache,
        void (*deleter)(void *)
);

void ReleaseMatcherThreadLocalCaches(const std::vector<std::thread::id> &thread_ids);

} // namespace dexkit
