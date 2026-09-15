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
option(DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS "Compare raw member identities without materializing descriptors" OFF)
option(DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS "Publish stable standard strings through dense atomic pointers" OFF)
option(DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS "Store descriptors in paged stable byte records" OFF)
option(DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS "Align paged descriptor bodies to 16 bytes" OFF)
option(DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS "Separate descriptor cache hits from cold construction" OFF)
if (DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS AND NOT (DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS OR DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS OR DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS))
    message(FATAL_ERROR "Descriptor fast hits require a published descriptor cache")
endif ()
if (DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS AND DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS)
    message(FATAL_ERROR "Choose only one descriptor storage experiment")
endif ()
if (DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS AND NOT DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS)
    message(FATAL_ERROR "Aligned descriptor bodies require paged descriptors")
endif ()
option(DEXKIT_EXPERIMENT_RAW_DESCRIPTOR_LOOKUP "Use raw structural descriptor lookup instead of cached text" OFF)
if (DEXKIT_EXPERIMENT_RAW_DESCRIPTOR_LOOKUP AND NOT DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS)
    message(FATAL_ERROR "Raw descriptor lookup requires structural descriptor publication support")
endif ()
option(DEXKIT_EXPERIMENT_RAW_INTERFACES "Borrow raw DEX interface type lists" OFF)
option(DEXKIT_EXPERIMENT_COMPACT_INVOKES "Build forward invocation rows in contiguous storage" OFF)
option(DEXKIT_EXPERIMENT_RAW_SOURCE_FILES "Read source file views through existing ClassDef indexes" OFF)
option(DEXKIT_EXPERIMENT_SINGLE_RELATION "Match one invocation or caller requirement without a general solver" OFF)
option(DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT "Resolve field identity without building reverse field graphs" OFF)
option(DEXKIT_EXPERIMENT_SINGLE_STRING_DIRECT "Direct comparison for one ordinary usingStrings condition" OFF)
option(DEXKIT_EXPERIMENT_SINGLE_STRING_ID "Sorted-pool ID lookup for one ordinary usingStrings condition" OFF)
option(DEXKIT_EXPERIMENT_SINGLE_STRING_PREFIX "Include StartWith in the single usingStrings experiment" OFF)
option(DEXKIT_BENCHMARK_STRING_TRACE "Count ordinary usingStrings work per query task" OFF)
option(DEXKIT_EXPERIMENT_BATCH_STRING_INCLUDES "Check batch keyword containment without an intersection vector" OFF)
option(DEXKIT_BENCHMARK_BATCH_TRACE "Count batch group and intersection work per DEX task" OFF)
option(DEXKIT_EXPERIMENT_SINGLE_USING_FIELD "Match one using-field requirement without a general solver" OFF)
option(DEXKIT_EXPERIMENT_COMPACT_FIELDS "Build forward field-use rows in contiguous storage" OFF)
option(DEXKIT_BENCHMARK_FIELD_TRACE "Count using-field preparation and judges per query task" OFF)
if (DEXKIT_EXPERIMENT_SINGLE_STRING_DIRECT AND DEXKIT_EXPERIMENT_SINGLE_STRING_ID)
    message(FATAL_ERROR "Choose one single-string matching experiment")
endif ()
if (DEXKIT_EXPERIMENT_SINGLE_STRING_PREFIX AND NOT (DEXKIT_EXPERIMENT_SINGLE_STRING_DIRECT OR DEXKIT_EXPERIMENT_SINGLE_STRING_ID))
    message(FATAL_ERROR "The prefix extension requires a single-string experiment")
