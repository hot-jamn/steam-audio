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

include(FindPackageHandleStandardArgs)
include(SteamAudioHelpers)

get_local_lib_path(IPL_LIB_PATH)

find_path(FMOD_INCLUDE_DIR
	NAMES 			fmod.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/include
	PATH_SUFFIXES 	fmod/
)

# FMOD Core API is header-only for DSP plugins, so library is optional
find_library(FMOD_LIBRARY
	NAMES fmod_vc fmod fmodL_vc fmodL
	PATHS ${IPL_LIB_PATH}
)

# For FMOD Core DSP plugins, we only need headers
find_package_handle_standard_args(FMOD
	FOUND_VAR 		FMOD_FOUND
	REQUIRED_VARS 	FMOD_INCLUDE_DIR
)

if (FMOD_FOUND)
	if (NOT TARGET FMOD::FMOD)
		add_library(FMOD INTERFACE)
		target_include_directories(FMOD INTERFACE ${FMOD_INCLUDE_DIR})
		
		# Link library if found (for runtime loading scenarios)
		if(FMOD_LIBRARY)
			target_link_libraries(FMOD INTERFACE ${FMOD_LIBRARY})
		endif()

		add_library(FMOD::FMOD ALIAS FMOD)
	endif()
endif()

mark_as_advanced(FMOD_INCLUDE_DIR FMOD_LIBRARY)
