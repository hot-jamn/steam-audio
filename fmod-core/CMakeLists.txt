# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.17)

project(Phonon VERSION 4.6.1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH "${CMAKE_HOME_DIRECTORY}/build")


#
# SYSTEM INTROSPECTION
#

# OS detection
if (WIN32)
    set(IPL_OS_WINDOWS TRUE)
elseif (UNIX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(IPL_OS_LINUX TRUE)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(IPL_OS_MACOS TRUE)
elseif (ANDROID)
    set(IPL_OS_ANDROID TRUE)
elseif (APPLE AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IPL_OS_IOS TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(IPL_OS_WASM TRUE)
endif()

# CPU architecture detection
if (IPL_OS_WINDOWS OR IPL_OS_LINUX)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IPL_CPU_X64 TRUE)
    else()
        set(IPL_CPU_X86 TRUE)
    endif()
elseif (IPL_OS_MACOS)
elseif (IPL_OS_ANDROID)
    if (CMAKE_ANDROID_ARCH STREQUAL "arm")
        set(IPL_CPU_ARMV7 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "arm64")
        set(IPL_CPU_ARMV8 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "x86")
        set(IPL_CPU_X86 TRUE)
    elseif (CMAKE_ANDROID_ARCH STREQUAL "x86_64")
        set(IPL_CPU_X64 TRUE)
    endif()
elseif (IPL_OS_IOS)
    set(IPL_CPU_ARMV8 TRUE)
elseif (IPL_OS_WASM)
    set(IPL_CPU_ARMV7 TRUE)
endif()


#
# OPTIONS
#

# Whether to compile a static or dynamic library
option(BUILD_SHARED_LIBS "Build shared libraries instead of static." ON)

# Options for all platforms
option(STEAMAUDIOFMODCORE_BUILD_DOCS "Build documentation." OFF)

# Enhanced FMOD Core integration options
option(STEAMAUDIO_FMODCORE_BUILD_ENHANCED "Build enhanced FMOD Core plugin with error handling and performance monitoring" ON)
option(STEAMAUDIO_FMODCORE_BUILD_ORIGINAL "Build original FMOD Core plugin for compatibility" ON)

# Paths for find_package
set(Sphinx_EXECUTABLE_DIR "" CACHE PATH "Directory containing the Sphinx binary.")
mark_as_advanced(Sphinx_EXECUTABLE_DIR)


#
# COMPILER FLAGS
#

# Flags determined by system introspection
if (IPL_OS_WINDOWS)
    add_definitions(-DIPL_OS_WINDOWS)
endif()
if (IPL_OS_LINUX)
    add_definitions(-DIPL_OS_LINUX)
endif()
if (IPL_OS_MACOS)
    add_definitions(-DIPL_OS_MACOSX)
endif()
if (IPL_OS_ANDROID)
    add_definitions(-DIPL_OS_ANDROID)
endif()
if (IPL_OS_IOS)
    add_definitions(-DIPL_OS_IOS)
endif()
if (IPL_OS_WASM)
    add_definitions(-DIPL_OS_WASM)
endif()

if (IPL_CPU_X86)
    add_definitions(-DIPL_CPU_X86)
endif()
if (IPL_CPU_X64)
    add_definitions(-DIPL_CPU_X64)
endif()
if (IPL_CPU_ARMV7)
    add_definitions(-DIPL_CPU_ARMV7)
endif()
if (IPL_CPU_ARMV8)
    add_definitions(-DIPL_CPU_ARM64)
endif()

# Windows flags
if (IPL_OS_WINDOWS)
    add_compile_definitions(
        WIN32
        WINDOWS
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _DISABLE_EXTENDED_ALIGNED_STORAGE
        $<IF:$<CONFIG:Debug>,_DEBUG,NDEBUG>
    )
    add_compile_options(
        /EHsc /MP /W3 /Zi
        $<IF:$<CONFIG:Debug>,/MTd,/MT>
        $<$<CONFIG:Debug>:/RTC1>
        $<$<NOT:$<CONFIG:Debug>>:/Ox>
        $<$<CONFIG:Release>:/GL>
    )
    add_link_options(
        /DEBUG
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/MANIFEST:NO>
        $<IF:$<CONFIG:Release>,/INCREMENTAL:NO,/INCREMENTAL>
    )
endif()

# Linux flags
if (IPL_OS_LINUX)
    add_compile_options(-fPIC -g -Wl,--no-undefined)
    if (IPL_CPU_X86)
        add_compile_options(-m32 -mfpmath=sse -march=native)
    endif()
endif()

# macOS flags
if (IPL_OS_MACOS)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
endif()

# Android flags
if (IPL_OS_ANDROID)
    add_compile_options(-fPIC -g $<$<CONFIG:Release>:-O3>)
    add_link_options(-static-libstdc++)
endif()

# iOS flags
# todo

# WASM flags
if (IPL_OS_WASM)
    add_compile_options(-msimd128 -mfpu=neon)
    add_link_options(-sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1)

    # Enable 16 KB page size support. (https://developer.android.com/about/versions/15/behavior-changes-all#16-kb)
    add_link_options("-Wl,-z,max-page-size=16384")
endif()


#
# VERSION STAMPING
#

# Set version variables for template substitution
set(STEAMAUDIO_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(STEAMAUDIO_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(STEAMAUDIO_VERSION_PATCH ${PROJECT_VERSION_PATCH})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/steamaudio_fmodcore_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/steamaudio_fmodcore_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)


#
# DEPENDENCIES
#

find_package(FMOD REQUIRED)

find_package(SteamAudio REQUIRED)

include(SteamAudioHelpers)
if (STEAMAUDIOFMODCORE_BUILD_DOCS)
    get_bin_subdir(IPL_BIN_SUBDIR)
    set(Doxygen_ROOT ${CMAKE_HOME_DIRECTORY}/../core/deps/doxygen/bin/${IPL_BIN_SUBDIR})
    set(DOXYGEN_SKIP_DOT TRUE)
    find_package(Doxygen)
    find_package(Sphinx)
    if (NOT DOXYGEN_FOUND OR NOT Sphinx_FOUND)
        message(STATUS "Disabling docs")
        set(STEAMAUDIOFMODCORE_BUILD_DOCS OFF)
    endif()
endif()


#
# TARGETS
#

add_subdirectory(src)

if (STEAMAUDIOFMODCORE_BUILD_DOCS)
    add_subdirectory(doc)
endif()


#
# FMOD CORE AUDIO PLUGIN
#

# Core source files (shared between original and enhanced)
set(SRC_FMODCORE_CORE
    src/pch.h
    src/steamaudio_fmodcore_version.h.in
    src/library.h
    src/library.cpp
    src/steamaudio_fmodcore.h
    src/steamaudio_fmodcore.cpp
)

# Original implementation
set(SRC_FMODCORE_ORIGINAL
    src/spatialize_effect.cpp
    src/reverb_effect.cpp
    src/mix_return_effect.cpp
    src/plugin_list.cpp
)

# Enhanced implementation (if available)
set(SRC_FMODCORE_ENHANCED
    src/spatialize_effect_enhanced.cpp
    src/reverb_effect_enhanced.cpp
    src/mix_return_effect_enhanced.cpp
    src/steamaudio_fmodcore_enhanced.cpp
    src/plugin_list_enhanced.cpp
    src/performance_monitor.h
    src/error_handling.h
    src/error_handling.cpp
)

# Default to original implementation
set(SRC_FMODCORE
    ${SRC_FMODCORE_CORE}
    ${SRC_FMODCORE_ORIGINAL}
)

# Add enhanced files if building enhanced version
if(STEAMAUDIO_FMODCORE_BUILD_ENHANCED)
    # Check if enhanced files exist
    set(ENHANCED_FILES_EXIST TRUE)
    foreach(ENHANCED_FILE ${SRC_FMODCORE_ENHANCED})
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ENHANCED_FILE}")
            set(ENHANCED_FILES_EXIST FALSE)
            message(WARNING "Enhanced file not found: ${ENHANCED_FILE}")
        endif()
    endforeach()
    
    if(ENHANCED_FILES_EXIST)
        list(APPEND SRC_FMODCORE ${SRC_FMODCORE_ENHANCED})
        add_definitions(-DSTEAMAUDIO_FMODCORE_ENHANCED_BUILD=1)
        message(STATUS "Building enhanced FMOD Core plugin with error handling and performance monitoring")
    else()
        message(WARNING "Enhanced files not found, falling back to original implementation")
        set(STEAMAUDIO_FMODCORE_BUILD_ENHANCED OFF)
    endif()
endif()

if (IPL_OS_WINDOWS)
    list(APPEND SRC_FMODCORE src/phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)
endif()

if (BUILD_SHARED_LIBS AND NOT IPL_OS_IOS)
    add_library(phonon_fmodcore SHARED ${SRC_FMODCORE})
    if (IPL_OS_MACOS)
        add_library(phonon_fmodcore_bundle MODULE ${SRC_FMODCORE})
        set_target_properties(phonon_fmodcore_bundle PROPERTIES BUNDLE TRUE)
        set_target_properties(phonon_fmodcore_bundle PROPERTIES OUTPUT_NAME phonon_fmodcore)
    endif()
else()
    add_library(phonon_fmodcore STATIC ${SRC_FMODCORE})
endif()

# Include directories
target_include_directories(phonon_fmodcore PRIVATE 
    ${CMAKE_HOME_DIRECTORY}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmodcore_bundle PRIVATE 
        ${CMAKE_HOME_DIRECTORY}/include 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/unity
    )
endif()

# Generated headers
target_include_directories(phonon_fmodcore PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmodcore_bundle PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Platform-specific linking
if (IPL_OS_WINDOWS)
    target_link_libraries(phonon_fmodcore PRIVATE delayimp)
elseif (IPL_OS_LINUX)
    target_link_libraries(phonon_fmodcore PRIVATE m dl pthread)
elseif (IPL_OS_ANDROID)
    target_link_libraries(phonon_fmodcore PRIVATE log android)
endif()

# Link against FMOD Core and Steam Audio
target_link_libraries(phonon_fmodcore PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_link_libraries(phonon_fmodcore_bundle PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
endif()

# Precompiled headers
target_precompile_headers(phonon_fmodcore PRIVATE src/pch.h)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_precompile_headers(phonon_fmodcore_bundle PRIVATE src/pch.h)
endif()

# Debug symbols extraction on Linux
if (IPL_OS_LINUX AND BUILD_SHARED_LIBS AND (NOT IPL_CPU_ARMV8))
    add_custom_command(
        TARGET      phonon_fmodcore
        POST_BUILD
        COMMAND     objcopy --only-keep-debug $<TARGET_FILE_NAME:phonon_fmodcore> $<TARGET_FILE_NAME:phonon_fmodcore>.dbg
        COMMAND     strip --strip-debug --strip-unneeded $<TARGET_FILE_NAME:phonon_fmodcore>
        COMMAND     objcopy --add-gnu-debuglink=$<TARGET_FILE_NAME:phonon_fmodcore>.dbg $<TARGET_FILE_NAME:phonon_fmodcore>
        COMMENT     "Extracting debug symbols..."
    )
endif()

#
# POST-BUILD STEP FOR COPYING fmod-core -> unity
#

set(UNITY_FMODCORE_PLUGIN_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64)
    endif()
elseif (IPL_OS_MACOS)
    set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64-v8a)
    elseif (IPL_CPU_X86)
        set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86)
    endif()
elseif (IPL_OS_IOS)
    set(UNITY_FMODCORE_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/iOS)
endif()

if (NOT UNITY_FMODCORE_PLUGIN_DIR STREQUAL "")
    if (IPL_OS_MACOS)
        add_custom_target(phonon_fmodcore_bundle_copy_unity ALL
            COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:phonon_fmodcore_bundle> ${UNITY_FMODCORE_PLUGIN_DIR}/phonon_fmodcore.bundle
            VERBATIM
        )
        add_dependencies(phonon_fmodcore_bundle_copy_unity phonon_fmodcore_bundle)
    else()
        add_custom_command(
            TARGET      phonon_fmodcore
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_FMODCORE_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon_fmodcore> ${UNITY_FMODCORE_PLUGIN_DIR}
        )
    endif()
endif()

#
# INSTALL
#

get_bin_subdir(IPL_BIN_SUBDIR)

install(
    TARGETS             phonon_fmodcore
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
)

if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    install(
        TARGETS             phonon_fmodcore_bundle
        ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
        LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
        RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
    )
endif()

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/phonon_fmodcore.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_LINUX AND NOT IPL_CPU_ARMV8)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/libphonon_fmodcore.so.dbg
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
endif()

# Install Steam Audio dependencies
if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/phonon.dll
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_MACOS)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/libphonon.dylib
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
    install(
        DIRECTORY   ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/phonon.bundle
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_LINUX OR IPL_OS_ANDROID)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/lib/${IPL_BIN_SUBDIR}/libphonon.so
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_IOS)
    # todo: install libphonon.a?