endif ()
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
        DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS=$<BOOL:${DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS}>
        DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS=$<BOOL:${DEXKIT_EXPERIMENT_POINTER_DESCRIPTORS}>
        DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS=$<BOOL:${DEXKIT_EXPERIMENT_PAGED_DESCRIPTORS}>
        DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS=$<BOOL:${DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS}>
        DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS=$<BOOL:${DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS}>
        DEXKIT_EXPERIMENT_RAW_DESCRIPTOR_LOOKUP=$<BOOL:${DEXKIT_EXPERIMENT_RAW_DESCRIPTOR_LOOKUP}>
        DEXKIT_EXPERIMENT_RAW_INTERFACES=$<BOOL:${DEXKIT_EXPERIMENT_RAW_INTERFACES}>
        DEXKIT_EXPERIMENT_COMPACT_INVOKES=$<BOOL:${DEXKIT_EXPERIMENT_COMPACT_INVOKES}>
        DEXKIT_EXPERIMENT_RAW_SOURCE_FILES=$<BOOL:${DEXKIT_EXPERIMENT_RAW_SOURCE_FILES}>
        DEXKIT_EXPERIMENT_SINGLE_RELATION=$<BOOL:${DEXKIT_EXPERIMENT_SINGLE_RELATION}>
        DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT=$<BOOL:${DEXKIT_EXPERIMENT_FIELD_IDENTITY_SPLIT}>
        DEXKIT_EXPERIMENT_SINGLE_STRING_DIRECT=$<BOOL:${DEXKIT_EXPERIMENT_SINGLE_STRING_DIRECT}>
        DEXKIT_EXPERIMENT_SINGLE_STRING_ID=$<BOOL:${DEXKIT_EXPERIMENT_SINGLE_STRING_ID}>
        DEXKIT_EXPERIMENT_SINGLE_STRING_PREFIX=$<BOOL:${DEXKIT_EXPERIMENT_SINGLE_STRING_PREFIX}>
        DEXKIT_BENCHMARK_STRING_TRACE=$<BOOL:${DEXKIT_BENCHMARK_STRING_TRACE}>
        DEXKIT_EXPERIMENT_BATCH_STRING_INCLUDES=$<BOOL:${DEXKIT_EXPERIMENT_BATCH_STRING_INCLUDES}>
        DEXKIT_BENCHMARK_BATCH_TRACE=$<BOOL:${DEXKIT_BENCHMARK_BATCH_TRACE}>
        DEXKIT_EXPERIMENT_SINGLE_USING_FIELD=$<BOOL:${DEXKIT_EXPERIMENT_SINGLE_USING_FIELD}>
        DEXKIT_EXPERIMENT_COMPACT_FIELDS=$<BOOL:${DEXKIT_EXPERIMENT_COMPACT_FIELDS}>
        DEXKIT_BENCHMARK_FIELD_TRACE=$<BOOL:${DEXKIT_BENCHMARK_FIELD_TRACE}>
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
option(DEXKIT_BENCHMARK_SYMBOL_CHECKS "Build descriptor identity and lifetime checks" OFF)
option(DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD "Build adverse descriptor and interface workloads" OFF)
option(DEXKIT_BENCHMARK_RELATION_CHECKS "Build cross-DEX field and invoke lifecycle checks" OFF)
option(DEXKIT_BENCHMARK_RELATION_WORKLOAD "Build adverse field and invoke workloads" OFF)
option(DEXKIT_BENCHMARK_SOURCE_CHECKS "Build raw source metadata checks" OFF)
option(DEXKIT_BENCHMARK_SOURCE_WORKLOAD "Build source matcher and class output workloads" OFF)
option(DEXKIT_BENCHMARK_STRING_WORKLOAD "Build ordinary usingStrings checks and workloads" OFF)
option(DEXKIT_BENCHMARK_BATCH_WORKLOAD "Build batch usingStrings checks and workloads" OFF)
option(DEXKIT_BENCHMARK_FIELD_WORKLOAD "Build using-field checks and workloads" OFF)
if (DEXKIT_BENCHMARK_FIELD_WORKLOAD)
    add_executable(dexkit_field_checks ../tools/qaux-benchmark/field_checks.cpp)
    target_link_libraries(dexkit_field_checks PRIVATE dexkit_static)
    add_executable(dexkit_field_workload ../tools/qaux-benchmark/field_workload.cpp)
    target_link_libraries(dexkit_field_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_BATCH_WORKLOAD)
    add_executable(dexkit_batch_checks ../tools/qaux-benchmark/batch_checks.cpp)
    target_link_libraries(dexkit_batch_checks PRIVATE dexkit_static)
    add_executable(dexkit_batch_workload ../tools/qaux-benchmark/batch_workload.cpp)
    target_link_libraries(dexkit_batch_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_STRING_WORKLOAD)
    add_executable(dexkit_string_checks ../tools/qaux-benchmark/string_checks.cpp)
    target_link_libraries(dexkit_string_checks PRIVATE dexkit_static)
    add_executable(dexkit_string_workload ../tools/qaux-benchmark/string_workload.cpp)
    target_link_libraries(dexkit_string_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_SOURCE_WORKLOAD)
    add_executable(dexkit_source_workload ../tools/qaux-benchmark/source_workload.cpp)
    target_link_libraries(dexkit_source_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_SOURCE_CHECKS)
    if (NOT DEXKIT_BENCHMARK_DIAGNOSTICS)
        message(FATAL_ERROR "Source checks require benchmark diagnostics")
    endif ()
    add_executable(dexkit_source_checks ../tools/qaux-benchmark/source_checks.cpp)
    target_link_libraries(dexkit_source_checks PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_RELATION_WORKLOAD)
    add_executable(dexkit_relation_workload ../tools/qaux-benchmark/relation_workload.cpp)
    target_link_libraries(dexkit_relation_workload PRIVATE dexkit_static)
    add_executable(dexkit_invocation_workload ../tools/qaux-benchmark/invocation_workload.cpp)
    target_link_libraries(dexkit_invocation_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_RELATION_CHECKS)
    if (NOT DEXKIT_BENCHMARK_DIAGNOSTICS)
        message(FATAL_ERROR "Relation checks require benchmark diagnostics")
    endif ()
    add_executable(dexkit_relation_checks ../tools/qaux-benchmark/relation_checks.cpp)
    target_link_libraries(dexkit_relation_checks PRIVATE dexkit_static)
    add_executable(dexkit_invocation_checks ../tools/qaux-benchmark/invocation_checks.cpp)
    target_link_libraries(dexkit_invocation_checks PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD)
    add_executable(dexkit_descriptor_workload ../tools/qaux-benchmark/descriptor_workload.cpp)
    target_link_libraries(dexkit_descriptor_workload PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_SYMBOL_CHECKS)
    if (NOT DEXKIT_BENCHMARK_DIAGNOSTICS)
        message(FATAL_ERROR "Symbol checks require benchmark diagnostics")
    endif ()
    add_executable(dexkit_symbol_checks ../tools/qaux-benchmark/symbol_checks.cpp)
    target_link_libraries(dexkit_symbol_checks PRIVATE dexkit_static)
    add_executable(dexkit_paged_descriptor_checks ../tools/qaux-benchmark/paged_descriptor_checks.cpp)
    target_link_libraries(dexkit_paged_descriptor_checks PRIVATE dexkit_static)
endif ()
if (DEXKIT_BENCHMARK_MEMO_CHECKS)
    if (NOT DEXKIT_EXPERIMENT_NEGATIVE_STRINGS)
        message(FATAL_ERROR "Memo checks require the negative string experiment")
    endif ()
    add_executable(dexkit_memo_checks ../tools/qaux-benchmark/memo_checks.cpp)
    target_link_libraries(dexkit_memo_checks PRIVATE dexkit_static)
    add_executable(dexkit_memo_budget_checks ../tools/qaux-benchmark/memo_budget_checks.cpp)
    target_link_libraries(dexkit_memo_budget_checks PRIVATE dexkit_static)
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
