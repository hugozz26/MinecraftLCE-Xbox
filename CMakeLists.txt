# ==============================================================================
# CMakeLists_UWP.txt - UWP build for Xbox One Dev Mode
# ==============================================================================
# Usage:
#   mkdir build_uwp && cd build_uwp
#   cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_TOOLCHAIN_FILE=../UWP/CMakeLists_UWP.txt
#
# Or more simply, use the dedicated preset:
#   cmake --preset uwp-x64
#   cmake --build --preset uwp-x64 --config Release
# ==============================================================================

cmake_minimum_required(VERSION 3.24)

project(MinecraftLCE_UWP LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # C++/CX needs extensions

if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Use a 64-bit generator/toolchain (x64).")
endif()

# The pre-compiled 4J libs were built with /MT (static CRT). MSBuild rejects
# /MT for WindowsStore targets (MSB8024). WORKAROUND: set RuntimeLibrary to
# /MD (dynamic) which MSBuild accepts, but then add /MT as an additional
# compiler option. The LAST CRT flag on the cl.exe command line wins, so
# /MT overrides the /MD that comes from RuntimeLibrary. This satisfies
# MSBuild's validation while actually compiling with /MT to match 4J libs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ==============================================================================
# UWP-specific compile flags
# ==============================================================================
# Base compile flags shared by all targets (no /ZW, no /GL here)
function(configure_uwp_base target)
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/EHsc>     # Standard C++ exceptions
    $<$<COMPILE_LANGUAGE:C,CXX>:/MP>     # Multi-processor compilation
    $<$<COMPILE_LANGUAGE:C,CXX>:/FS>     # Force synchronous PDB writes
    $<$<COMPILE_LANGUAGE:C,CXX>:/bigobj> # Large .obj support for World lib
    # NOTE: /MT and /MTd are NOT set here — CMake detects them and converts
    # to <RuntimeLibrary> which triggers MSB8024 on WindowsStore.
    # Instead, we post-patch the vcxproj to inject /MT into <AdditionalOptions>.
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/O2 /Oi /GT /GF>
    $<$<AND:$<NOT:$<CONFIG:Release>>,$<COMPILE_LANGUAGE:C,CXX>>:/W3>
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/W0>
  )
  set_target_properties(${target} PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17763.0"
    VS_GLOBAL_MinimumVisualStudioVersion "16.0"
  )
endfunction()

# Full UWP flags including /ZW (only for targets with WinRT code)
function(configure_uwp_target target)
  configure_uwp_base(${target})
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/ZW>       # Enable C++/CX (for ref classes)
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:/GL> # WPO only for exe (World lib too big with /GL)
  )
endfunction()

