
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
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters
    bool binaural;
    float mixLevel;
    int32_t simOutHandle;
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
        , monoBuffer{}
        , outputBuffer{}
        , binaural(false)
        , mixLevel(1.0f)
        , simOutHandle(-1)
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

FMOD_RESULT F_CALLBACK reverbCreate(FMOD_DSP_STATE* dsp);
FMOD_RESULT F_CALLBACK reverbRelease(FMOD_DSP_STATE* dsp);
FMOD_RESULT F_CALLBACK reverbReset(FMOD_DSP_STATE* dsp);
FMOD_RESULT F_CALLBACK reverbProcess(FMOD_DSP_STATE* dsp,
                                     unsigned int length,
                                     const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                     FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                     FMOD_BOOL inputsIdle,
                                     FMOD_DSP_PROCESS_OPERATION op);
FMOD_RESULT F_CALLBACK reverbSetParameterFloat(FMOD_DSP_STATE* dsp, int index, float value);
FMOD_RESULT F_CALLBACK reverbSetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL value);
FMOD_RESULT F_CALLBACK reverbSetParameterInt(FMOD_DSP_STATE* dsp, int index, int value);
FMOD_RESULT F_CALLBACK reverbGetParameterFloat(FMOD_DSP_STATE* dsp, int index, float* value, char* valuestr);
FMOD_RESULT F_CALLBACK reverbGetParameterInt(FMOD_DSP_STATE* dsp, int index, int* value, char* valuestr);
FMOD_RESULT F_CALLBACK reverbGetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL* value, char* valuestr);

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
    if (state->monoBuffer.data)
        iplAudioBufferFree(gContext, &state->monoBuffer);
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
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->outputFormatType))
            return FMOD_ERR_FORMAT;
        return FMOD_OK;
    }

    if (!gContext || inputsIdle)
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

        // Validate context before allocation
        if (!gContext)
        {
            if (inBuffers->buffers[0] != outBuffers->buffers[0])
            {
                memcpy(outBuffers->buffers[0], inBuffers->buffers[0],
                       length * inBuffers->buffernumchannels[0] * sizeof(float));
            }
            return FMOD_OK;
        }

        iplAudioBufferAllocate(gContext, 1, length, &state->monoBuffer);
        iplAudioBufferAllocate(gContext, numChannelsOut, length, &state->outputBuffer);

        if (gIsSimulationSettingsValid.load())
        {
            auto numAmbisonicChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
            iplAudioBufferAllocate(gContext, numAmbisonicChannels, length, &state->ambisonicsBuffer);

            IPLReflectionEffectSettings reflectionSettings{};
            reflectionSettings.type = gSimulationSettings.reflectionType;
            reflectionSettings.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, audioSettings.samplingRate);
            reflectionSettings.numChannels = numAmbisonicChannels;
            iplReflectionEffectCreate(gContext, &audioSettings, &reflectionSettings, &state->reflectionEffect);

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

    if (gNewReverbSourceWritten.load())
    {
        iplSourceRelease(&gReverbSource[0]);
        gReverbSource[0] = iplSourceRetain(gReverbSource[1]);
        gNewReverbSourceWritten.store(false);
    }

    if (!gReverbSource[0])
        return FMOD_OK;

    // Additional context validation before Steam Audio calls
    if (!gContext)
    {
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0],
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    auto listenerCoordinates = calcListenerCoordinates(dsp);

    IPLAudioBuffer inBuffer;
    inBuffer.numChannels = numChannelsIn;
    inBuffer.numSamples = length;
    inBuffer.data = &in;

    iplAudioBufferDownmix(gContext, &inBuffer, &state->monoBuffer);

    IPLSimulationOutputs reverbOutputs{};
    iplSourceGetOutputs(gReverbSource[0], IPL_SIMULATIONFLAGS_REFLECTIONS, &reverbOutputs);

    IPLReflectionEffectParams reflectionParams = reverbOutputs.reflections;
    reflectionParams.type = gSimulationSettings.reflectionType;
    reflectionParams.numChannels = numChannelsForOrder(gSimulationSettings.maxOrder);
    reflectionParams.irSize = numSamplesForDuration(gSimulationSettings.maxDuration, audioSettings.samplingRate);

    if (gNewReflectionMixerWritten.load())
    {
        iplReflectionMixerRelease(&gReflectionMixer[0]);
        gReflectionMixer[0] = iplReflectionMixerRetain(gReflectionMixer[1]);
        gNewReflectionMixerWritten.store(false);
    }

    // Validate effects before calling
    if (!state->reflectionEffect || !gReflectionMixer[0])
    {
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0],
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    iplReflectionEffectApply(state->reflectionEffect, &reflectionParams, &state->monoBuffer, &state->ambisonicsBuffer, gReflectionMixer[0]);

    IPLAmbisonicsDecodeEffectParams ambisonicsParams{};
    ambisonicsParams.order = gSimulationSettings.maxOrder;
    ambisonicsParams.hrtf = gHRTF[0];
    ambisonicsParams.orientation = listenerCoordinates;
    ambisonicsParams.binaural = (numChannelsOut == 2 && !gHRTFDisabled.load() && state->binaural) ? IPL_TRUE : IPL_FALSE;

    if (!state->ambisonicsEffect)
    {
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0],
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    iplAmbisonicsDecodeEffectApply(state->ambisonicsEffect, &ambisonicsParams, &state->ambisonicsBuffer, &state->outputBuffer);

    for (int i = 0; i < numChannelsOut; ++i)
    {
        applyVolumeRamp(state->mixLevel, state->mixLevel, length, state->outputBuffer.data[i]);
    }

    iplAudioBufferInterleave(gContext, &state->outputBuffer, out);

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbSetParameterFloat(FMOD_DSP_STATE* dsp, int index, float value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_MIXLEVEL:
        state->mixLevel = value;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbSetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL value)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;m

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
    case IPL_FMODCORE_REVERB_SIMULATION_OUTPUTS_HANDLE:
        state->simOutHandle = value;
        break;
    case IPL_FMODCORE_REVERB_OUTPUT_FORMAT:
        state->outputFormatType = static_cast<ParameterSpeakerFormatType>(value);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbGetParameterFloat(FMOD_DSP_STATE* dsp, int index, float* value, char* valuestr)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_MIXLEVEL:
        *value = state->mixLevel;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbGetParameterInt(FMOD_DSP_STATE* dsp, int index, int* value, char* valuestr)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_SIMULATION_OUTPUTS_HANDLE:
        *value = state->simOutHandle;
        break;
    case IPL_FMODCORE_REVERB_OUTPUT_FORMAT:
        *value = state->outputFormatType;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reverbGetParameterBool(FMOD_DSP_STATE* dsp, int index, FMOD_BOOL* value, char* valuestr)
{
    if (!dsp || !dsp->plugindata)
        return FMOD_ERR_INVALID_PARAM;

    auto* state = static_cast<ReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case IPL_FMODCORE_REVERB_BINAURAL:
        *value = state->binaural;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

namespace ReverbEffect {
void initReverbParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_REVERB_NUM_PARAMS; ++i)
    {
        gReverbParameterDescsArray[i] = &gReverbParameterDescs[i];
    }

    gReverbParameterDescs[IPL_FMODCORE_REVERB_BINAURAL].booldesc = {false};
    gReverbParameterDescs[IPL_FMODCORE_REVERB_MIXLEVEL].floatdesc = {0.0f, 1.0f, 1.0f};
    gReverbParameterDescs[IPL_FMODCORE_REVERB_SIMULATION_OUTPUTS_HANDLE].intdesc = {-1, 10000, -1};
    gReverbParameterDescs[IPL_FMODCORE_REVERB_OUTPUT_FORMAT].intdesc = {0, 2, 0, false, gReverbOutputFormatValues};
}
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
    reverbSetParameterFloat,
    reverbSetParameterInt,
    reverbSetParameterBool,
    nullptr,
    reverbGetParameterFloat,
    reverbGetParameterInt,
    reverbGetParameterBool,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

}