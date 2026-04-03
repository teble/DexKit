#pragma once

#include "parallel_hashmap/phmap.h"
#include <cstdint>
#include <thread>
#include <shared_mutex>

#define POINT_CASE(X) reinterpret_cast<std::uintptr_t>(X)

// 1: parallel_flat_hash_map
// 2: flat_hash_map
// 3: node_hash_map
#define THREAD_VARIABLE_MAP_TYPE 2

class ThreadVariable {
public:

    template<typename T, typename Arg>
    static std::shared_ptr<T> SetSharedVariable(std::uintptr_t key, Arg &&arg) {
        auto shared_ptr = std::make_shared<T>(std::forward<Arg>(arg));
        std::unique_lock lock(_shared_lock);
        _shared_variables[key] = shared_ptr;
        return shared_ptr;
    }

    template<typename T>
    static std::shared_ptr<T> GetSharedVariable(std::uintptr_t key) {
        if (_shared_variables.contains(key)) {
            auto &shared_ptr = _shared_variables[key];
            return std::shared_ptr<T>(shared_ptr, reinterpret_cast<T *>(shared_ptr.get()));
        }
        return nullptr;
    }

    template<typename T, typename Arg>
    static std::shared_ptr<T> SetThreadVariable(std::uintptr_t key, Arg &&arg) {
        auto thread_id = std::this_thread::get_id();
        auto shared_ptr = std::make_shared<T>(std::forward<Arg>(arg));
        _thread_variables[thread_id][key] = shared_ptr;
        return shared_ptr;
    }

    template<typename T>
    static std::shared_ptr<T> GetThreadVariable(std::uintptr_t key) {
        auto thread_id = std::this_thread::get_id();
        auto &map = _thread_variables[thread_id];
        if (map.contains(key)) {
            auto &shared_ptr = map[key];
            return std::shared_ptr<T>(shared_ptr, reinterpret_cast<T *>(shared_ptr.get()));
        }
        return nullptr;
    }

    static void InitThreadVariableMap() {
        auto thread_id = std::this_thread::get_id();
        std::unique_lock lock(_lock);
        _thread_variables[thread_id] = {};
    }

    static void ClearThreadVariables(std::vector<std::thread::id> &thread_ids) {
        std::unique_lock lock(_lock);
        for (auto &thread_id : thread_ids) {
            _thread_variables[thread_id].clear();
        }
    }

private:
    inline static std::mutex _lock;
    inline static std::mutex _shared_lock;

#if THREAD_VARIABLE_MAP_TYPE == 1
    typedef phmap::parallel_flat_hash_map<std::uintptr_t, std::shared_ptr<void>,
            phmap::priv::hash_default_hash<std::uintptr_t>,
            phmap::priv::hash_default_eq<std::uintptr_t>,
            std::allocator<std::pair<const std::uintptr_t, std::shared_ptr<void>>>, 4, std::shared_mutex>
            MapT;
#elif THREAD_VARIABLE_MAP_TYPE == 2
    typedef phmap::flat_hash_map<std::uintptr_t, std::shared_ptr<void>> MapT;
#else // THREAD_VARIABLE_MAP_TYPE == 3
    typedef phmap::node_hash_map<std::uintptr_t, std::shared_ptr<void>> MapT;
#endif
    inline static phmap::flat_hash_map<std::thread::id, MapT> _thread_variables;
    inline static MapT _shared_variables;
};
