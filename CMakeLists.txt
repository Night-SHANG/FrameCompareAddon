cmake_minimum_required(VERSION 3.24)
project(FrameCompareV2 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(FRAMECOMPARE_BUILD_ADDON "Build the ReShade add-on" ON)
option(FRAMECOMPARE_BUILD_TESTS "Build core unit tests" ON)

add_library(framecompare_core STATIC
    src/core/capture_cycle.cpp
    src/core/frame_pair.cpp
    src/render/compositor_params.cpp
)
target_include_directories(framecompare_core PUBLIC src)

if(FRAMECOMPARE_BUILD_TESTS)
    enable_testing()
    add_executable(frame_pair_tests tests/frame_pair_tests.cpp)
    target_link_libraries(frame_pair_tests PRIVATE framecompare_core)
    add_test(NAME frame_pair_tests COMMAND frame_pair_tests)

    add_executable(compositor_params_tests tests/compositor_params_tests.cpp)
    target_link_libraries(compositor_params_tests PRIVATE framecompare_core)
    add_test(NAME compositor_params_tests COMMAND compositor_params_tests)
endif()

if(FRAMECOMPARE_BUILD_ADDON)
    if(NOT WIN32)
        message(FATAL_ERROR "FrameCompare v2 add-on requires Windows.")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        reshade_headers
        GIT_REPOSITORY https://github.com/crosire/reshade.git
        GIT_TAG v6.8.0
        GIT_SHALLOW TRUE
        GIT_SUBMODULES deps/imgui
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_GetProperties(reshade_headers)
    if(NOT reshade_headers_POPULATED)
        FetchContent_Populate(reshade_headers)
    endif()

    add_library(FrameCompareV2 SHARED
        src/addon_entry.cpp
        src/capture/reshade_capture.cpp
        src/render/compositor.cpp
        src/ui/panel.cpp
    )
    target_include_directories(FrameCompareV2 PRIVATE
        src
        "${reshade_headers_SOURCE_DIR}/include"
        "${reshade_headers_SOURCE_DIR}/deps/imgui"
    )
    target_compile_definitions(FrameCompareV2 PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        ImTextureID=ImU64
    )
    target_link_libraries(FrameCompareV2 PRIVATE framecompare_core)
    if(MSVC)
        target_compile_options(FrameCompareV2 PRIVATE /W4 /permissive- /utf-8 /EHsc)
    endif()
    set_target_properties(FrameCompareV2 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "00-FrameCompare-v2"
        SUFFIX ".addon64"
    )
    add_custom_command(TARGET FrameCompareV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:FrameCompareV2>/package/reshade-shaders/Shaders"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:FrameCompareV2>"
            "$<TARGET_FILE_DIR:FrameCompareV2>/package/00-FrameCompare-v2.addon64"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/FrameCompare.fx"
            "$<TARGET_FILE_DIR:FrameCompareV2>/package/reshade-shaders/Shaders/FrameCompare.fx"
    )
endif()
