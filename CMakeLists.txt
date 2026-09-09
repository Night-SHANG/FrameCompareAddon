cmake_minimum_required(VERSION 3.24)
project(FrameCompare VERSION 1.3.0 LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "FrameCompare is a Windows ReShade add-on and must be built with a Windows toolchain.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "FrameCompare v1.3 currently targets 64-bit ReShade only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# ReShade is used header-only by the add-on. Its imgui submodule must match the
# generated reshade_overlay.hpp function table in the same checkout.
set(FRAMECOMPARE_RESHADE_ROOT "" CACHE PATH "Existing ReShade source checkout (optional)")
if(FRAMECOMPARE_RESHADE_ROOT)
    set(reshade_SOURCE_DIR "${FRAMECOMPARE_RESHADE_ROOT}")
else()
    FetchContent_Declare(
        reshade_headers
        GIT_REPOSITORY https://github.com/crosire/reshade.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_SUBMODULES deps/imgui
        GIT_SUBMODULES_RECURSE TRUE
    )
    FetchContent_GetProperties(reshade_headers)
    if(NOT reshade_headers_POPULATED)
        FetchContent_Populate(reshade_headers)
    endif()
    set(reshade_SOURCE_DIR "${reshade_headers_SOURCE_DIR}")
endif()

if(NOT EXISTS "${reshade_SOURCE_DIR}/include/reshade.hpp")
    message(FATAL_ERROR "ReShade headers not found at ${reshade_SOURCE_DIR}")
endif()
if(NOT EXISTS "${reshade_SOURCE_DIR}/deps/imgui/imgui.h")
    message(FATAL_ERROR "ReShade-matched Dear ImGui headers not found. Clone ReShade with the imgui submodule.")
endif()

set(FRAMECOMPARE_MINHOOK_ROOT "" CACHE PATH "Existing MinHook source checkout (optional)")
if(FRAMECOMPARE_MINHOOK_ROOT)
    add_subdirectory("${FRAMECOMPARE_MINHOOK_ROOT}" "${CMAKE_BINARY_DIR}/minhook")
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        minhook_src
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(minhook_src)
endif()

add_library(FrameCompare SHARED
    src/FrameCompare.cpp
    src/NgxHook.cpp
    src/NgxHook.hpp
    src/NgxAbi.hpp
    src/FrameCompare.rc
)

target_include_directories(FrameCompare PRIVATE
    "${reshade_SOURCE_DIR}/include"
    "${reshade_SOURCE_DIR}/deps/imgui"
)

target_compile_definitions(FrameCompare PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    ImTextureID=ImU64
)

if(MSVC)
    target_compile_options(FrameCompare PRIVATE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
else()
    target_compile_options(FrameCompare PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(FrameCompare PRIVATE
    minhook
    d3d11
    d3d12
    dxgi
    user32
    psapi
)

set_target_properties(FrameCompare PROPERTIES
    PREFIX ""
    OUTPUT_NAME "00-FrameCompare"
    SUFFIX ".addon64"
)

add_custom_command(TARGET FrameCompare POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FrameCompare>/package"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FrameCompare>/package/reshade-shaders/Shaders"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:FrameCompare>" "$<TARGET_FILE_DIR:FrameCompare>/package/00-FrameCompare.addon64"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/shaders/FrameCompare.fx" "$<TARGET_FILE_DIR:FrameCompare>/package/reshade-shaders/Shaders/FrameCompare.fx"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/FrameCompare.ini.example" "$<TARGET_FILE_DIR:FrameCompare>/package/FrameCompare.ini.example"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "$<TARGET_FILE_DIR:FrameCompare>/package/README.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/docs/INSTALL_CN.md" "$<TARGET_FILE_DIR:FrameCompare>/package/INSTALL_CN.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/docs/FUNCTIONS_CN.md" "$<TARGET_FILE_DIR:FrameCompare>/package/FUNCTIONS_CN.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/docs/LOG_CN.md" "$<TARGET_FILE_DIR:FrameCompare>/package/LOG_CN.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/docs/AUDIT.md" "$<TARGET_FILE_DIR:FrameCompare>/package/AUDIT.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/CHANGELOG.md" "$<TARGET_FILE_DIR:FrameCompare>/package/CHANGELOG.md"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md" "$<TARGET_FILE_DIR:FrameCompare>/package/THIRD_PARTY.md"
)
