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

    
// --------------------------------------------------------------------------------------------------------------------
// Global State
// --------------------------------------------------------------------------------------------------------------------

IPLContext gContext = nullptr;
IPLHRTF gHRTF[2] = { nullptr, nullptr };
IPLSimulationSettings gSimulationSettings{};
IPLSource gReverbSource[2] = { nullptr, nullptr };
IPLReflectionMixer gReflectionMixer[2] = { nullptr, nullptr };

std::atomic<bool> gNewHRTFWritten{ false };
std::atomic<bool> gIsSimulationSettingsValid{ false };
std::atomic<bool> gNewReverbSourceWritten{ false };
std::atomic<bool> gNewReflectionMixerWritten{ false };
std::atomic<bool> gHRTFDisabled{ false };

// Context sharing with Unity
std::atomic<IPLContext> gSharedContext{ nullptr };
std::atomic<bool> gContextShared{ false };

// Global source manager
SourceManager gSourceManager;

// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

IPLSpeakerLayout speakerLayoutForNumChannels(int numChannels)
{
    IPLSpeakerLayout layout{};
    switch (numChannels)
    {
    case 1:
        layout.type = IPL_SPEAKERLAYOUTTYPE_MONO;
        layout.numSpeakers = 1;
        layout.speakers = nullptr;
        break;
    case 2:
        layout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        layout.numSpeakers = 2;
        layout.speakers = nullptr;
        break;
    case 4:
        layout.type = IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC;
        layout.numSpeakers = 4;
        layout.speakers = nullptr;
        break;
    case 6:
        layout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1;
        layout.numSpeakers = 6;
        layout.speakers = nullptr;
        break;
    case 8:
        layout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1;
        layout.numSpeakers = 8;
        layout.speakers = nullptr;
        break;
    default:
        layout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        layout.numSpeakers = 2;
        layout.speakers = nullptr;
        break;
    }
    return layout;
}

int orderForNumChannels(int numChannels)
{
    if (numChannels == 1)
        return 0;
    else if (numChannels == 4)
        return 1;
    else if (numChannels == 9)
        return 2;
    else if (numChannels == 16)
        return 3;
    else
        return 0;
}

int numChannelsForOrder(int order)
{
    return (order + 1) * (order + 1);
}

int numSamplesForDuration(float duration, int samplingRate)
{
    return static_cast<int>(ceilf(duration * samplingRate));
}

IPLVector3 convertVector(float x, float y, float z)
{
    // Convert from FMOD's left-handed coordinate system to Steam Audio's right-handed system
    return IPLVector3{ x, y, -z };
}

IPLVector3 unitVector(IPLVector3 v)
{
    auto length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.0f)
    {
        return IPLVector3{ v.x / length, v.y / length, v.z / length };
    }
    else
    {
        return IPLVector3{ 0.0f, 0.0f, 0.0f };
    }
}

