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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#
# VERSION STAMPING
#
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/steamaudio_fmodcore_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/steamaudio_fmodcore_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)

#
# FMOD CORE AUDIO PLUGIN
#
set(SRC_FMODCORE
    steamaudio_fmodcore.cpp
    spatialize_effect.cpp
    reverb_effect.cpp
    mix_return_effect.cpp
    plugin_list.cpp
)

if (WIN32)
    list(APPEND SRC_FMODCORE phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)
endif()

add_library(phonon_fmod SHARED ${SRC_FMODCORE})

target_include_directories(phonon_fmod PRIVATE
    ${CMAKE_HOME_DIRECTORY}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(phonon_fmod PRIVATE FMOD::FMOD SteamAudio::SteamAudio)

target_precompile_headers(phonon_fmod PRIVATE pch.h)

#
# POST-BUILD STEP FOR COPYING fmod-core-linked -> unity
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
    add_custom_command(
        TARGET      phonon_fmod
        POST_BUILD
        COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_FMODCORE_PLUGIN_DIR}
        COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon_fmod> ${UNITY_FMODCORE_PLUGIN_DIR}
    )
endif()

#
# INSTALL
#

get_bin_subdir(IPL_BIN_SUBDIR)

install(
    TARGETS             phonon_fmod
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
)

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/phonon_fmodcore.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
endif()
