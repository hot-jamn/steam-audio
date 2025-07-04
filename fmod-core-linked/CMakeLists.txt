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

project(PhononFMOD VERSION 4.6.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build")

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
set(BUILD_SHARED_LIBS ON)

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
        /std:c++20 
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

#
# DEPENDENCIES
#
find_package(FMOD REQUIRED)
find_package(SteamAudio REQUIRED)
include(SteamAudioHelpers)

#
# TARGETS
#
add_subdirectory(src)