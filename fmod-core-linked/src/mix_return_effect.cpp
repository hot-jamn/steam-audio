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
    IPLReflectionMixer reflectionMixer;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio settings (Steam Audio 4.6.1 uses IPLAudioSettings instead of IPLAudioFormat)
    IPLAudioSettings audioSettings;
    int inputChannels;
    int outputChannels;

    // Audio buffers
    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters
    bool binaural;
    ParameterSpeakerFormatType outputFormatType;

    // State tracking
    bool initialized;
    int currentHRTFIndex;
    int currentReflectionMixerIndex;

    MixerReturnEffectState()
        : reflectionMixer(nullptr)
        , ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , audioSettings{}
        , inputChannels(0)
        , outputChannels(0)
        , inBuffer{}
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

    if (state->reflectionMixer)
        iplReflectionMixerRelease(&state->reflectionMixer);
    if (state->binauralEffect)
        iplBinauralEffectRelease(&state->binauralEffect);
    if (state->ambisonicsEffect)
        iplAmbisonicsDecodeEffectRelease(&state->ambisonicsEffect);

    // Clean up audio buffers
    if (state->inBuffer.data)
        iplAudioBufferFree(gContext, &state->inBuffer);
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
    
    if (state->reflectionMixer)
        iplReflectionMixerReset(state->reflectionMixer);
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
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->outputFormatType))
            return FMOD_ERR_FORMAT;
        return FMOD_OK;
    }

    if (!gContext || !gIsSimulationSettingsValid.load() || inputsIdle)
    {
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0],
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    IPLAudioSettings audioSettings;
    dsp->functions->getsamplerate(dsp, &audioSettings.samplingRate);
    audioSettings.frameSize = length;

    int numChannelsIn = inBuffers->buffernumchannels[0];
    int numChannelsOut = outBuffers->buffernumchannels[0];
    float* in = inBuffers->buffers[0];
    float* out = outBuffers->buffers[0];

    memset(out, 0, numChannelsOut * length * sizeof(float));

    if (!state->initialized)
    {
        IPLAudioSettings audioSettings;
        dsp->functions->getsamplerate(dsp, &audioSettings.samplingRate);
        audioSettings.frameSize = length;

        iplAudioBufferAllocate(gContext, numChannelsIn, length, &state->inBuffer);
        iplAudioBufferAllocate(gContext, numChannelsOut, length, &state->outputBuffer);

        if (gIsSimulationSettingsValid.load())
        {
            IPLReflectionEffectSettings reflectionSettings{};
            reflectionSettings.type = gSimulationSettings.reflectionType;
            reflectionSettings.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
            iplReflectionMixerCreate(gContext, &audioSettings, &reflectionSettings, &state->reflectionMixer);

            IPLAmbisonicsDecodeEffectSettings ambisonicsSettings{};
            ambisonicsSettings.speakerLayout = speakerLayoutForNumChannels(numChannelsOut);
            ambisonicsSettings.hrtf = gHRTF[0];
            ambisonicsSettings.maxOrder = gSimulationSettings.maxOrder;
            iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &ambisonicsSettings, &state->ambisonicsEffect);
        }

        state->initialized = true;
    }

    if (gNewHRTFWritten.load())
    {
        iplHRTFRelease(&gHRTF[0]);
        gHRTF[0] = iplHRTFRetain(gHRTF[1]);
        gNewHRTFWritten.store(false);
    }

    auto listenerCoordinates = calcListenerCoordinates(dsp);

    if (state->reflectionMixer)
    {
        IPLReflectionEffectParams reflectionParams{};
        reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
        iplReflectionMixerApply(state->reflectionMixer, &reflectionParams, &state->outputBuffer);
    }

    iplAudioBufferDeinterleave(gContext, in, &state->inBuffer);
    iplAudioBufferMix(gContext, &state->inBuffer, &state->outputBuffer);

    iplAudioBufferInterleave(gContext, &state->outputBuffer, out);

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

FMOD_RESULT F_CALLBACK mixerReturnGetParameterInt(FMOD_DSP_STATE* dsp, int index, int* value, char* valuestr)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_MIXRETURN_OUTPUT_FORMAT:
        *value = state->outputFormatType;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK mixerReturnGetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL* value, char* valuestr)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<MixerReturnEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_MIXRETURN_BINAURAL:
        *value = state->binaural;
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

namespace MixerReturnEffect {
void initMixerReturnParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_MIXRETURN_NUM_PARAMS; ++i)
    {
        gMixerReturnParameterDescsArray[i] = &gMixerReturnParameterDescs[i];
    }

    gMixerReturnParameterDescs[IPL_FMODCORE_MIXRETURN_BINAURAL].booldesc = {false};
    gMixerReturnParameterDescs[IPL_FMODCORE_MIXRETURN_OUTPUT_FORMAT].intdesc = {0, 2, 0, false, gMixerReturnOutputFormatValues};
}
}

// --------------------------------------------------------------------------------------------------------------------
// DSP Description
// --------------------------------------------------------------------------------------------------------------------
 FMOD_DSP_DESCRIPTION gMixerReturnEffect = {
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
        mixerReturnGetParameterInt,
        mixerReturnGetParameterBool,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
}