endif()

#
# INSTALL UNITY INTEGRATION FILES
#

# Install Unity C# scripts
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/SteamAudioFMODCore.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/SteamAudioFMODCoreSource.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/SteamAudioFMODCoreManager.cs"
    DESTINATION "unity/Scripts/Runtime"
    COMPONENT fmod_core
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/Editor/SteamAudioFMODCoreEditor.cs"
    DESTINATION "unity/Scripts/Editor"
    COMPONENT fmod_core
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/Examples/SteamAudioFMODCoreExample.cs"
    DESTINATION "unity/Examples"
    COMPONENT fmod_core
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/unity/package.json"
    DESTINATION "unity"
    COMPONENT fmod_core
)

#
# BUILD CONFIGURATION SUMMARY
#

message(STATUS "Steam Audio FMOD Core Build Configuration:")
message(STATUS "  Enhanced Build: ${STEAMAUDIO_FMODCORE_BUILD_ENHANCED}")
message(STATUS "  Original Build: ${STEAMAUDIO_FMODCORE_BUILD_ORIGINAL}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Target Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Target Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Shared Libraries: ${BUILD_SHARED_LIBS}")

if(STEAMAUDIO_FMODCORE_BUILD_ENHANCED)
    message(STATUS "Enhanced Features:")
    message(STATUS "  - Comprehensive error handling with graceful degradation")
    message(STATUS "  - Real-time performance monitoring (zero overhead when disabled)")
    message(STATUS "  - Automatic resource management with leak detection")
    message(STATUS "  - Thread-safe operation with atomic state management")
    message(STATUS "  - Advanced parameter validation and recovery mechanisms")
    message(STATUS "  - Detailed logging and diagnostics for production debugging")
