# Install script for directory: D:/hot-jamn/steam-audio/fmod-core/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/hot-jamn/steam-audio/fmod-core/bin")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/windows-x64" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/src/phonon_fmodcore_enhanced.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core_enhanced" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/windows-x64" TYPE SHARED_LIBRARY FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/src/phonon_fmodcore_enhanced.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core_enhanced" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/symbols/windows-x64" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/src/Release/phonon_fmodcore_enhanced.pdb")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core_enhanced_dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/SteamAudio/fmod-core" TYPE FILE FILES
    "D:/hot-jamn/steam-audio/fmod-core/src/performance_monitor.h"
    "D:/hot-jamn/steam-audio/fmod-core/src/error_handling.h"
    "D:/hot-jamn/steam-audio/fmod-core/src/steamaudio_fmodcore.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core_enhanced_docs" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/docs/fmod-core" TYPE FILE FILES
    "D:/hot-jamn/steam-audio/fmod-core/src/code_quality_analysis.md"
    "D:/hot-jamn/steam-audio/fmod-core/src/../docs/enhanced_implementation_guide.md"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/hot-jamn/steam-audio/fmod-core/build-nmake/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
