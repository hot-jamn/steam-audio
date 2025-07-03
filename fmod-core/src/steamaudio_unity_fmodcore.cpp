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

#include "../src/pch.h"
#include "steamaudio_unity_fmodcore.h"
#include "../src/steamaudio_fmodcore.h"

// Define missing Unity constants if not available
#ifndef UNITY_AUDIO_PARAM_FLAG_META
#define UNITY_AUDIO_PARAM_FLAG_META 0x01
#endif

#ifndef UNITY_AUDIO_PARAM_FLAG_READONLY
#define UNITY_AUDIO_PARAM_FLAG_READONLY 0x02
#endif

#ifndef UNITY_AUDIO_EFFECT_FLAG_PASSTHROUGH
#define UNITY_AUDIO_EFFECT_FLAG_PASSTHROUGH 0x01
#endif

namespace SteamAudioUnityFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Global State
// --------------------------------------------------------------------------------------------------------------------

static std::atomic<bool> gInitialized{ false };
static std::atomic<IPLContext> gUnitySharedContext{ nullptr };
static std::mutex gContextMutex;

// --------------------------------------------------------------------------------------------------------------------
// Unity Native Audio Plugin Callbacks
// --------------------------------------------------------------------------------------------------------------------

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
{
    if (!state)
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;

    auto* contextState = new ContextSharingState();
    contextState->enabled = false;
    contextState->contextShared = false;
    contextState->sharedContext = nullptr;

    state->effectdata = contextState;
    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
{
    if (!state || !state->effectdata)
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;

    auto* contextState = static_cast<ContextSharingState*>(state->effectdata);
    delete contextState;
    state->effectdata = nullptr;

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state,
                                                               float* inBuffer,
                                                               float* outBuffer,
                                                               unsigned int length,
                                                               int inChannels,
                                                               int outChannels)
{
    if (!state || !state->effectdata)
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;

    auto* contextState = static_cast<ContextSharingState*>(state->effectdata);

    // If context sharing is enabled and we have a Unity context, share it with FMOD Core
    if (contextState->enabled && gUnitySharedContext.load())
    {
        if (!contextState->contextShared)
        {
            SteamAudioFMODCore::setSharedContext(gUnitySharedContext.load());
            contextState->contextShared = true;
            contextState->sharedContext = gUnitySharedContext.load();
        }
    }

    // Pass through audio unchanged (this is just a context sharing bridge)
    if (inBuffer != outBuffer)
    {
        memcpy(outBuffer, inBuffer, length * inChannels * sizeof(float));
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state,
                                                                         int index,
                                                                         float value)
{
    if (!state || !state->effectdata)
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;

    auto* contextState = static_cast<ContextSharingState*>(state->effectdata);

    switch (index)
    {
    case CONTEXT_SHARING_ENABLED:
        contextState->enabled = (value > 0.5f);
        break;
    default:
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state,
                                                                         int index,
                                                                         float* value,
                                                                         char* valuestr)
{
    if (!state || !state->effectdata || !value)
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;

    auto* contextState = static_cast<ContextSharingState*>(state->effectdata);

    switch (index)
    {
    case CONTEXT_SHARING_ENABLED:
        *value = contextState->enabled ? 1.0f : 0.0f;
        if (valuestr)
            strcpy(valuestr, contextState->enabled ? "Enabled" : "Disabled");
        break;
    case CONTEXT_SHARING_STATUS:
        *value = contextState->contextShared ? 1.0f : 0.0f;
        if (valuestr)
            strcpy(valuestr, contextState->contextShared ? "Active" : "Inactive");
        break;
    default:
        return UNITY_AUDIODSP_ERR_UNSUPPORTED;
    }

    return UNITY_AUDIODSP_OK;
}

UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state,
                                                                      const char* name,
                                                                      float* buffer,
                                                                      int numsamples)
{
    // Not used for context sharing
    return UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

// --------------------------------------------------------------------------------------------------------------------
// Unity Audio Effect Definition
// --------------------------------------------------------------------------------------------------------------------

UnityAudioParameterDefinition gContextSharingParameters[] =
{
    {
        "Context Enabled",  // Shortened to fit 16 char limit
        "",
        "Enable sharing of Unity Steam Audio context with FMOD Core",
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f
    },
    {
        "Context Status",   // Shortened to fit 16 char limit
        "",
        "Status of context sharing (read-only)",
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f
    }
};

UnityAudioEffectDefinition gContextSharingEffectDefinition =
{
    sizeof(UnityAudioEffectDefinition),                    // structsize
    sizeof(UnityAudioParameterDefinition),                // paramstructsize
    UNITY_AUDIO_PLUGIN_API_VERSION,                       // apiversion
    UNITY_AUDIO_PLUGIN_API_VERSION,                       // pluginversion
    0,                                                     // channels (0 = process any number)
    CONTEXT_SHARING_NUM_PARAMS,                           // numparameters
    UNITY_AUDIO_EFFECT_FLAG_PASSTHROUGH,                  // flags
    "SA FMOD Context",                                     // name (shortened to fit 32 char limit)
    CreateCallback,                                        // create
    ReleaseCallback,                                       // release
    nullptr,                                               // reset
    ProcessCallback,                                       // process
    nullptr,                                               // setposition
    gContextSharingParameters,                             // paramdefs
    SetFloatParameterCallback,                             // setfloatparameter
    GetFloatParameterCallback,                             // getfloatparameter
    GetFloatBufferCallback                                 // getfloatbuffer
};

// --------------------------------------------------------------------------------------------------------------------
// Context Sharing API Implementation
// --------------------------------------------------------------------------------------------------------------------

void initializeUnityFMODCoreContextSharing()
{
    std::lock_guard<std::mutex> lock(gContextMutex);

    if (!gInitialized.load())
    {
        gUnitySharedContext.store(nullptr);
        gInitialized.store(true);
    }
}

void shutdownUnityFMODCoreContextSharing()
{
    std::lock_guard<std::mutex> lock(gContextMutex);

    if (gInitialized.load())
    {
        gUnitySharedContext.store(nullptr);
        gInitialized.store(false);
    }
}

void shareUnityContextWithFMODCore(IPLContext context)
{
    std::lock_guard<std::mutex> lock(gContextMutex);

    gUnitySharedContext.store(context);

    // Also directly set it in the FMOD Core integration
    SteamAudioFMODCore::setSharedContext(context);
}

IPLContext getSharedUnityContext()
{
    return gUnitySharedContext.load();
}

bool isContextSharingActive()
{
    return gUnitySharedContext.load() != nullptr;
}

}

// --------------------------------------------------------------------------------------------------------------------
// Unity Plugin Export
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitions)
{
    using namespace SteamAudioUnityFMODCore;

    // Initialize context sharing system
    initializeUnityFMODCoreContextSharing();

    static UnityAudioEffectDefinition* effectDefinitions[] = {
        &gContextSharingEffectDefinition
    };

    *definitions = effectDefinitions;
    return sizeof(effectDefinitions) / sizeof(effectDefinitions[0]);
}

// --------------------------------------------------------------------------------------------------------------------
// Unity C# Integration API
// --------------------------------------------------------------------------------------------------------------------

UNITY_AUDIODSP_EXPORT_API void iplUnitySetContext(IPLContext context)
{
    SteamAudioUnityFMODCore::shareUnityContextWithFMODCore(context);
}

UNITY_AUDIODSP_EXPORT_API void iplUnitySetHRTF(IPLHRTF hrtf)
{
    // HRTF is typically managed by the main Steam Audio context
    // This is a placeholder for future HRTF sharing functionality
    // For now, HRTF sharing happens through the shared context
}

UNITY_AUDIODSP_EXPORT_API void iplUnitySetSimulationSettings(IPLSimulationSettings simulationSettings)
{
    // Simulation settings are typically managed by the main Steam Audio context
    // This is a placeholder for future simulation settings sharing functionality
    // For now, settings sharing happens through the shared context
}

UNITY_AUDIODSP_EXPORT_API int iplUnityAddSource(IPLSource source)
{
    // Source management is typically handled by the main Steam Audio system
    // This is a placeholder for future source sharing functionality
    // Return a dummy source ID for now
    return 0;
}

UNITY_AUDIODSP_EXPORT_API void iplUnityRemoveSource(int sourceId)
{
    // Source management is typically handled by the main Steam Audio system
    // This is a placeholder for future source sharing functionality
}

}