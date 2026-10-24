cmake_minimum_required(VERSION 3.15)
project(AnimatedNeonRadar LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Animated Neon Radar - standalone GTA Vice City ASI plugin
# Built against dk22pac/plugin-sdk (https://github.com/dk22pac/plugin-sdk),
# targeting i686-w64-mingw32-g++.
#
# Typical configure/build (from a Linux host with mingw-w64 installed):
#
#   cmake -B build -S . \
#       -DCMAKE_SYSTEM_NAME=Windows \
#       -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
#       -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
#       -DPLUGIN_SDK_DIR=/path/to/plugin-sdk \
#       -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Output: build/AnimatedNeonRadar.asi — drop it into Vice City's install
# directory alongside an ASI loader (e.g. silent's ASI loader).
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(WARNING
        "CMAKE_SYSTEM_NAME is not 'Windows' — this plugin targets the VC "
        ".exe, so configure with -DCMAKE_SYSTEM_NAME=Windows and the "
        "i686-w64-mingw32 toolchain or the result won't be a usable .asi.")
endif()

# Path to a local clone of https://github.com/dk22pac/plugin-sdk.
# Override with: -DPLUGIN_SDK_DIR=/path/to/plugin-sdk
if(NOT PLUGIN_SDK_DIR)
    set(PLUGIN_SDK_DIR "${CMAKE_SOURCE_DIR}/plugin-sdk" CACHE PATH "plugin-sdk checkout")
endif()

if(NOT EXISTS "${PLUGIN_SDK_DIR}/plugin_vc/game_vc")
    message(FATAL_ERROR
        "plugin-sdk not found at '${PLUGIN_SDK_DIR}' (expected a "
        "plugin_vc/game_vc subfolder there). Clone "
        "https://github.com/dk22pac/plugin-sdk and pass -DPLUGIN_SDK_DIR=<path>.")
endif()

# ---- plugin-sdk sources needed to build a GTA:VC plugin ----
# Mirrors the SDK's own "Plugin_VC" static-lib build (see
# tools/premake/premake5.lua -> pluginSdkStaticLibProject), folded directly
# into this one target instead of a separate prebuilt .a, so everything is
# compiled with the same flags in a single pass.
file(GLOB_RECURSE PLUGINSDK_VC_SOURCES        "${PLUGIN_SDK_DIR}/plugin_vc/*.cpp")
file(GLOB_RECURSE PLUGINSDK_SHARED_SOURCES    "${PLUGIN_SDK_DIR}/shared/*.cpp")
file(GLOB_RECURSE PLUGINSDK_HOOKING_SOURCES   "${PLUGIN_SDK_DIR}/hooking/*.cpp")
file(GLOB_RECURSE PLUGINSDK_SAFETYHOOK_SOURCES
    "${PLUGIN_SDK_DIR}/safetyhook/*.cpp"
    "${PLUGIN_SDK_DIR}/safetyhook/*.c"
)

# Exclude pieces that need the DirectX SDK / D3D9-RenderWare headers (not
# needed for this plugin, and not present on a bare mingw toolchain), and the
# MSVC-only precompiled-header stub.
list(FILTER PLUGINSDK_SHARED_SOURCES EXCLUDE REGEX "dxsdk/|rwd3d9|stdafx\\.cpp$")

add_library(AnimatedNeonRadar SHARED
    src/AnimatedNeonRadar.cpp
    ${PLUGINSDK_VC_SOURCES}
    ${PLUGINSDK_SHARED_SOURCES}
    ${PLUGINSDK_HOOKING_SOURCES}
    ${PLUGINSDK_SAFETYHOOK_SOURCES}
)

target_include_directories(AnimatedNeonRadar PRIVATE
    "${PLUGIN_SDK_DIR}/plugin_vc"
    "${PLUGIN_SDK_DIR}/plugin_vc/game_vc"
    "${PLUGIN_SDK_DIR}/plugin_vc/game_vc/enums"
    "${PLUGIN_SDK_DIR}/plugin_vc/game_vc/rw"
    "${PLUGIN_SDK_DIR}/shared"
    "${PLUGIN_SDK_DIR}/shared/game"
    "${PLUGIN_SDK_DIR}/injector"
    "${PLUGIN_SDK_DIR}/safetyhook"
)

# GTAVC selects Vice City throughout the SDK headers; PLUGIN_SGV_10EN/RW are
# the game-version/RenderWare defines the SDK's own VC build uses.
target_compile_definitions(AnimatedNeonRadar PRIVATE
    GTAVC
    PLUGIN_SGV_10EN
    RW
)

# Flags the SDK itself uses for GCC-family compilers on its gmake/mingw path
# (see tools/premake/premake5.lua). plugin-sdk leans on MSVC extensions
# (anonymous structs/unions, offsetof on non-standard-layout types), hence
# -fpermissive / -fms-extensions / -Wno-invalid-offsetof.
target_compile_options(AnimatedNeonRadar PRIVATE
    -fpermissive
    -fms-extensions
    -Wno-invalid-offsetof
    -Wno-builtin-macro-redefined
)

set_target_properties(AnimatedNeonRadar PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
)

# Static-link the runtime so the .asi doesn't need libgcc/libstdc++ DLLs
# sitting next to the game executable.
target_link_options(AnimatedNeonRadar PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
)

# -----------------------------------------------------------------------------
# NOTE: plugin-sdk evolves, and the exact source layout / required defines
# can drift between checkouts. If compilation fails on a header plugin-sdk
# ships, cross-check against PLUGIN_SDK_DIR/tools/premake/premake5.lua in
# your own checkout (function pluginSdkStaticLibProject / getExamplePlugin*)
# for whatever changed.
# -----------------------------------------------------------------------------
