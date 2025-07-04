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
// Reverb Effect State
// --------------------------------------------------------------------------------------------------------------------

struct ReverbEffectState
{
    // Steam Audio objects
    IPLReflectionEffect reflectionEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio buffers
    IPLAudioBuffer ambisonicsBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters
    bool binaural;
    ParameterSpeakerFormatType outputFormatType;

    // State tracking
    bool initialized;
    int currentHRTFIndex;
    int currentReverbSourceIndex;

    ReverbEffectState()
        : reflectionEffect(nullptr)
        , ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , ambisonicsBuffer{}
        , outputBuffer{}
        , binaural(false)
        , outputFormatType(PARAMETER_FROM_MIXER)
        , initialized(false)
        , currentHRTFIndex(0)
        , currentReverbSourceIndex(0)
    {
    }
};

// --------------------------------------------------------------------------------------------------------------------
// DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK reverbCreate(FMOD_DSP_STATE* dsp)
{
    if (!dsp)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = new ReverbEffectState();
    dsp->plugindata = state;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbRelease(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    // Clean up Steam Audio objects
    if (state->binauralEffect)
        iplBinauralEffectRelease(&state->binauralEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectRelease(&state->ambisonicsEffect);
    if (state->reflectionEffect)
        iplReflectionEffectRelease(&state->reflectionEffect);

    // Clean up audio buffers
    if (state->ambisonicsBuffer.data)
        iplAudioBufferFree(gContext, &state->ambisonicsBuffer);
    if (state->outputBuffer.data)
        iplAudioBufferFree(gContext, &state->outputBuffer);

    delete state;
    dsp->plugindata = nullptr;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbReset(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);
    
    // Reset effects
    if (state->reflectionEffect)
        iplReflectionEffectReset(state->reflectionEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectReset(state->ambisonicsEffect);
    if (state->binauralEffect)
        iplBinauralEffectReset(state->binauralEffect);

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbProcess(FMOD_DSP_STATE* dsp,
                                     unsigned int length,
                                     const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                     FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                     FMOD_BOOL inputsIdle,
                                     FMOD_DSP_PROCESS_OPERATION op)
{
    if (!dsp || !dsp->plugindata || !inBuffers || !outBuffers)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

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
        // Create Steam Audio objects
        IPLAudioSettings audioSettings{};
        audioSettings.samplingRate = 48000; // Default, will be updated
        audioSettings.frameSize = length;

        if (gIsSimulationSettingsValid.load())
        {
            IPLReflectionEffectSettings reflectionSettings{};
            reflectionSettings.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
            reflectionSettings.numChannels = inBuffers->buffernumchannels[0];
            reflectionSettings.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, gSimulationSettings.samplingRate);
            iplReflectionEffectCreate(gContext, &audioSettings, &reflectionSettings, &state->reflectionEffect);
        }

        if (state->binaural && gHRTF[0])
        {
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[0];
            iplBinauralEffectCreate(gContext, &audioSettings, &binauralSettings, &state->binauralEffect);
        }

        // Allocate audio buffers
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

    if (gNewReverbSourceWritten.load())
    {
        // Update reverb source
        state->currentReverbSourceIndex = 1 - state->currentReverbSourceIndex;
    }

    // Get listener coordinates
    auto listenerCoordinates = calcListenerCoordinates(dsp);

    // Set up input buffer
    IPLAudioBuffer inputBuffer{};
    inputBuffer.numChannels = inBuffers->buffernumchannels[0];
    inputBuffer.numSamples = length;
    inputBuffer.data = reinterpret_cast<float**>(&inBuffers->buffers[0]);

    // Copy input to output initially
    memcpy(state->outputBuffer.data[0], inputBuffer.data[0],
           length * outBuffers->buffernumchannels[0] * sizeof(float));

    // Apply reverb processing if we have a reverb source
    if (state->reflectionEffect && gReverbSource[state->currentReverbSourceIndex])
    {
        IPLReflectionEffectParams reflectionParams{};
        reflectionParams.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        reflectionParams.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, gSimulationSettings.samplingRate);
        reflectionParams.tanDevice = nullptr;

        // Get reflection IR from the reverb source
        // This would typically involve getting simulation results
        // For now, we'll just pass through the audio

        iplReflectionEffectApply(state->reflectionEffect, &reflectionParams, &inputBuffer, &state->outputBuffer, nullptr);
    }

    // Apply binaural processing if enabled
    if (state->binaural && state->binauralEffect && !gHRTFDisabled.load())
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = IPLVector3{ 0.0f, 0.0f, 1.0f }; // Default forward direction
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

FMOD_RESULT F_CALLBACK reverbSetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_BINAURAL:
        state->binaural = (value != 0);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbSetParameterInt(FMOD_DSP_STATE* dsp, int index, int value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_OUTPUT_FORMAT:
        state->outputFormatType = static_cast<ParameterSpeakerFormatType>(value);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

// --------------------------------------------------------------------------------------------------------------------
// DSP Parameter Descriptions
// --------------------------------------------------------------------------------------------------------------------

FMOD_DSP_PARAMETER_DESC gReverbParameterDescs[] = {
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "Binaural", "", "Apply HRTF to reverb." },
    { FMOD_DSP_PARAMETER_TYPE_FLOAT, "MixLevel", "", "Reverb mix level." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "SimOutHandle", "", "Simulation outputs handle." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "OutputFormat", "", "Output Format" },
};

FMOD_DSP_PARAMETER_DESC* gReverbParameterDescsArray[IPL_FMODCORE_REVERB_NUM_PARAMS];

const char* gReverbOutputFormatValues[] = {"From Mixer", "From Final Out", "From Input"};

void initReverbParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_REVERB_NUM_PARAMS; ++i)
    {
        gReverbParameterDescsArray[i] = &gReverbParameterDescs[i];
    }

    gReverbParameterDescs[IPL_FMODCORE_REVERB_BINAURAL].booldesc = {false};
    gReverbParameterDescs[IPL_FMODCORE_REVERB_OUTPUT_FORMAT].intdesc = {0, 2, 0, false, gReverbOutputFormatValues};
}

// --------------------------------------------------------------------------------------------------------------------
// DSP Description
// --------------------------------------------------------------------------------------------------------------------
 FMOD_DSP_DESCRIPTION gReverbEffect = {
        FMOD_PLUGIN_SDK_VERSION,
        "Steam Audio FMOD Core Reverb",
        STEAMAUDIO_FMODCORE_VERSION,
        1, 1,
        reverbCreate,
        reverbRelease,
        reverbReset,
        nullptr,
        reverbProcess,
        nullptr,
        IPL_FMODCORE_REVERB_NUM_PARAMS,
        gReverbParameterDescsArray,
        nullptr,
        reverbSetParameterInt,
        reverbSetParameterBool,
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