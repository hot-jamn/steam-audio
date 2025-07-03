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
// Spatializer Effect State
// --------------------------------------------------------------------------------------------------------------------

struct SpatializerEffectState
{
    // Steam Audio objects
    IPLSource source;
    IPLDirectEffect directEffect;
    IPLReflectionEffect reflectionEffect;
    IPLPathEffect pathEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio buffers
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer ambisonicsBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters
    bool applyDistanceAttenuation;
    bool applyAirAbsorption;
    bool applyDirectivity;
    bool applyOcclusion;
    bool applyTransmission;
    bool applyReflections;
    bool applyPathing;
    bool directBinaural;
    bool reflectionsBinaural;
    bool pathingBinaural;
    
    float directMixLevel;
    float reflectionsMixLevel;
    float pathingMixLevel;
    
    int32_t simulationOutputsHandle;
    ParameterSpeakerFormatType outputFormatType;

    // Simulation outputs
    IPLSimulationOutputs simulationOutputs;
    
    // State tracking
    bool initialized;
    int currentHRTFIndex;
    int currentReverbSourceIndex;
    int currentReflectionMixerIndex;

    SpatializerEffectState()
        : source(nullptr)
        , directEffect(nullptr)
        , reflectionEffect(nullptr)
        , pathEffect(nullptr)
        , ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , monoBuffer{}
        , ambisonicsBuffer{}
        , outputBuffer{}
        , applyDistanceAttenuation(false)
        , applyAirAbsorption(false)
        , applyDirectivity(false)
        , applyOcclusion(false)
        , applyTransmission(false)
        , applyReflections(false)
        , applyPathing(false)
        , directBinaural(false)
        , reflectionsBinaural(false)
        , pathingBinaural(false)
        , directMixLevel(1.0f)
        , reflectionsMixLevel(1.0f)
        , pathingMixLevel(1.0f)
        , simulationOutputsHandle(-1)
        , outputFormatType(PARAMETER_FROM_MIXER)
        , simulationOutputs{}
        , initialized(false)
        , currentHRTFIndex(0)
        , currentReverbSourceIndex(0)
        , currentReflectionMixerIndex(0)
    {
    }
};

