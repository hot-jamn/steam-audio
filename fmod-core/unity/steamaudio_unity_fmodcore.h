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

#pragma once

#include <phonon.h>

// Unity Native Audio Plugin Interface
#include "unity5/AudioPluginInterface.h"

namespace SteamAudioUnityFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Unity Native Audio Plugin Interface
// --------------------------------------------------------------------------------------------------------------------

// Unity native audio effect definition for FMOD Core context sharing
extern UnityAudioEffectDefinition gContextSharingEffectDefinition;

// Unity native audio effect callbacks
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, 
                                                               float* inBuffer, 
                                                               float* outBuffer, 
                                                               unsigned int length, 
                                                               int inChannels, 
                                                               int outChannels);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, 
                                                                         int index, 
                                                                         float value);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, 
                                                                         int index, 
                                                                         float* value, 
                                                                         char* valuestr);
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, 
                                                                      const char* name, 
                                                                      float* buffer, 
                                                                      int numsamples);

// Context sharing parameters
enum ContextSharingParams
{
    CONTEXT_SHARING_ENABLED,        // Enable/disable context sharing
    CONTEXT_SHARING_STATUS,         // Read-only status of context sharing
    CONTEXT_SHARING_NUM_PARAMS
};

// Context sharing state
struct ContextSharingState
{
    bool enabled;
    bool contextShared;
    IPLContext sharedContext;
};

// --------------------------------------------------------------------------------------------------------------------
// Context Sharing API
// --------------------------------------------------------------------------------------------------------------------

/**
 *  Initializes the Unity-FMOD Core context sharing system.
 */
void initializeUnityFMODCoreContextSharing();

/**
 *  Shuts down the Unity-FMOD Core context sharing system.
 */
void shutdownUnityFMODCoreContextSharing();

/**
 *  Shares the Unity Steam Audio context with FMOD Core.
 *
 *  \param  context  The Unity Steam Audio context to share.
 */
void shareUnityContextWithFMODCore(IPLContext context);

/**
 *  Gets the shared Unity context for use by FMOD Core.
 *
 *  \return  The shared Unity context, or nullptr if none is available.
 */
IPLContext getSharedUnityContext();

/**
 *  Checks if context sharing is currently active.
 *
 *  \return  True if context sharing is active, false otherwise.
 */
bool isContextSharingActive();

}

// --------------------------------------------------------------------------------------------------------------------
// Unity Plugin Export
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

/**
 *  Unity plugin entry point. This function is called by Unity when the plugin is loaded.
 *
 *  \param  definitions  Array to fill with effect definitions.
 *  \return              Number of effects defined by this plugin.
 */
UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitions);

}