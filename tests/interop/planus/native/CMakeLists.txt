cmake_minimum_required(VERSION 3.20)
project(dexkit_reference LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
add_library(dexkit_reference STATIC reference.cpp)
target_include_directories(dexkit_reference PRIVATE
    "${DEXKIT_ROOT}/Core/dexkit/include"
    "${DEXKIT_ROOT}/Core/third_party/slicer/export"
    "${DEXKIT_ROOT}/Core/third_party/flatbuffers/include"
    "${DEXKIT_ROOT}/Core/third_party/thread_helper"
    "${DEXKIT_ROOT}/Core/third_party/parallel_hashmap"
    "${DEXKIT_ROOT}/Core/third_party/aho_corasick_trie")
# Core is supplied by the production dexkit-sys crate, not built a second time.
install(TARGETS dexkit_reference ARCHIVE DESTINATION lib)
