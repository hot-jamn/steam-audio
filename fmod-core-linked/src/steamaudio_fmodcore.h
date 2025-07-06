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

#include <assert.h>
#include <math.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#elif defined(IPL_OS_MACOSX)
#endif

#include <fmod/fmod.hpp>

#include <phonon.h>

#include "steamaudio_fmodcore_version.h"


typedef enum IPLFMODCoreSpatializerParams
{
    IPL_FMODCORE_SPATIALIZE_SOURCE_POSITION,
    IPL_FMODCORE_SPATIALIZE_OVERALL_GAIN,
    IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION,
    IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION,
    IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY,
    IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION,
    IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION,
    IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS,
    IPL_FMODCORE_SPATIALIZE_APPLY_PATHING,
    IPL_FMODCORE_SPATIALIZE_HRTF_INTERPOLATION,
    IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION,
    IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE,
    IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE,
    IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE,
    IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_LOW,
    IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_MID,
    IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_HIGH,
    IPL_FMODCORE_SPATIALIZE_DIRECTIVITY,
    IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT,
    IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER,
    IPL_FMODCORE_SPATIALIZE_OCCLUSION,
    IPL_FMODCORE_SPATIALIZE_TRANSMISSION_TYPE,
    IPL_FMODCORE_SPATIALIZE_TRANSMISSION_LOW,
    IPL_FMODCORE_SPATIALIZE_TRANSMISSION_MID,
    IPL_FMODCORE_SPATIALIZE_TRANSMISSION_HIGH,
    IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL,
    IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL,
    IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL,
    IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL,
    IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL,
    IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS,
    IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL,
    IPL_FMODCORE_SPATIALIZE_DISTANCE_ATTENUATION_RANGE,
    IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE,
    IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT,
    IPL_FMODCORE_X,
    IPL_FMODCORE_Y,
    IPL_FMODCORE_Z,
    IPL_FMODCORE_SAMPLE_RATE,
    IPL_FMODCORE_SPATIALIZE_NUM_PARAMS
} IPLFMODCoreSpatializerParams;

typedef enum IPLFMODCoreReverbParams
{
    IPL_FMODCORE_REVERB_BINAURAL,
    IPL_FMODCORE_REVERB_MIXLEVEL,
    IPL_FMODCORE_REVERB_SAMPLE_RATE,
    IPL_FMODCORE_REVERB_FRAME_SIZE,
    IPL_FMODCORE_REVERB_OUTPUT_FORMAT,
    IPL_FMODCORE_REVERB_NUM_PARAMS
} IPLFMODCoreReverbParams;

typedef enum IPLFMODCoreMixerReturnParams
{
    IPL_FMODCORE_MIXRETURN_BINAURAL,
    IPL_FMODCORE_MIXRETURN_OUTPUT_FORMAT,
    IPL_FMODCORE_MIXRETURN_NUM_PARAMS
} IPLFMODCoreMixerReturnParams;



namespace SteamAudioFMODCore {

    extern FMOD_DSP_DESCRIPTION gSpatializeEffect;
    extern FMOD_DSP_DESCRIPTION gReverbEffect;
    extern FMOD_DSP_DESCRIPTION gMixerReturnEffect;

#if defined(STEAMAUDIO_FMODCORE_ENABLE_ENHANCED)
    extern FMOD_DSP_DESCRIPTION gSpatializeEffectEnhanced;
    extern FMOD_DSP_DESCRIPTION gReverbEffectEnhanced;
    extern FMOD_DSP_DESCRIPTION gMixerReturnEffectEnhanced;
#endif


// --------------------------------------------------------------------------------------------------------------------
// Parameter Types
// --------------------------------------------------------------------------------------------------------------------

enum ParameterApplyType
{
    PARAMETER_DISABLE,
    PARAMETER_SIMULATIONDEFINED,
    PARAMETER_USERDEFINED,
};

enum ParameterSpeakerFormatType
{
    PARAMETER_FROM_MIXER,
    PARAMETER_FROM_FINAL_OUTPUT,
    PARAMETER_FROM_INPUT,
};


// --------------------------------------------------------------------------------------------------------------------
// Global State
// --------------------------------------------------------------------------------------------------------------------

extern IPLContext gContext;
extern IPLHRTF gHRTF[2];
extern IPLSimulationSettings gSimulationSettings;
extern IPLSource gReverbSource[2];
extern IPLReflectionMixer gReflectionMixer[2];

extern std::atomic<bool> gNewHRTFWritten;
extern std::atomic<bool> gIsSimulationSettingsValid;
extern std::atomic<bool> gNewReverbSourceWritten;
extern std::atomic<bool> gNewReflectionMixerWritten;
extern std::atomic<bool> gHRTFDisabled;

class SourceManager
{
public:
    SourceManager();
    ~SourceManager();

