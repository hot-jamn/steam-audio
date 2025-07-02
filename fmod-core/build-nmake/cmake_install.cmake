# Install script for directory: D:/hot-jamn/steam-audio/fmod-core

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("D:/hot-jamn/steam-audio/fmod-core/build-nmake/src/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/windows-x64" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/phonon_fmodcore.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/windows-x64" TYPE SHARED_LIBRARY FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/phonon_fmodcore.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/symbols/windows-x64" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/build-nmake/Release/phonon_fmodcore.pdb")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/windows-x64" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/lib/windows-x64/phonon.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/unity/Scripts/Runtime" TYPE FILE FILES
    "D:/hot-jamn/steam-audio/fmod-core/unity/SteamAudioFMODCore.cs"
    "D:/hot-jamn/steam-audio/fmod-core/unity/SteamAudioFMODCoreSource.cs"
    "D:/hot-jamn/steam-audio/fmod-core/unity/SteamAudioFMODCoreManager.cs"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/unity/Scripts/Editor" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/unity/Editor/SteamAudioFMODCoreEditor.cs")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/unity/Examples" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/unity/Examples/SteamAudioFMODCoreExample.cs")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/unity" TYPE FILE FILES
    "D:/hot-jamn/steam-audio/fmod-core/unity/README.md"
    "D:/hot-jamn/steam-audio/fmod-core/unity/package.json"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/root" TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/../core/THIRDPARTY.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "fmod_core" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/hot-jamn/steam-audio/fmod-core/bin/README.md")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/hot-jamn/steam-audio/fmod-core/build-nmake/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/hot-jamn/steam-audio/fmod-core/build-nmake/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
