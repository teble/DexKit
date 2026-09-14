cmake_minimum_required(VERSION 3.4.1...3.27)
project(dexkit)

find_program(CCACHE ccache)

if (CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif ()

set(CMAKE_CXX_STANDARD 20)
option(DEXKIT_ENABLE_INTERNAL_METRICS "Compile internal metrics into core" OFF)
option(DEXKIT_BENCHMARK_DIAGNOSTICS "Compile benchmark-only cache diagnostics" OFF)
option(DEXKIT_EXPERIMENT_LAZY_DIRECTORIES "Allocate lazy-slot directories on first use" OFF)
option(DEXKIT_EXPERIMENT_COMPACT_STRINGS "Build method string uses in contiguous storage" OFF)
option(DEXKIT_EXPERIMENT_NEGATIVE_STRINGS "Memoize empty batch trie parses within a query" OFF)
set(DEXKIT_EXPERIMENT_STRING_MEMO_BYTES 1048576 CACHE STRING "Per-query negative string memo payload budget")
if (NOT DEXKIT_EXPERIMENT_STRING_MEMO_BYTES MATCHES "^[0-9]+$")
    message(FATAL_ERROR "String memo budget must be a nonnegative integer")
endif ()

file(GLOB_RECURSE SLICER_SRC_FILES CONFIGURE_DEPENDS third_party/slicer/*.cc)
file(GLOB_RECURSE DEXKIT_SRC_FILES CONFIGURE_DEPENDS dexkit/*.cpp)

set(SLICER_SOURCES
        ${SLICER_SRC_FILES}
)

set(COMPILE_FLAGS "-Wno-empty-body -Wno-unused-private-field \
    -Wno-unused-value -Wno-unused-variable -Wno-unused-function")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_library(${PROJECT_NAME}_static
        STATIC
        ${SLICER_SOURCES}
        ${DEXKIT_SRC_FILES}
)
target_include_directories(${PROJECT_NAME}_static
        PUBLIC
        dexkit/include
        third_party/slicer/export
        third_party/thread_helper
        third_party/aho_corasick_trie
        third_party/parallel_hashmap
        third_party/flatbuffers/include
)

set_target_properties(${PROJECT_NAME}_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME}_static PUBLIC
        DEXKIT_BENCHMARK_DIAGNOSTICS=$<BOOL:${DEXKIT_BENCHMARK_DIAGNOSTICS}>
        DEXKIT_EXPERIMENT_LAZY_DIRECTORIES=$<BOOL:${DEXKIT_EXPERIMENT_LAZY_DIRECTORIES}>
        DEXKIT_EXPERIMENT_COMPACT_STRINGS=$<BOOL:${DEXKIT_EXPERIMENT_COMPACT_STRINGS}>
        DEXKIT_EXPERIMENT_NEGATIVE_STRINGS=$<BOOL:${DEXKIT_EXPERIMENT_NEGATIVE_STRINGS}>
        DEXKIT_EXPERIMENT_STRING_MEMO_BYTES=${DEXKIT_EXPERIMENT_STRING_MEMO_BYTES})
if (DEXKIT_ENABLE_INTERNAL_METRICS)
    target_compile_definitions(${PROJECT_NAME}_static PUBLIC DEXKIT_ENABLE_INTERNAL_METRICS=1)
else ()
    target_compile_definitions(${PROJECT_NAME}_static PUBLIC DEXKIT_ENABLE_INTERNAL_METRICS=0)
endif ()
target_link_libraries(${PROJECT_NAME}_static
        PUBLIC
        z
)

option(EXECUTABLE_TEST "execute test cpp" OFF)
option(DEXKIT_BENCHMARK_METADATA_CHECKS "Build exhaustive demo metadata checks" OFF)
option(DEXKIT_BENCHMARK_MEMO_CHECKS "Build bounded string memo counterexamples" OFF)
if (DEXKIT_BENCHMARK_MEMO_CHECKS)
    if (NOT DEXKIT_EXPERIMENT_NEGATIVE_STRINGS)
        message(FATAL_ERROR "Memo checks require the negative string experiment")
    endif ()
    add_executable(dexkit_memo_checks ../tools/qaux-benchmark/memo_checks.cpp)
    target_link_libraries(dexkit_memo_checks PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_METADATA_CHECKS)
    if (NOT DEXKIT_BENCHMARK_DIAGNOSTICS)
        message(FATAL_ERROR "Metadata checks require benchmark diagnostics")
    endif ()
    add_executable(dexkit_metadata_checks ../tools/qaux-benchmark/metadata_checks.cpp)
    target_link_libraries(dexkit_metadata_checks PRIVATE dexkit_static)
endif ()
if (EXECUTABLE_TEST)
    link_libraries(z ${PROJECT_NAME}_static)
    add_executable(${PROJECT_NAME}
            main.cpp
            )
endif ()