// --------------------------------------------------------------------------------------------------------------------
// DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK spatializeCreate(FMOD_DSP_STATE* dsp)
{
    if (!dsp)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = new SpatializerEffectState();
    dsp->plugindata = state;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeRelease(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);

    // Clean up Steam Audio objects
    if (state->binauralEffect)
        iplBinauralEffectRelease(&state->binauralEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectRelease(&state->ambisonicsEffect);
    if (state->pathEffect)
        iplPathEffectRelease(&state->pathEffect);
    if (state->reflectionEffect)
        iplReflectionEffectRelease(&state->reflectionEffect);
    if (state->directEffect)
        iplDirectEffectRelease(&state->directEffect);
    if (state->source)
        iplSourceRelease(&state->source);

    // Clean up audio buffers
    if (state->monoBuffer.data)
        iplAudioBufferFree(gContext, &state->monoBuffer);
    if (state->ambisonicsBuffer.data)
        iplAudioBufferFree(gContext, &state->ambisonicsBuffer);
    if (state->outputBuffer.data)
        iplAudioBufferFree(gContext, &state->outputBuffer);

    delete state;
    dsp->plugindata = nullptr;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeReset(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);
    
    // Reset effects
    if (state->directEffect)
        iplDirectEffectReset(state->directEffect);
    if (state->reflectionEffect)
        iplReflectionEffectReset(state->reflectionEffect);
    if (state->pathEffect)
        iplPathEffectReset(state->pathEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectReset(state->ambisonicsEffect);
    if (state->binauralEffect)
        iplBinauralEffectReset(state->binauralEffect);

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeProcess(FMOD_DSP_STATE* dsp,
                                         unsigned int length,
                                         const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                         FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                         FMOD_BOOL inputsIdle,
                                         FMOD_DSP_PROCESS_OPERATION op)
{
    if (!dsp || !dsp->plugindata || !inBuffers || !outBuffers)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);

    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        // Set up output format
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->outputFormatType))
            return FMOD_ERR_FORMAT;
        return FMOD_OK;
    }

    if (!gContext || inputsIdle)
    {
        // Pass through if no context or input is idle
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    // Initialize if needed
    if (!state->initialized)
    {
        // Set up audio formats
        // Audio format setup is handled by Steam Audio internally
        // We don't need to manually set up input/output formats

        // Create Steam Audio objects
        IPLAudioSettings audioSettings{};
        audioSettings.samplingRate = 48000; // Default, will be updated
        audioSettings.frameSize = length;

        IPLDirectEffectSettings directSettings{};
        directSettings.numChannels = inBuffers->buffernumchannels[0];
        iplDirectEffectCreate(gContext, &audioSettings, &directSettings, &state->directEffect);

        if (state->directBinaural && gHRTF[0])
        {
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[0];
            iplBinauralEffectCreate(gContext, &audioSettings, &binauralSettings, &state->binauralEffect);
        }

        // Allocate audio buffers
        iplAudioBufferAllocate(gContext, 1, length, &state->monoBuffer);
        iplAudioBufferAllocate(gContext, outBuffers->buffernumchannels[0], length, &state->outputBuffer);

        state->initialized = true;
    }

    // Update global state if needed
    if (gNewHRTFWritten.load())
    {
        // Update HRTF
        state->currentHRTFIndex = 1 - state->currentHRTFIndex;
        
        if (state->binauralEffect && gHRTF[state->currentHRTFIndex])
        {
            iplBinauralEffectRelease(&state->binauralEffect);
            
            IPLAudioSettings audioSettings{};
            audioSettings.samplingRate = 48000;
            audioSettings.frameSize = length;
            
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[state->currentHRTFIndex];
            iplBinauralEffectCreate(gContext, &audioSettings, &binauralSettings, &state->binauralEffect);
        }
    }

    // Get simulation outputs if available
    if (state->simulationOutputsHandle >= 0)
    {
        // This would be implemented to get simulation results from the source manager
        // For now, we'll use default values
        memset(&state->simulationOutputs, 0, sizeof(state->simulationOutputs));
    }

    // Get listener coordinates
    auto listenerCoordinates = calcListenerCoordinates(dsp);

    // Get source position (this would come from FMOD's 3D attributes)
    FMOD_3D_ATTRIBUTES sourceAttributes{};
    // In a real implementation, this would be obtained from FMOD's parameter system
    auto sourceCoordinates = calcCoordinates(sourceAttributes);

    // Process direct sound
    if (state->directEffect)
    {
        IPLDirectEffectParams directParams{};
        directParams.flags = static_cast<IPLDirectEffectFlags>(0);
        
        if (state->applyDistanceAttenuation)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
        if (state->applyAirAbsorption)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
        if (state->applyDirectivity)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
        if (state->applyOcclusion)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);
        if (state->applyTransmission)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);

        directParams.distanceAttenuation = 1.0f;
        directParams.airAbsorption[0] = 1.0f;
        directParams.airAbsorption[1] = 1.0f;
        directParams.airAbsorption[2] = 1.0f;
        directParams.directivity = 1.0f;
        directParams.occlusion = 1.0f;
        directParams.transmission[0] = 1.0f;
        directParams.transmission[1] = 1.0f;
        directParams.transmission[2] = 1.0f;

        // Set up input buffer
        IPLAudioBuffer inputBuffer{};
        inputBuffer.numChannels = inBuffers->buffernumchannels[0];
        inputBuffer.numSamples = length;
        inputBuffer.data = reinterpret_cast<float**>(&inBuffers->buffers[0]);

        iplDirectEffectApply(state->directEffect, &directParams, &inputBuffer, &state->outputBuffer);
    }

    // Apply binaural processing if enabled
    if (state->directBinaural && state->binauralEffect && !gHRTFDisabled.load())
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = unitVector(IPLVector3{ 
            sourceCoordinates.origin.x - listenerCoordinates.origin.x,
            sourceCoordinates.origin.y - listenerCoordinates.origin.y,
            sourceCoordinates.origin.z - listenerCoordinates.origin.z
        });
        binauralParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
        binauralParams.spatialBlend = 1.0f;
        binauralParams.hrtf = gHRTF[state->currentHRTFIndex];

        iplBinauralEffectApply(state->binauralEffect, &binauralParams, &state->outputBuffer, &state->outputBuffer);
    }

    // Copy output to FMOD buffer
    if (state->outputBuffer.data && outBuffers->buffers[0])
    {
        memcpy(outBuffers->buffers[0], state->outputBuffer.data[0], 
               length * outBuffers->buffernumchannels[0] * sizeof(float));
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeSetParameterFloat(FMOD_DSP_STATE* dsp, int index, float value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL:
        state->directMixLevel = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL:
        state->reflectionsMixLevel = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL:
        state->pathingMixLevel = value;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeSetParameterInt(FMOD_DSP_STATE* dsp, int index, int value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION:
        state->applyDistanceAttenuation = (value > 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION:
        state->applyAirAbsorption = (value > 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY:
        state->applyDirectivity = (value > 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION:
        state->applyOcclusion = (value > 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION:
        state->applyTransmission = (value > 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE:
        state->simulationOutputsHandle = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT:
        state->outputFormatType = static_cast<ParameterSpeakerFormatType>(value);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK spatializeSetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<SpatializerEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS:
        state->applyReflections = (value != 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_PATHING:
        state->applyPathing = (value != 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL:
        state->directBinaural = (value != 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL:
        state->reflectionsBinaural = (value != 0);
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL:
        state->pathingBinaural = (value != 0);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

// --------------------------------------------------------------------------------------------------------------------
// DSP Parameter Descriptions
// --------------------------------------------------------------------------------------------------------------------

FMOD_DSP_PARAMETER_DESC gSpatializeParameterDescs[] = {
    { FMOD_DSP_PARAMETER_TYPE_DATA, "SourcePos", "", "Position of the source." },
    { FMOD_DSP_PARAMETER_TYPE_DATA, "OverallGain", "", "Overall gain." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyDA", "", "Apply distance attenuation." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyAA", "", "Apply air absorption." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyDir", "", "Apply directivity." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyOccl", "", "Apply occlusion." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyTrans", "", "Apply transmission." },
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "ApplyRefl", "", "Apply reflections." },
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "ApplyPath", "", "Apply pathing." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "Interpolation", "", "HRTF interpolation." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DistAtt", "", "Distance attenuation." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "DAType", "", "Distance attenuation rolloff type." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DAMinDist", "", "Distance attenuation min distance." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DAMaxDist", "", "Distance attenuation max distance." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsLow", "", "Air absorption (low frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsMid", "", "Air absorption (mid frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsHigh", "", "Air absorption (high frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "Directivity", "", "Directivity." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipoleWeight", "", "Directivity dipole weight." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipolePower", "", "Directivity dipole power." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "Occlusion", "", "Occlusion." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "TransType", "", "Transmission type." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransLow", "", "Transmission (low frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransMid", "", "Transmission (mid frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransHigh", "", "Transmission (high frequency)." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DirMixLevel", "", "Direct mix level." },
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "ReflBinaural", "", "Apply HRTF to reflections." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "ReflMixLevel", "", "Reflections mix level." },
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "PathBinaural", "", "Apply HRTF to pathing." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "PathMixLevel", "", "Pathing mix level." },
    { FMOD_DSP_PARAMETER_TYPE_DATA, "SimOutputs", "", "Simulation outputs." },
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "DirectBinaural", "", "Apply HRTF to direct path." },
    { FMOD_DSP_PARAMETER_TYPE_DATA, "DistRange", "", "Distance attenuation range." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "SimOutHandle", "", "Simulation outputs handle." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "OutputFormat", "", "Output Format" },
};

FMOD_DSP_PARAMETER_DESC* gSpatializeParameterDescsArray[IPL_FMODCORE_SPATIALIZE_NUM_PARAMS];

const char* gParameterApplyTypeValues[] = {"Off", "Simulation-Defined", "User-Defined"};
const char* gDistanceAttenuationTypeValues[] = {"Off", "Physics-Based", "Curve-Driven"};
const char* gHRTFInterpolationValues[] = {"Nearest", "Bilinear"};
const char* gTransmissionTypeValues[] = {"Frequency Independent", "Frequency Dependent"};
const char* gRolloffTypeValues[] = {"Linear Squared", "Linear", "Inverse", "Inverse Squared", "Custom"};
const char* gOutputFormatValues[] = {"From Mixer", "From Final Out", "From Input"};

extern void initSpatializeParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_SPATIALIZE_NUM_PARAMS; ++i)
    {
        gSpatializeParameterDescsArray[i] = &gSpatializeParameterDescs[i];
    }

    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_SOURCE_POSITION].datadesc = {FMOD_DSP_PARAMETER_DATA_TYPE_3DATTRIBUTES};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_OVERALL_GAIN].datadesc = {FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION].intdesc = {0, 2, 0, false, gDistanceAttenuationTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION].intdesc = {0, 2, 0, false, gParameterApplyTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY].intdesc = {0, 2, 0, false, gParameterApplyTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION].intdesc = {0, 2, 0, false, gParameterApplyTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION].intdesc = {0, 2, 0, false, gParameterApplyTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS].booldesc = {false};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_APPLY_PATHING].booldesc = {false};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_HRTF_INTERPOLATION].intdesc = {0, 1, 0, false, gHRTFInterpolationValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE].intdesc = {0, 4, 2, false, gRolloffTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE].floatdesc = {0.0f, 10000.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE].floatdesc = {0.0f, 10000.0f, 20.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_LOW].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_MID].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_HIGH].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER].floatdesc = {1.0f, 4.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_OCCLUSION].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_TYPE].intdesc = {0, 1, 0, false, gTransmissionTypeValues};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_LOW].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_MID].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_HIGH].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL].floatdesc = {0.0f, 1.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL].booldesc = {false};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL].floatdesc = {0.0f, 10.0f, 1.0f};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL].booldesc = {false};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL].floatdesc = {0.0f, 10.0f, 1.0f};
    // Skip the missing parameter constants for now - they may not be needed in the current implementation
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL].booldesc = {true};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE].intdesc = {-1, 10000, -1};
    gSpatializeParameterDescs[IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT].intdesc = {0, 2, 0, false, gOutputFormatValues};
}
// --------------------------------------------------------------------------------------------------------------------
// DSP Description
// --------------------------------------------------------------------------------------------------------------------

 FMOD_DSP_DESCRIPTION gSpatializeEffect = {
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio FMOD Spatializer",
    STEAMAUDIO_FMODCORE_VERSION,
    1, 1,
    spatializeCreate,
    spatializeRelease,
    spatializeReset,
    nullptr,
    spatializeProcess,
    nullptr,
    IPL_FMODCORE_SPATIALIZE_NUM_PARAMS,
    gSpatializeParameterDescsArray,
    spatializeSetParameterFloat,
    spatializeSetParameterInt,
    spatializeSetParameterBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

}
