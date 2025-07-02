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

include(SteamAudioHelpers)

#
# VERSION STAMPING
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/steamaudio_fmodcore_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/steamaudio_fmodcore_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)

#
# ENHANCED FMOD CORE AUDIO PLUGIN
#

# Core source files
set(SRC_FMODCORE_CORE
    pch.h
    steamaudio_fmodcore_version.h.in
    library.h
    library.cpp
    steamaudio_fmodcore.h
    steamaudio_fmodcore.cpp
    ../unity/steamaudio_unity_fmodcore.h
    ../unity/steamaudio_unity_fmodcore.cpp
)

# Original effect implementations
set(SRC_FMODCORE_ORIGINAL
    spatialize_effect.cpp
    reverb_effect.cpp
    mix_return_effect.cpp
    plugin_list.cpp
)

# Enhanced effect implementations with comprehensive error handling and performance monitoring
set(SRC_FMODCORE_ENHANCED
    spatialize_effect_enhanced.cpp
    reverb_effect_enhanced.cpp
    mix_return_effect_enhanced.cpp
    steamaudio_fmodcore_enhanced.cpp
    plugin_list_enhanced.cpp
    error_handling.cpp
)

# Performance monitoring and error handling infrastructure
set(SRC_FMODCORE_INFRASTRUCTURE
    performance_monitor.h
    error_handling.h
)

# Testing framework (optional, for development builds)
set(SRC_FMODCORE_TESTING
    test_framework.h
)

# Documentation
set(SRC_FMODCORE_DOCS
    code_quality_analysis.md
)

# Combine source files based on build configuration
set(SRC_FMODCORE ${SRC_FMODCORE_CORE})

# Add original files if building original version
if(STEAMAUDIO_FMODCORE_BUILD_ORIGINAL)
    list(APPEND SRC_FMODCORE ${SRC_FMODCORE_ORIGINAL})
endif()

# Add enhanced files if building enhanced version
if(STEAMAUDIO_FMODCORE_BUILD_ENHANCED)
    list(APPEND SRC_FMODCORE ${SRC_FMODCORE_ENHANCED})
    list(APPEND SRC_FMODCORE ${SRC_FMODCORE_INFRASTRUCTURE})
endif()

# Default to original if nothing is selected
if(NOT STEAMAUDIO_FMODCORE_BUILD_ORIGINAL AND NOT STEAMAUDIO_FMODCORE_BUILD_ENHANCED)
    list(APPEND SRC_FMODCORE ${SRC_FMODCORE_ORIGINAL})
    set(STEAMAUDIO_FMODCORE_BUILD_ORIGINAL ON)
    message(STATUS "No implementation selected, defaulting to original")
endif()

# Add testing framework in debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    list(APPEND SRC_FMODCORE ${SRC_FMODCORE_TESTING})
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_TESTING=1)
endif()

# Add Windows resource file
if (IPL_OS_WINDOWS)
    list(APPEND SRC_FMODCORE phonon_fmodcore.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_fmodcore.rc)
endif()

#
# BUILD CONFIGURATION OPTIONS
#

# Enhanced features configuration
option(STEAMAUDIO_FMODCORE_ENABLE_ENHANCED "Enable enhanced effects with error handling and performance monitoring" ON)
option(STEAMAUDIO_FMODCORE_ENABLE_PROFILING "Enable performance profiling (can be toggled at runtime)" ON)
option(STEAMAUDIO_FMODCORE_ENABLE_ERROR_RECOVERY "Enable automatic error recovery mechanisms" ON)
option(STEAMAUDIO_FMODCORE_ENABLE_RESOURCE_TRACKING "Enable resource leak detection and tracking" ON)
option(STEAMAUDIO_FMODCORE_ENABLE_THREAD_SAFETY "Enable thread-safe operations with atomic state management" ON)

# Performance and debugging options
option(STEAMAUDIO_FMODCORE_ENABLE_DETAILED_LOGGING "Enable detailed logging for production debugging" OFF)
option(STEAMAUDIO_FMODCORE_ENABLE_MEMORY_VALIDATION "Enable memory validation and bounds checking" OFF)
option(STEAMAUDIO_FMODCORE_ENABLE_PARAMETER_VALIDATION "Enable comprehensive parameter validation" ON)

# Configure preprocessor definitions based on options
if(STEAMAUDIO_FMODCORE_ENABLE_ENHANCED)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_ENHANCED=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_PROFILING)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_PROFILING=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_ERROR_RECOVERY)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_ERROR_RECOVERY=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_RESOURCE_TRACKING)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_RESOURCE_TRACKING=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_THREAD_SAFETY)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_THREAD_SAFETY=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_DETAILED_LOGGING)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_DETAILED_LOGGING=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_MEMORY_VALIDATION)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_MEMORY_VALIDATION=1)
endif()

if(STEAMAUDIO_FMODCORE_ENABLE_PARAMETER_VALIDATION)
    add_definitions(-DSTEAMAUDIO_FMODCORE_ENABLE_PARAMETER_VALIDATION=1)
