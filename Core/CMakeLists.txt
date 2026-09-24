cmake_minimum_required(VERSION 3.4.1...3.27)
project(dexkit)

find_program(CCACHE ccache)

if (CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
endif ()

set(CMAKE_CXX_STANDARD 20)
option(DEXKIT_ENABLE_INTERNAL_METRICS "Compile internal metrics into core" OFF)
option(DEXKIT_ENABLE_SMALI "Compile native smali output" ON)

file(GLOB_RECURSE SLICER_SRC_FILES CONFIGURE_DEPENDS third_party/slicer/*.cc)
file(GLOB_RECURSE DEXKIT_SRC_FILES CONFIGURE_DEPENDS dexkit/*.cpp)
if (NOT DEXKIT_ENABLE_SMALI)
    list(FILTER DEXKIT_SRC_FILES EXCLUDE REGEX "/smali/.*\\.cpp$")
endif ()

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
target_compile_definitions(${PROJECT_NAME}_static PUBLIC DEXKIT_ENABLE_SMALI=$<BOOL:${DEXKIT_ENABLE_SMALI}>)
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
if (EXECUTABLE_TEST)
    link_libraries(z ${PROJECT_NAME}_static)
    add_executable(${PROJECT_NAME}
            main.cpp
            )
endif ()

option(DEXKIT_BUILD_SMALI_TESTS "Build native smali regression tests" OFF)
if (DEXKIT_BUILD_SMALI_TESTS)
    if (NOT DEXKIT_ENABLE_SMALI)
        message(FATAL_ERROR "Smali tests require DEXKIT_ENABLE_SMALI")
    endif ()
    enable_testing()
    add_executable(dexkit_smali_tests tests/smali_reader_test.cpp)
    target_link_libraries(dexkit_smali_tests PRIVATE dexkit_static)
    add_test(NAME smali_checked_reader COMMAND dexkit_smali_tests)
    add_executable(dexkit_smali_writer_harness tests/smali_writer_harness.cpp)
    target_link_libraries(dexkit_smali_writer_harness PRIVATE dexkit_static)
endif ()

option(DEXKIT_BUILD_LOADER_TESTS "Build native archive loading regression tests" OFF)
if (DEXKIT_BUILD_LOADER_TESTS)
    enable_testing()
    add_executable(dexkit_zip_archive_tests tests/zip_archive_test.cpp)
    target_link_libraries(dexkit_zip_archive_tests PRIVATE dexkit_static)
    add_test(NAME zip_archive_batch COMMAND dexkit_zip_archive_tests)
endif ()
