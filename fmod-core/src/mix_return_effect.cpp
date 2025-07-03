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
// Mixer Return Effect State
// --------------------------------------------------------------------------------------------------------------------

struct MixerReturnEffectState
{
    // Steam Audio objects
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio settings (Steam Audio 4.6.1 uses IPLAudioSettings instead of IPLAudioFormat)
    IPLAudioSettings audioSettings;
    int inputChannels;
    int outputChannels;

    // Audio buffers
    IPLAudioBuffer outputBuffer;

    // Parameters
    bool binaural;
    ParameterSpeakerFormatType outputFormatType;

    // State tracking
    bool initialized;
    int currentHRTFIndex;
    int currentReflectionMixerIndex;

    MixerReturnEffectState()
        : ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , audioSettings{}
        , inputChannels(0)
        , outputChannels(0)
        , outputBuffer{}
        , binaural(false)
        , outputFormatType(PARAMETER_FROM_MIXER)
        , initialized(false)
        , currentHRTFIndex(0)
        , currentReflectionMixerIndex(0)
    {
    }
};

// --------------------------------------------------------------------------------------------------------------------
// DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK mixerReturnCreate(FMOD_DSP_STATE* dsp)
{
    if (!dsp)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = new MixerReturnEffectState();
    dsp->plugindata = state;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnRelease(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

    // Clean up Steam Audio objects
    if (state->binauralEffect)
        iplBinauralEffectRelease(&state->binauralEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectRelease(&state->ambisonicsEffect);

    // Clean up audio buffers
    if (state->outputBuffer.data)
        iplAudioBufferFree(gContext, &state->outputBuffer);

    delete state;
    dsp->plugindata = nullptr;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnReset(FMOD_DSP_STATE* dsp)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);
    
    // Reset effects
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectReset(state->ambisonicsEffect);
    if (state->binauralEffect)
        iplBinauralEffectReset(state->binauralEffect);

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnProcess(FMOD_DSP_STATE* dsp,
                                          unsigned int length,
                                          const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                          FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                          FMOD_BOOL inputsIdle,
                                          FMOD_DSP_PROCESS_OPERATION op)
{
    if (!dsp || !dsp->plugindata || !inBuffers || !outBuffers)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

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
        // Set up audio settings for Steam Audio 4.6.1
        state->inputChannels = inBuffers->buffernumchannels[0];
        state->outputChannels = outBuffers->buffernumchannels[0];
        
        state->audioSettings.samplingRate = 48000; // Default sampling rate
        state->audioSettings.frameSize = length;

        // Create Steam Audio objects
        if (state->binaural && gHRTF[0])
        {
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[0];
            iplBinauralEffectCreate(gContext, &state->audioSettings, &binauralSettings, &state->binauralEffect);
        }

        // Check if input is Ambisonics format
        if (inBuffers->buffernumchannels[0] == 4 || inBuffers->buffernumchannels[0] == 9 || inBuffers->buffernumchannels[0] == 16)
        {
            // Assume Ambisonics input
            IPLAmbisonicsDecodeEffectSettings ambisonicsSettings{};
            ambisonicsSettings.speakerLayout = speakerLayoutForNumChannels(state->outputChannels);
            ambisonicsSettings.hrtf = gHRTF[0];
            ambisonicsSettings.maxOrder = orderForNumChannels(inBuffers->buffernumchannels[0]);
            iplAmbisonicsDecodeEffectCreate(gContext, &state->audioSettings, &ambisonicsSettings, &state->ambisonicsEffect);
        }

        // Allocate audio buffers
        iplAudioBufferAllocate(gContext, state->outputChannels, length, &state->outputBuffer);

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
            
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[state->currentHRTFIndex];
            iplBinauralEffectCreate(gContext, &state->audioSettings, &binauralSettings, &state->binauralEffect);
        }

        if (state->ambisonicsEffect && gHRTF[state->currentHRTFIndex])
        {
            iplAmbisonicsDecodeEffectRelease(&state->ambisonicsEffect);
            
            IPLAmbisonicsDecodeEffectSettings ambisonicsSettings{};
            ambisonicsSettings.speakerLayout = speakerLayoutForNumChannels(state->outputChannels);
            ambisonicsSettings.hrtf = gHRTF[state->currentHRTFIndex];
            ambisonicsSettings.maxOrder = orderForNumChannels(inBuffers->buffernumchannels[0]);
            iplAmbisonicsDecodeEffectCreate(gContext, &state->audioSettings, &ambisonicsSettings, &state->ambisonicsEffect);
        }
    }

    if (gNewReflectionMixerWritten.load())
    {
        // Update reflection mixer
        state->currentReflectionMixerIndex = 1 - state->currentReflectionMixerIndex;
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
           length * state->outputChannels * sizeof(float));

    // Apply Ambisonics decoding if we have an Ambisonics effect
    if (state->ambisonicsEffect)
    {
        IPLAmbisonicsDecodeEffectParams ambisonicsParams{};
        ambisonicsParams.order = orderForNumChannels(inBuffers->buffernumchannels[0]);
        ambisonicsParams.hrtf = gHRTF[state->currentHRTFIndex];
        ambisonicsParams.orientation = listenerCoordinates;
        ambisonicsParams.binaural = state->binaural ? IPL_TRUE : IPL_FALSE;

        iplAmbisonicsDecodeEffectApply(state->ambisonicsEffect, &ambisonicsParams, &inputBuffer, &state->outputBuffer);
    }

    // Apply binaural processing if enabled and we don't have Ambisonics decoding
    if (state->binaural && state->binauralEffect && !state->ambisonicsEffect && !gHRTFDisabled.load())
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = IPLVector3{ 0.0f, 0.0f, 1.0f }; // Default forward direction
        binauralParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
        binauralParams.spatialBlend = 1.0f;
        binauralParams.hrtf = gHRTF[state->currentHRTFIndex];

        iplBinauralEffectApply(state->binauralEffect, &binauralParams, &state->outputBuffer, &state->outputBuffer);
    }

    // Apply reflection mixer processing if available
    if (gReflectionMixer[state->currentReflectionMixerIndex])
    {
        // This would involve mixing reflected sound from multiple sources
        // For now, we'll just pass through the processed audio
    }

    // Copy output to FMOD buffer
    if (state->outputBuffer.data && outBuffers->buffers[0])
    {
        memcpy(outBuffers->buffers[0], state->outputBuffer.data[0], 
               length * outBuffers->buffernumchannels[0] * sizeof(float));
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnSetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_MIXRETURN_BINAURAL:
        state->binaural = (value != 0);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnSetParameterInt(FMOD_DSP_STATE* dsp, int index, int value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_MIXRETURN_OUTPUT_FORMAT:
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

FMOD_DSP_PARAMETER_DESC gMixerReturnParameterDescs[] = {
    { FMOD_DSP_PARAMETER_TYPE_BOOL, "Binaural", "", "Apply HRTF to mixer return." },
    { FMOD_DSP_PARAMETER_TYPE_INT, "OutputFormat", "", "Output Format" },
};

FMOD_DSP_PARAMETER_DESC* gMixerReturnParameterDescsArray[IPL_FMODCORE_MIXRETURN_NUM_PARAMS];

const char* gMixerReturnOutputFormatValues[] = {"From Mixer", "From Final Out", "From Input"};

void initMixerReturnParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_MIXRETURN_NUM_PARAMS; ++i)
    {
        gMixerReturnParameterDescsArray[i] = &gMixerReturnParameterDescs[i];
    }

    gMixerReturnParameterDescs[IPL_FMODCORE_MIXRETURN_BINAURAL].booldesc = {false};
    gMixerReturnParameterDescs[IPL_FMODCORE_MIXRETURN_OUTPUT_FORMAT].intdesc = {0, 2, 0, false, gMixerReturnOutputFormatValues};
}

// --------------------------------------------------------------------------------------------------------------------
// DSP Description
// --------------------------------------------------------------------------------------------------------------------

FMOD_DSP_DESCRIPTION gMixerReturnDescription = {
    FMOD_PLUGIN_SDK_VERSION,
    "SA MixerReturn",
    STEAMAUDIO_FMODCORE_VERSION,
    1, 1,
    mixerReturnCreate,
    mixerReturnRelease,
    mixerReturnReset,
    nullptr,
    mixerReturnProcess,
    nullptr,
    IPL_FMODCORE_MIXRETURN_NUM_PARAMS,
    gMixerReturnParameterDescsArray,
    nullptr,
    mixerReturnSetParameterInt,
    mixerReturnSetParameterBool,
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

// --------------------------------------------------------------------------------------------------------------------
// Export Function
// --------------------------------------------------------------------------------------------------------------------

extern "C" {

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_FMODCore_MixerReturn_GetDSPDescription()
{
    return &SteamAudioFMODCore::gMixerReturnDescription;
}

}