endif()

#
# LIBRARY TARGETS
#

# Create main library target
if (BUILD_SHARED_LIBS AND NOT IPL_OS_IOS)
    add_library(phonon_fmodcore_enhanced SHARED ${SRC_FMODCORE})
    if (IPL_OS_MACOS)
        add_library(phonon_fmodcore_enhanced_bundle MODULE ${SRC_FMODCORE})
        set_target_properties(phonon_fmodcore_enhanced_bundle PROPERTIES BUNDLE TRUE)
        set_target_properties(phonon_fmodcore_enhanced_bundle PROPERTIES OUTPUT_NAME phonon_fmodcore_enhanced)
    endif()
else()
    add_library(phonon_fmodcore_enhanced STATIC ${SRC_FMODCORE})
endif()

# Set target properties
set_target_properties(phonon_fmodcore_enhanced PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    set_target_properties(phonon_fmodcore_enhanced_bundle PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()

#
# INCLUDE DIRECTORIES
#

# Include directories for headers
target_include_directories(phonon_fmodcore_enhanced PRIVATE 
    ${CMAKE_HOME_DIRECTORY}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../unity
)

if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmodcore_enhanced_bundle PRIVATE 
        ${CMAKE_HOME_DIRECTORY}/include 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../unity
    )
endif()

# Include generated headers
target_include_directories(phonon_fmodcore_enhanced PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_include_directories(phonon_fmodcore_enhanced_bundle PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

#
# COMPILER-SPECIFIC SETTINGS
#

# Enable enhanced warnings and optimizations
if(MSVC)
    target_compile_options(phonon_fmodcore_enhanced PRIVATE
        /W4                     # High warning level
        /WX                     # Treat warnings as errors
        /permissive-           # Disable non-conforming code
        /Zc:__cplusplus        # Enable correct __cplusplus macro
        /MP                     # Multi-processor compilation
        /wd4201                 # Disable warning C4201: nonstandard extension used: nameless struct/union (Unity headers)
        /wd4100                 # Disable warning C4100: unreferenced parameter (Unity interface parameters)
    )
    
    # Enhanced optimizations for release builds
    target_compile_options(phonon_fmodcore_enhanced PRIVATE 
        $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Ob1 /Zi>
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(phonon_fmodcore_enhanced PRIVATE 
        -Wall -Wextra -Wpedantic  # Enhanced warnings
        -Werror                   # Treat warnings as errors
        -fno-exceptions          # Disable exceptions for audio processing
        -ffast-math              # Enable fast math optimizations
    )
    
    # Enhanced optimizations for release builds
    target_compile_options(phonon_fmodcore_enhanced PRIVATE 
        $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Debug>:-O0 -g -fsanitize=address>
    )
    
    # Link sanitizers in debug builds
    target_link_options(phonon_fmodcore_enhanced PRIVATE 
        $<$<CONFIG:Debug>:-fsanitize=address>
    )
endif()

# Apply same settings to bundle target on macOS
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    if(MSVC)
        target_compile_options(phonon_fmodcore_enhanced_bundle PRIVATE
            /W4 /WX /permissive- /Zc:__cplusplus /MP /wd4201 /wd4100
            $<$<CONFIG:Release>:/O2 /Ob2 /DNDEBUG>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Ob1 /Zi>
            $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(phonon_fmodcore_enhanced_bundle PRIVATE 
            -Wall -Wextra -Wpedantic -Werror -fno-exceptions -ffast-math
            $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
            $<$<CONFIG:RelWithDebInfo>:-O2 -g>
            $<$<CONFIG:Debug>:-O0 -g -fsanitize=address>
        )
        target_link_options(phonon_fmodcore_enhanced_bundle PRIVATE 
            $<$<CONFIG:Debug>:-fsanitize=address>
        )
    endif()
endif()

#
# PLATFORM-SPECIFIC LINKING
#

if (IPL_OS_WINDOWS)
    target_link_libraries(phonon_fmodcore_enhanced PRIVATE delayimp)
elseif (IPL_OS_LINUX)
    target_link_libraries(phonon_fmodcore_enhanced PRIVATE m dl pthread)
elseif (IPL_OS_ANDROID)
    target_link_libraries(phonon_fmodcore_enhanced PRIVATE log android)
endif()

# Link against FMOD Core and Steam Audio
target_link_libraries(phonon_fmodcore_enhanced PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_link_libraries(phonon_fmodcore_enhanced_bundle PRIVATE FMOD::FMOD SteamAudio::SteamAudio)
endif()

#
# PRECOMPILED HEADERS
#

target_precompile_headers(phonon_fmodcore_enhanced PRIVATE pch.h)
if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    target_precompile_headers(phonon_fmodcore_enhanced_bundle PRIVATE pch.h)
endif()

#
# DEBUG SYMBOLS EXTRACTION
#

if (IPL_OS_LINUX AND BUILD_SHARED_LIBS AND (NOT IPL_CPU_ARMV8))
    add_custom_command(
        TARGET      phonon_fmodcore_enhanced
        POST_BUILD
        COMMAND     objcopy --only-keep-debug $<TARGET_FILE_NAME:phonon_fmodcore_enhanced> $<TARGET_FILE_NAME:phonon_fmodcore_enhanced>.dbg
        COMMAND     strip --strip-debug --strip-unneeded $<TARGET_FILE_NAME:phonon_fmodcore_enhanced>
        COMMAND     objcopy --add-gnu-debuglink=$<TARGET_FILE_NAME:phonon_fmodcore_enhanced>.dbg $<TARGET_FILE_NAME:phonon_fmodcore_enhanced>
        COMMENT     "Extracting debug symbols for enhanced FMOD Core plugin..."
    )
endif()

#
# TESTING TARGETS (Debug builds only)
#

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # Create test executable for enhanced components
    add_executable(phonon_fmodcore_enhanced_tests
        test_framework.h
        # Add test source files here when created
    )
    
    target_link_libraries(phonon_fmodcore_enhanced_tests PRIVATE 
        phonon_fmodcore_enhanced
        FMOD::FMOD 
        SteamAudio::SteamAudio
    )
    
    target_include_directories(phonon_fmodcore_enhanced_tests PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Add test target
    enable_testing()
    add_test(NAME enhanced_fmodcore_tests COMMAND phonon_fmodcore_enhanced_tests)
endif()

#
# INSTALLATION RULES
#

get_bin_subdir(IPL_BIN_SUBDIR)

# Install enhanced library
install(
    TARGETS             phonon_fmodcore_enhanced
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
    COMPONENT           fmod_core_enhanced
)

if (BUILD_SHARED_LIBS AND IPL_OS_MACOS)
    install(
        TARGETS             phonon_fmodcore_enhanced_bundle
        ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
        LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
        RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
        COMPONENT           fmod_core_enhanced
    )
endif()

# Install debug symbols
if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/phonon_fmodcore_enhanced.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
        COMPONENT   fmod_core_enhanced
    )
elseif (IPL_OS_LINUX AND NOT IPL_CPU_ARMV8)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/libphonon_fmodcore_enhanced.so.dbg
        DESTINATION symbols/${IPL_BIN_SUBDIR}
        COMPONENT   fmod_core_enhanced
    )
endif()

# Install enhanced headers for development
install(
    FILES
        performance_monitor.h
        error_handling.h
        steamaudio_fmodcore.h
    DESTINATION include/SteamAudio/fmod-core
    COMPONENT   fmod_core_enhanced_dev
)

# Install Unity integration artifacts
install(
    FILES
        ../unity/steamaudio_unity_fmodcore.h
        ../unity/steamaudio_unity_fmodcore.cpp
    DESTINATION unity/Native
    COMPONENT   fmod_core_unity
)

# Install Unity C# integration files
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/SteamAudioFMODCore.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/SteamAudioFMODCoreSource.cs"
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/SteamAudioFMODCoreManager.cs"
    DESTINATION "unity/Scripts/Runtime"
    COMPONENT fmod_core_unity
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/Editor/SteamAudioFMODCoreEditor.cs"
    DESTINATION "unity/Scripts/Editor"
    COMPONENT fmod_core_unity
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/Examples/SteamAudioFMODCoreExample.cs"
    DESTINATION "unity/Examples"
    COMPONENT fmod_core_unity
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../unity/package.json"
    DESTINATION "unity"
    COMPONENT fmod_core_unity
)

# Install documentation
install(
    FILES       
        code_quality_analysis.md
        ../docs/enhanced_implementation_guide.md
    DESTINATION docs/fmod-core
    COMPONENT   fmod_core_enhanced_docs
)

#
# SUMMARY OUTPUT
#

message(STATUS "Steam Audio FMOD Core Enhanced Configuration:")
message(STATUS "  Enhanced Effects: ${STEAMAUDIO_FMODCORE_ENABLE_ENHANCED}")
message(STATUS "  Performance Profiling: ${STEAMAUDIO_FMODCORE_ENABLE_PROFILING}")
message(STATUS "  Error Recovery: ${STEAMAUDIO_FMODCORE_ENABLE_ERROR_RECOVERY}")
message(STATUS "  Resource Tracking: ${STEAMAUDIO_FMODCORE_ENABLE_RESOURCE_TRACKING}")
message(STATUS "  Thread Safety: ${STEAMAUDIO_FMODCORE_ENABLE_THREAD_SAFETY}")
message(STATUS "  Detailed Logging: ${STEAMAUDIO_FMODCORE_ENABLE_DETAILED_LOGGING}")
message(STATUS "  Memory Validation: ${STEAMAUDIO_FMODCORE_ENABLE_MEMORY_VALIDATION}")
message(STATUS "  Parameter Validation: ${STEAMAUDIO_FMODCORE_ENABLE_PARAMETER_VALIDATION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Target Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Target Architecture: ${CMAKE_SYSTEM_PROCESSOR}")

# Create alias target for easier integration
add_library(SteamAudio::FMODCoreEnhanced ALIAS phonon_fmodcore_enhanced)