endif()

# Create alias targets for easier integration
add_library(SteamAudio::FMODCore ALIAS phonon_fmodcore)
if(STEAMAUDIO_FMODCORE_BUILD_ENHANCED)
    message(STATUS "Enhanced plugin components included in main build")
    message(STATUS "Use src/CMakeLists.txt for standalone enhanced build with additional options")
endif()


#
# PACKAGING
#

install(FILES ${CMAKE_HOME_DIRECTORY}/../core/THIRDPARTY.md DESTINATION root)

# Install main README for bin directory
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/bin/README.md" DESTINATION . COMPONENT fmod_core)

set(CPACK_PACKAGE_VENDOR "Valve Corporation")
set(CPACK_PACKAGE_FILE_NAME steamaudio_fmodcore)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_HOME_DIRECTORY}/dist)
set(CPACK_INSTALL_CMAKE_PROJECTS "")
set(CPACK_INSTALLED_DIRECTORIES
    ${CMAKE_INSTALL_PREFIX}/lib/windows-x86 lib/windows-x86
    ${CMAKE_INSTALL_PREFIX}/lib/windows-x64 lib/windows-x64
    ${CMAKE_INSTALL_PREFIX}/lib/linux-x86 lib/linux-x86
    ${CMAKE_INSTALL_PREFIX}/lib/linux-x64 lib/linux-x64
    ${CMAKE_INSTALL_PREFIX}/lib/osx lib/osx
    ${CMAKE_INSTALL_PREFIX}/lib/android-armv7 lib/android-armv7
    ${CMAKE_INSTALL_PREFIX}/lib/android-armv8 lib/android-armv8
    ${CMAKE_INSTALL_PREFIX}/lib/android-x86 lib/android-x86
    ${CMAKE_INSTALL_PREFIX}/lib/android-x64 lib/android-x64
    ${CMAKE_INSTALL_PREFIX}/lib/ios lib/ios
    ${CMAKE_INSTALL_PREFIX}/symbols symbols
    ${CMAKE_INSTALL_PREFIX}/src src
    ${CMAKE_INSTALL_PREFIX}/doc doc
    ${CMAKE_INSTALL_PREFIX}/root .
)

include(CPack)