# ==============================================================================
# World sources (static library - same as PC build)
# ==============================================================================
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WorldSources.cmake")
list(TRANSFORM MINECRAFT_WORLD_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/")

add_library(MinecraftWorld STATIC ${MINECRAFT_WORLD_SOURCES})
target_include_directories(MinecraftWorld PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
)
target_compile_definitions(MinecraftWorld PRIVATE
  _LARGE_WORLDS
  _LIB
  _CRT_NON_CONFORMING_SWPRINTFS
  _CRT_SECURE_NO_WARNINGS
  _WINDOWS64
  _UWP
  _CONTENT_PACKAGE
  $<$<CONFIG:Debug>:_DEBUG>
)
if(MSVC)
  configure_uwp_base(MinecraftWorld)  # No /ZW - World has no WinRT code
endif()

# ==============================================================================
# Client sources - REUSE PC sources but REPLACE Windows64_Minecraft.cpp
# ==============================================================================
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClientSources.cmake")
list(TRANSFORM MINECRAFT_CLIENT_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/")

# Remove Win32-specific entry point (replaced by UWP_App.cpp)
list(REMOVE_ITEM MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Windows64_Minecraft.cpp"
)

# Remove Win32 resource file
list(REMOVE_ITEM MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/MinecraftWindows.rc"
)

# Add UWP-specific sources
list(APPEND MINECRAFT_CLIENT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/UWP_App.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/crt_compat.cpp"
)

# ==============================================================================
# Build the UWP executable
# ==============================================================================
# For UWP, we don't use WIN32 subsystem - the entry point is main()
add_executable(MinecraftLCE ${MINECRAFT_CLIENT_SOURCES})

target_include_directories(MinecraftLCE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Xbox/Sentient/Include"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.World/x64headers"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/"
)

target_compile_definitions(MinecraftLCE PRIVATE
  _LARGE_WORLDS
  _CRT_NON_CONFORMING_SWPRINTFS
  _CRT_SECURE_NO_WARNINGS
  _WINDOWS64
  _UWP
  _CONTENT_PACKAGE
  $<$<CONFIG:Debug>:_DEBUG>
)

if(MSVC)
  configure_uwp_target(MinecraftLCE)
  target_link_options(MinecraftLCE PRIVATE
    $<$<CONFIG:Release>:/LTCG /INCREMENTAL:NO>
    # NOTE: /APPCONTAINER is intentionally OMITTED.
    # The pre-compiled 4J .libs use desktop Win32 APIs that would fail
    # the /APPCONTAINER link check. Xbox Dev Mode is more permissive
    # than standard UWP — it allows most Win32 APIs.
    # For sideloading on Xbox Dev Mode this is fine.
    #
    # Our code uses /MD (dynamic CRT, required for /ZW C++/CX).
    # The 4J libs were compiled with /MT and have DEFAULTLIB directives for
    # static CRT libs (libcmt, libcpmt). Suppress those to prevent
    # multiply-defined symbols — all CRT functions come from msvcrt/msvcprt.
    /NODEFAULTLIB:libcmt.lib
    /NODEFAULTLIB:libcmtd.lib
    /NODEFAULTLIB:libcpmt.lib
    /NODEFAULTLIB:libcpmtd.lib
  )
  # We need the regular VC x64 lib path in addition to the store path
  # because some CRT/STL libs may not be in the store path.
  target_link_directories(MinecraftLCE PRIVATE
    "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/lib/x64"
  )
endif()

# ==============================================================================
# Link libraries
# ==============================================================================
# UWP uses d3d11.lib and dxgi.lib; XInput9_1_0 for the 4J_Input lib;
# ws2_32 for WinsockNetLayer; WindowsApp.lib for UWP runtime umbrella.

target_link_libraries(MinecraftLCE PRIVATE
  MinecraftWorld
  d3d11
  dxgi
  ws2_32
  xinput
  legacy_stdio_definitions

  # Iggy UI libraries (pre-compiled, should work as static libs)
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggy_w64.lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggyperfmon_w64.lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/Iggy/lib/iggyexpruntime_w64.lib"

  # 4J Studios libraries (pre-compiled static libs)
  $<$<CONFIG:Debug>:
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage_d.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC_d.lib"
  >
  $<$<NOT:$<CONFIG:Debug>>:
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Input.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Storage.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/Windows64/4JLibs/libs/4J_Render_PC.lib"
  >

  # UWP platform libraries (umbrella)
  WindowsApp.lib

  # user32.lib for Win32 APIs used by game code (clipboard, cursor, raw input,
  # GetClientRect). Not in WindowsApp.lib umbrella. Since /APPCONTAINER is
  # omitted (Dev Mode sideload), linking user32 directly works fine.
  user32.lib
)

# ==============================================================================
# UWP Package configuration
# ==============================================================================
# Set the AppX manifest
set_property(TARGET MinecraftLCE PROPERTY VS_APPX_MANIFEST
  "${CMAKE_CURRENT_SOURCE_DIR}/UWP/Package.appxmanifest"
)

# Placeholder logo assets (you need to create these PNG files)
# 150x150, 44x44, 310x150, 310x310, 620x300 splash
set(UWP_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/UWP/Assets")

# ==============================================================================
# Asset copying (same as PC build but adapted)
# ==============================================================================
if(CMAKE_HOST_WIN32)
  message(STATUS "Starting redist copy for UWP build...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/x64/Release"
            "${CMAKE_CURRENT_BINARY_DIR}"
            /S /MT /R:0 /W:0 /NP
  )
  message(STATUS "Starting asset copy for UWP build...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client"
            "${CMAKE_CURRENT_BINARY_DIR}"
            /S /MT /R:0 /W:0 /NP
            /XF "*.cpp" "*.c" "*.h" "*.hpp" "*.asm"
                "*.xml" "*.lang" "*.vcxproj" "*.vcxproj.*" "*.sln"
                "*.docx" "*.xls"
                "*.bat" "*.cmd" "*.ps1" "*.py"
                "*Test*"
            /XD "Durango*" "Orbis*" "PS*" "Xbox"
  )
  message(STATUS "Patching Windows64Media for UWP...")
  execute_process(
    COMMAND robocopy.exe
            "${CMAKE_CURRENT_SOURCE_DIR}/Minecraft.Client/DurangoMedia"
            "${CMAKE_CURRENT_BINARY_DIR}/Windows64Media"
            /S /MT /R:0 /W:0 /NP
            /XF "*.h" "*.xml" "*.lang" "*.bat"
  )
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MinecraftLCE)

# NOTE: After running cmake configure, you MUST run the vcxproj post-patch
# script to inject /MT into AdditionalOptions. See the build instructions.
# The <RuntimeLibrary> is set to /MD (DLL) to pass MSBuild's MSB8024 check,
# but we need /MT at compile time to match the 4J precompiled libs.
