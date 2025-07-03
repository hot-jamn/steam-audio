//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "pch.h"
#include "steamaudio_fmodcore.h"

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Plugin Initialization
// --------------------------------------------------------------------------------------------------------------------

void initializeParameterDescriptions()
{
    // Initialize parameter descriptions for all effects
    initSpatializeParameterDescs();
    initReverbParameterDescs();
    initMixerReturnParameterDescs();
}

// --------------------------------------------------------------------------------------------------------------------
// Plugin List
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

// Forward declarations of DSP description getters
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_FMODCore_Spatialize_GetDSPDescription();
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_FMODCore_Reverb_GetDSPDescription();
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_FMODCore_MixerReturn_GetDSPDescription();

// Plugin list - this is what FMOD will call to get all available plugins
F_EXPORT FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList()
{
    static FMOD_DSP_DESCRIPTION* gPluginList[] = {
        FMOD_SteamAudio_FMODCore_Spatialize_GetDSPDescription(),
        FMOD_SteamAudio_FMODCore_Reverb_GetDSPDescription(),
        FMOD_SteamAudio_FMODCore_MixerReturn_GetDSPDescription(),
        nullptr
    };

    static FMOD_PLUGINLIST gPluginListContainer = {
        FMOD_PLUGINTYPE_DSP,
        gPluginList
    };

    // Initialize parameter descriptions on first call
    static bool initialized = false;
    if (!initialized)
    {
        initializeParameterDescriptions();
        initialized = true;
    }

    return &gPluginListContainer;
}

// Alternative entry point for individual plugin loading
F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetDSPDescription()
{
    // Return the spatializer as the default plugin
    return FMOD_SteamAudio_FMODCore_Spatialize_GetDSPDescription();
}

// Plugin information
F_EXPORT unsigned int F_CALL FMODGetNumPlugins()
{
    return 3; // Spatializer, Reverb, MixerReturn
}

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetPluginDescription(unsigned int index)
{
    switch (index)
    {
    case 0:
        return FMOD_SteamAudio_FMODCore_Spatialize_GetDSPDescription();
    case 1:
        return FMOD_SteamAudio_FMODCore_Reverb_GetDSPDescription();
    case 2:
        return FMOD_SteamAudio_FMODCore_MixerReturn_GetDSPDescription();
    default:
        return nullptr;
    }
}

// Plugin version information
F_EXPORT unsigned int F_CALL FMODGetPluginVersion()
{
    return 0x00020229;
}

// Plugin name
F_EXPORT const char* F_CALL FMODGetPluginName()
{
    return "Steam Audio FMOD Core";
}

}

}