float dot(const IPLVector3& a, const IPLVector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

IPLVector3 cross(const IPLVector3& a, const IPLVector3& b)
{
    return IPLVector3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float distance(const IPLVector3& a, const IPLVector3& b)
{
    auto dx = a.x - b.x;
    auto dy = a.y - b.y;
    auto dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

void applyVolumeRamp(float startVolume, float endVolume, int numSamples, float* buffer)
{
    if (numSamples <= 0)
        return;

    auto volumeStep = (endVolume - startVolume) / numSamples;
    auto currentVolume = startVolume;

    for (auto i = 0; i < numSamples; ++i)
    {
        buffer[i] *= currentVolume;
        currentVolume += volumeStep;
    }
}

IPLCoordinateSpace3 calcCoordinates(const FMOD_3D_ATTRIBUTES& attributes)
{
    IPLCoordinateSpace3 coordinates{};
    
    coordinates.origin = convertVector(attributes.position.x, attributes.position.y, attributes.position.z);
    coordinates.ahead = convertVector(attributes.forward.x, attributes.forward.y, attributes.forward.z);
    coordinates.up = convertVector(attributes.up.x, attributes.up.y, attributes.up.z);
    coordinates.right = cross(coordinates.ahead, coordinates.up);

    return coordinates;
}

IPLCoordinateSpace3 calcListenerCoordinates(FMOD_DSP_STATE* state)
{
    FMOD_3D_ATTRIBUTES attributes{};
    state->functions->getlistenerattributes(state, 0, &attributes);
    return calcCoordinates(attributes);
}

bool initFmodOutBufferFormat(const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                             FMOD_DSP_BUFFER_ARRAY* outBuffers,
                             FMOD_DSP_STATE* state,
                             ParameterSpeakerFormatType outputFormat)
{
    if (!inBuffers || !outBuffers || !state)
        return false;

    FMOD_SPEAKERMODE speakerMode;
    FMOD_SPEAKERMODE numRawSpeakers;

    switch (outputFormat)
    {
    case PARAMETER_FROM_MIXER:
        state->functions->getspeakermode(state, &speakerMode, &numRawSpeakers);
        break;
    case PARAMETER_FROM_FINAL_OUTPUT:
        state->functions->getspeakermode(state, &speakerMode, &numRawSpeakers);
        break;
    case PARAMETER_FROM_INPUT:
        if (inBuffers->buffernumchannels && inBuffers->buffernumchannels[0] > 0)
        {
            outBuffers->buffernumchannels[0] = inBuffers->buffernumchannels[0];
            outBuffers->bufferchannelmask[0] = inBuffers->bufferchannelmask[0];
            return true;
        }
        return false;
    default:
        return false;
    }

    // Set output format based on speaker mode
    int numChannels = 2; // Default to stereo
    switch (speakerMode)
    {
    case FMOD_SPEAKERMODE_MONO:
        numChannels = 1;
        break;
    case FMOD_SPEAKERMODE_STEREO:
        numChannels = 2;
        break;
    case FMOD_SPEAKERMODE_QUAD:
        numChannels = 4;
        break;
    case FMOD_SPEAKERMODE_5POINT1:
        numChannels = 6;
        break;
    case FMOD_SPEAKERMODE_7POINT1:
        numChannels = 8;
        break;
    default:
        numChannels = 2;
        break;
    }

    outBuffers->buffernumchannels[0] = numChannels;
    return true;
}

void initializeContextSharing()
{
    gContextShared.store(false);
    gSharedContext.store(nullptr);
}

void shutdownContextSharing()
{
    gContextShared.store(false);
    gSharedContext.store(nullptr);
}

IPLContext getSharedContext()
{
    return gSharedContext.load();
}

void setSharedContext(IPLContext context)
{
    gSharedContext.store(context);
    gContextShared.store(context != nullptr);
}

// --------------------------------------------------------------------------------------------------------------------
// SourceManager Implementation
// --------------------------------------------------------------------------------------------------------------------

SourceManager::SourceManager()
    : mNextHandle(1)
{
}

SourceManager::~SourceManager()
{
    std::lock_guard<std::mutex> sourceLock(mSourceMutex);
    
    for (auto& pair : mSources)
    {
        if (pair.second)
        {
            iplSourceRelease(&pair.second);
        }
    }
    mSources.clear();
}

int32_t SourceManager::addSource(IPLSource source)
{
    if (!source)
        return -1;

    std::lock_guard<std::mutex> handleLock(mHandleMutex);
    std::lock_guard<std::mutex> sourceLock(mSourceMutex);

    int32_t handle;
    if (!mFreeHandles.empty())
    {
        handle = mFreeHandles.top();
        mFreeHandles.pop();
    }
    else
    {
        handle = mNextHandle++;
    }

    iplSourceRetain(source);
    mSources[handle] = source;

    return handle;
}

void SourceManager::removeSource(int32_t handle)
{
    std::lock_guard<std::mutex> handleLock(mHandleMutex);
    std::lock_guard<std::mutex> sourceLock(mSourceMutex);

    auto it = mSources.find(handle);
    if (it != mSources.end())
    {
        if (it->second)
        {
            iplSourceRelease(&it->second);
        }
        mSources.erase(it);
        mFreeHandles.push(handle);
    }
}

IPLSource SourceManager::getSource(int32_t handle)
{
    std::lock_guard<std::mutex> sourceLock(mSourceMutex);

    auto it = mSources.find(handle);
    if (it != mSources.end())
    {
        return it->second;
    }

    return nullptr;
}

}

// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

F_EXPORT void F_CALL iplFMODGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch)
{
    if (major) *major = STEAMAUDIO_FMODCORE_VERSION_MAJOR;
    if (minor) *minor = STEAMAUDIO_FMODCORE_VERSION_MINOR;
    if (patch) *patch = STEAMAUDIO_FMODCORE_VERSION_PATCH;
}

F_EXPORT void F_CALL iplFMODInitialize(IPLContext context)
{
    using namespace SteamAudioFMODCore;

    // Initialize context sharing
    initializeContextSharing();

    // Use provided context or try to get shared context from Unity
    if (context)
    {
        gContext = context;
        iplContextRetain(gContext);
    }
    else
    {
        auto sharedContext = getSharedContext();
        if (sharedContext)
        {
            gContext = sharedContext;
            iplContextRetain(gContext);
        }
        else
        {
            // Create a new context if none is available
            IPLContextSettings contextSettings{};
            contextSettings.version = STEAMAUDIO_VERSION;
            
            auto status = iplContextCreate(&contextSettings, &gContext);
            if (status != IPL_STATUS_SUCCESS)
            {
                gContext = nullptr;
            }
        }
    }

    // Initialize other global state
    gNewHRTFWritten.store(false);
    gIsSimulationSettingsValid.store(false);
    gNewReverbSourceWritten.store(false);
    gNewReflectionMixerWritten.store(false);
    gHRTFDisabled.store(false);
}

F_EXPORT void F_CALL iplFMODTerminate()
{
    using namespace SteamAudioFMODCore;

    // Clean up HRTFs
    for (int i = 0; i < 2; ++i)
    {
        if (gHRTF[i])
        {
            iplHRTFRelease(&gHRTF[i]);
        }
    }

    // Clean up reverb sources
    for (int i = 0; i < 2; ++i)
    {
        if (gReverbSource[i])
        {
            iplSourceRelease(&gReverbSource[i]);
        }
    }

    // Clean up reflection mixers
    for (int i = 0; i < 2; ++i)
    {
        if (gReflectionMixer[i])
        {
            iplReflectionMixerRelease(&gReflectionMixer[i]);
        }
    }

    // Clean up context
    if (gContext)
    {
        iplContextRelease(&gContext);
    }

    // Shutdown context sharing
    shutdownContextSharing();
}

F_EXPORT void F_CALL iplFMODSetHRTF(IPLHRTF hrtf)
{
    using namespace SteamAudioFMODCore;

    if (gHRTF[1])
    {
        iplHRTFRelease(&gHRTF[1]);
    }

    gHRTF[1] = hrtf;
    if (gHRTF[1])
    {
        iplHRTFRetain(gHRTF[1]);
    }

    gNewHRTFWritten.store(true);
}

F_EXPORT void F_CALL iplFMODSetSimulationSettings(IPLSimulationSettings simulationSettings)
{
    using namespace SteamAudioFMODCore;

    gSimulationSettings = simulationSettings;
    gIsSimulationSettingsValid.store(true);
}

F_EXPORT void F_CALL iplFMODSetReverbSource(IPLSource reverbSource)
{
    using namespace SteamAudioFMODCore;

    if (gReverbSource[1])
    {
        iplSourceRelease(&gReverbSource[1]);
    }

    gReverbSource[1] = reverbSource;
    if (gReverbSource[1])
    {
        iplSourceRetain(gReverbSource[1]);
    }

    gNewReverbSourceWritten.store(true);
}

F_EXPORT IPLint32 F_CALL iplFMODAddSource(IPLSource source)
{
    using namespace SteamAudioFMODCore;
    return gSourceManager.addSource(source);
}

F_EXPORT void F_CALL iplFMODRemoveSource(IPLint32 handle)
{
    using namespace SteamAudioFMODCore;
    gSourceManager.removeSource(handle);
}

F_EXPORT void F_CALL iplFMODSetHRTFDisabled(bool disabled)
{
    using namespace SteamAudioFMODCore;
    gHRTFDisabled.store(disabled);
}

F_EXPORT void F_CALL iplFMODSetSharedContext(IPLContext context)
{
    using namespace SteamAudioFMODCore;
    setSharedContext(context);
}

F_EXPORT IPLContext F_CALL iplFMODGetSharedContext()
{
    using namespace SteamAudioFMODCore;
    return getSharedContext();
}

// --------------------------------------------------------------------------------------------------------------------
// Unity-specific API Functions
// --------------------------------------------------------------------------------------------------------------------

F_EXPORT void F_CALL iplUnitySetReverbSource(IPLSource reverbSource)
{
    iplFMODSetReverbSource(reverbSource);
}

F_EXPORT int F_CALL iplUnityAddSource(IPLSource source)
{
    return iplFMODAddSource(source);
}

F_EXPORT void F_CALL iplUnityRemoveSource(int handle)
{
    iplFMODRemoveSource(handle);
}

F_EXPORT void F_CALL iplUnityTerminate()
{
    iplFMODTerminate();
}

F_EXPORT void F_CALL iplUnitySetHRTFDisabled(bool disabled)
{
    iplFMODSetHRTFDisabled(disabled);
}

}
