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
    std::shared_ptr<SourceManager> gSourceManager;

    // --------------------------------------------------------------------------------------------------------------------
    // Helper Functions
    // --------------------------------------------------------------------------------------------------------------------

    IPLSpeakerLayout speakerLayoutForNumChannels(int numChannels)
    {
        IPLSpeakerLayout speakerLayout;
        speakerLayout.numSpeakers = numChannels;
        speakerLayout.speakers = nullptr;

        if (numChannels == 1)
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_MONO;
        else if (numChannels == 2)
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
        else if (numChannels == 4)
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC;
        else if (numChannels == 6)
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1;
        else if (numChannels == 8)
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1;
        else
            speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_CUSTOM;

        return speakerLayout;
    }

    int orderForNumChannels(int numChannels)
    {
        return static_cast<int>(sqrtf(static_cast<float>(numChannels))) - 1;
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
        if (length > 1e-2f)
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
        IPLCoordinateSpace3 coordinates;
        coordinates.ahead = convertVector(attributes.forward.x, attributes.forward.y, attributes.forward.z);
        coordinates.up = convertVector(attributes.up.x, attributes.up.y, attributes.up.z);
        coordinates.right = unitVector(cross(coordinates.ahead, coordinates.up));
        coordinates.origin = convertVector(attributes.position.x, attributes.position.y, attributes.position.z);
        return coordinates;
    }

    IPLCoordinateSpace3 calcListenerCoordinates(FMOD_DSP_STATE* state)
    {
        auto numListeners = 1;
        FMOD_3D_ATTRIBUTES listenerAttributes;
        state->functions->getlistenerattributes(state, &numListeners, &listenerAttributes);

        return calcCoordinates(listenerAttributes);
    }

    bool initFmodOutBufferFormat(const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                 FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                 FMOD_DSP_STATE* state,
                                 ParameterSpeakerFormatType outputFormat)
    {
        if (!inBuffers || !outBuffers || !state || !state->functions)
            return false;

        // platforms speaker mode
        FMOD_SPEAKERMODE mixerMode = {};
        // final speaker mode
        FMOD_SPEAKERMODE outputMode = {};
        state->functions->getspeakermode(state, &mixerMode, &outputMode);

        int bufferNumChannels = 0;
        FMOD_CHANNELMASK bufferChannelMask;
        FMOD_SPEAKERMODE outputSpeakerMode = {};

        switch (outputFormat) {
        case PARAMETER_FROM_MIXER:
            outputSpeakerMode = mixerMode;
            break;
        case PARAMETER_FROM_FINAL_OUTPUT:
            outputSpeakerMode = outputMode;
            break;
        case PARAMETER_FROM_INPUT:
            outputSpeakerMode = inBuffers->speakermode;
            break;
        default:
            // Missing switch case
            return false;
        }

        switch (outputSpeakerMode)
        {
        case FMOD_SPEAKERMODE_MONO:
            bufferNumChannels = 1;
            bufferChannelMask = FMOD_CHANNELMASK_MONO;
            break;
        case FMOD_SPEAKERMODE_STEREO:
            bufferNumChannels = 2;
            bufferChannelMask = FMOD_CHANNELMASK_STEREO;
            break;
        case FMOD_SPEAKERMODE_QUAD:
            bufferNumChannels = 4;
            bufferChannelMask = FMOD_CHANNELMASK_QUAD;
            break;
        case FMOD_SPEAKERMODE_SURROUND:
            bufferNumChannels = 5;
            bufferChannelMask = FMOD_CHANNELMASK_SURROUND;
            break;
        case FMOD_SPEAKERMODE_5POINT1:
            bufferNumChannels = 6;
            bufferChannelMask = FMOD_CHANNELMASK_5POINT1;
            break;
        case FMOD_SPEAKERMODE_7POINT1:
            bufferNumChannels = 8;
            bufferChannelMask = FMOD_CHANNELMASK_7POINT1;
            break;
        case FMOD_SPEAKERMODE_7POINT1POINT4:
            bufferNumChannels = 8;
            bufferChannelMask = FMOD_CHANNELMASK_7POINT1;
            outputSpeakerMode = FMOD_SPEAKERMODE_7POINT1;
            break;
        default:
            // Unsupported output format, prevent processing
            return false;
        }

        for (size_t i = 0; i < outBuffers->numbuffers; i++)
        {
            outBuffers->buffernumchannels[i] = bufferNumChannels;
            outBuffers->bufferchannelmask[i] = bufferChannelMask;
        }

        // Accept the input format by setting the output format to what the plugin can support for that input format
        outBuffers->speakermode = outputSpeakerMode;

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

    SourceManager::SourceManager() : mNextHandle(0)
    {
    }

    SourceManager::~SourceManager()
    {
        std::lock_guard<std::mutex> lock(mSourceMutex);
        for (auto& it : mSources)
        {
            iplSourceRelease(&mSources[it.first]);
        }
    }

    int32_t SourceManager::addSource(IPLSource source)
    {
        auto sourceRetained = iplSourceRetain(source);

        auto handle = -1;

        // First, figure out the handle we want to use.
        {
            std::lock_guard<std::mutex> lock(mHandleMutex);

            if (mFreeHandles.empty())
            {
                // No free handles, use the next-available unused handle.
                handle = mNextHandle++;
            }
            else
            {
                // Use one of the free handles.
                handle = mFreeHandles.top();
                mFreeHandles.pop();
            }
        }

        assert(handle >= 0);

        // Now store the mapping from the handle to this source.
        {
            std::lock_guard<std::mutex> lock(mSourceMutex);

            assert(mSources.find(handle) == mSources.end());

            mSources[handle] = sourceRetained;
        }

        return handle;
    }

    void SourceManager::removeSource(int32_t handle)
    {
        // Remove the source from the handle-to-source map.
        {
            std::lock_guard<std::mutex> lock(mSourceMutex);

            if (mSources.find(handle) != mSources.end())
            {
                iplSourceRelease(&mSources[handle]);
                mSources.erase(handle);
            }
        }

        // Mark the handle as free.
        {
            std::lock_guard<std::mutex> lock(mHandleMutex);

            mFreeHandles.push(handle);
        }
    }

    IPLSource SourceManager::getSource(int32_t handle)
    {
        std::lock_guard<std::mutex> lock(mSourceMutex);

        if (mSources.find(handle) != mSources.end())
            return mSources[handle];
        else
            return nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {
    using namespace SteamAudioFMODCore;

    F_EXPORT void F_CALL iplFMODGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch)
    {
        if (major) *major = STEAMAUDIO_FMODCORE_VERSION_MAJOR;
        if (minor) *minor = STEAMAUDIO_FMODCORE_VERSION_MINOR;
        if (patch) *patch = STEAMAUDIO_FMODCORE_VERSION_PATCH;
    }

    F_EXPORT void F_CALL iplFMODInitialize(IPLContext context)
    {
        // Initialize context sharing
        initializeContextSharing();

        // Use provided context or try to get shared context from Unity
        if (context)
        {
            gContext = iplContextRetain(context);
        }
        else
        {
            auto sharedContext = getSharedContext();
            if (sharedContext)
            {
                gContext = iplContextRetain(sharedContext);
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

        gSourceManager = std::make_shared<SourceManager>();

        // Initialize other global state
        gNewHRTFWritten.store(false);
        gIsSimulationSettingsValid.store(false);
        gNewReverbSourceWritten.store(false);
        gNewReflectionMixerWritten.store(false);
        gHRTFDisabled.store(false);
    }

    F_EXPORT void F_CALL iplFMODTerminate()
    {
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

        gNewHRTFWritten.store(false);
        gIsSimulationSettingsValid.store(false);
        gNewReverbSourceWritten.store(false);
        gNewReflectionMixerWritten.store(false);
        gHRTFDisabled.store(false);

        gSourceManager = nullptr;
    }

    F_EXPORT void F_CALL iplFMODSetHRTF(IPLHRTF hrtf)
    {
		if (gHRTF[1] == hrtf) return; // No change, nothing to do

        if (!gNewHRTFWritten)
        {
            iplHRTFRelease(&gHRTF[1]);
            gHRTF[1] = iplHRTFRetain(hrtf);

            gNewHRTFWritten = true;
        }
    }

    F_EXPORT void F_CALL iplFMODSetSimulationSettings(IPLSimulationSettings simulationSettings)
    {
        gSimulationSettings = simulationSettings;
        gIsSimulationSettingsValid.store(true);
    }

    F_EXPORT void F_CALL iplFMODSetReverbSource(IPLSource reverbSource)
    {
        if (reverbSource == gReverbSource[1])
            return;

        if (!gNewReverbSourceWritten)
        {
            iplSourceRelease(&gReverbSource[1]);
            gReverbSource[1] = iplSourceRetain(reverbSource);

            gNewReverbSourceWritten = true;
        }
    }

    F_EXPORT IPLint32 F_CALL iplFMODAddSource(IPLSource source)
    {
        if (!gSourceManager)
            return -1;

        return gSourceManager->addSource(source);
    }

    F_EXPORT void F_CALL iplFMODRemoveSource(IPLint32 handle)
    {
        if (!gSourceManager)
            return;

        gSourceManager->removeSource(handle);
    }

    F_EXPORT void F_CALL iplFMODSetHRTFDisabled(bool disabled)
    {
        gHRTFDisabled.store(disabled);
    }
}