    // Registers a source that has already been created, and returns the corresponding handle. A reference to the
    // IPLSource will be retained by this object.
    int32_t addSource(IPLSource source);

    // Unregisters a source (by handle), and releases the reference.
    void removeSource(int32_t handle);

    // Returns the IPLSource corresponding to a given handle. If the handle is invalid or the IPLSource has been
    // released, returns nullptr. Does not retain an additional reference.
    IPLSource getSource(int32_t handle);

private:
    // The next available integer that hasn't yet been assigned as the handle for any source.
    int32_t mNextHandle;

    // Handles for sources that have been unregistered, and which can now be reused. We will prefer reusing free
    // handle values over using a new handle value.
    std::priority_queue<int32_t> mFreeHandles;

    // The mapping from handle values to IPLSource objects.
    std::unordered_map<int32_t, IPLSource> mSources;

    // Synchronizes access to the handle priority queue and related values.
    std::mutex mHandleMutex;

    // Synchronizes access to the handle-to-source map.
    std::mutex mSourceMutex;
};

// Context sharing with Unity
extern std::atomic<IPLContext> gSharedContext;
extern std::atomic<bool> gContextShared;



// --------------------------------------------------------------------------------------------------------------------
// Helper Functions
// --------------------------------------------------------------------------------------------------------------------

// Returns an IPLSpeakerLayout that corresponds to a given number of channels.
IPLSpeakerLayout speakerLayoutForNumChannels(int numChannels);

// Returns the Ambisonics order corresponding to a given number of channels.
int orderForNumChannels(int numChannels);

// Returns the number of channels corresponding to a given Ambisonics order.
int numChannelsForOrder(int order);

// Returns the number of samples corresponding to a given duration and sampling rate.
int numSamplesForDuration(float duration,
                          int samplingRate);

// Converts a 3D vector from FMOD's coordinate system to Steam Audio's coordinate system.
IPLVector3 convertVector(float x,
                         float y,
                         float z);

// Normalizes a 3D vector.
IPLVector3 unitVector(IPLVector3 v);

// Calculates the dot product of two 3D vectors.
float dot(const IPLVector3& a,
          const IPLVector3& b);

// Calculates the cross product of two 3D vectors.
IPLVector3 cross(const IPLVector3& a,
                 const IPLVector3& b);

// Calculates the distance between two points.
float distance(const IPLVector3& a,
               const IPLVector3& b);

// Ramps a volume from a start value to an end value, applying it to a buffer.
void applyVolumeRamp(float startVolume,
                     float endVolume,
                     int numSamples,
                     float* buffer);

// Converts from FMOD's coordinate system structure to Steam Audio's.
IPLCoordinateSpace3 calcCoordinates(const FMOD_3D_ATTRIBUTES& attributes);

// Extracts listener coordinate system from the transform provided by FMOD.
IPLCoordinateSpace3 calcListenerCoordinates(FMOD_DSP_STATE* state);

// Initialize FMOD's outBuffer (output format, channel count, mask). Returns true on success.
bool initFmodOutBufferFormat(const FMOD_DSP_BUFFER_ARRAY* inBuffers, 
                             FMOD_DSP_BUFFER_ARRAY* outBuffers,
                             FMOD_DSP_STATE* state,
                             ParameterSpeakerFormatType outputFormat);

// Context sharing functions
void initializeContextSharing();
void shutdownContextSharing();
IPLContext getSharedContext();
void setSharedContext(IPLContext context);

// --------------------------------------------------------------------------------------------------------------------
// Parameter Initialization Functions
// --------------------------------------------------------------------------------------------------------------------

void initSpatializeParameterDescs();
void initReverbParameterDescs();
void initMixerReturnParameterDescs();

}


// --------------------------------------------------------------------------------------------------------------------
// API Functions
// --------------------------------------------------------------------------------------------------------------------

extern "C" {
    F_EXPORT void F_CALL iplFMODGetVersion(unsigned int* major, unsigned int* minor, unsigned int* patch);
    F_EXPORT void F_CALL iplFMODInitialize(IPLContext context);
    F_EXPORT void F_CALL iplFMODTerminate();
    F_EXPORT void F_CALL iplFMODSetHRTF(IPLHRTF hrtf);
    F_EXPORT void F_CALL iplFMODSetSimulationSettings(IPLSimulationSettings simulationSettings);
    F_EXPORT void F_CALL iplFMODSetReverbSource(IPLSource reverbSource);
    F_EXPORT IPLint32 F_CALL iplFMODAddSource(IPLSource source);
    F_EXPORT void F_CALL iplFMODRemoveSource(IPLint32 handle);
    F_EXPORT void F_CALL iplFMODSetHRTFDisabled(bool disabled);
}

namespace SteamAudioFMODCore {
    void initSpatializeParameterDescs();
    void initReverbParameterDescs();
    void initMixerReturnParameterDescs();
}
