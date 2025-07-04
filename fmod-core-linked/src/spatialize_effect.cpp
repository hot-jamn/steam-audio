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
f
namespace SpatializeEffect {

FMOD_DSP_PARAMETER_DESC gParams[IPL_FMODCORE_SPATIALIZE_NUM_PARAMS] = {};
FMOD_DSP_PARAMETER_DESC* gParamsArray[IPL_FMODCORE_SPATIALIZE_NUM_PARAMS];

const char* gParameterApplyTypeValues[] = {"Off", "Simulation-Defined", "User-Defined"};
const char* gDistanceAttenuationTypeValues[] = {"Off", "Physics-Based", "Curve-Driven"};
const char* gHRTFInterpolationValues[] = {"Nearest", "Bilinear"};
const char* gTransmissionTypeValues[] = {"Frequency Independent", "Frequency Dependent"};
const char* gRolloffTypeValues[] = {"Linear Squared", "Linear", "Inverse", "Inverse Squared", "Custom"};
const char* gOutputFormatValues[] = {"From Mixer", "From Final Out", "From Input"};

void initSpatializeParameterDescs()
{
    for (auto i = 0; i < IPL_FMODCORE_SPATIALIZE_NUM_PARAMS; ++i)
    {
        gParamsArray[i] = &gParams[i];
    }

    FMOD_DSP_PARAMETER_DESC_DATA attributes_desc = { FMOD_DSP_PARAMETER_DATA_TYPE_3DATTRIBUTES };
    gParams[IPL_FMODCORE_SPATIALIZE_SOURCE_POSITION] = { FMOD_DSP_PARAMETER_TYPE_DATA, "SourcePos", "", "Position of the source.", { .datadesc = attributes_desc } };
    FMOD_DSP_PARAMETER_DESC_DATA gain_desc = { FMOD_DSP_PARAMETER_DATA_TYPE_OVERALLGAIN };
    gParams[IPL_FMODCORE_SPATIALIZE_OVERALL_GAIN] = { FMOD_DSP_PARAMETER_TYPE_DATA, "OverallGain", "", "Overall gain.", { .datadesc = gain_desc } };
    FMOD_DSP_PARAMETER_DESC_INT apply_da_desc = { 0, 2, 0, false, gDistanceAttenuationTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION] = { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyDA", "", "Apply distance attenuation.", { .intdesc = apply_da_desc } };
    FMOD_DSP_PARAMETER_DESC_INT apply_aa_desc = { 0, 2, 0, false, gParameterApplyTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION] = { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyAA", "", "Apply air absorption.", { .intdesc = apply_aa_desc } };
    FMOD_DSP_PARAMETER_DESC_INT apply_dir_desc = { 0, 2, 0, false, gParameterApplyTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY] = { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyDir", "", "Apply directivity.", { .intdesc = apply_dir_desc } };
    FMOD_DSP_PARAMETER_DESC_INT apply_occl_desc = { 0, 2, 0, false, gParameterApplyTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION] = { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyOccl", "", "Apply occlusion.", { .intdesc = apply_occl_desc } };
    FMOD_DSP_PARAMETER_DESC_INT apply_trans_desc = { 0, 2, 0, false, gParameterApplyTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION] = { FMOD_DSP_PARAMETER_TYPE_INT, "ApplyTrans", "", "Apply transmission.", { .intdesc = apply_trans_desc } };
    FMOD_DSP_PARAMETER_DESC_BOOL apply_refl_desc = { false };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS] = { FMOD_DSP_PARAMETER_TYPE_BOOL, "ApplyRefl", "", "Apply reflections.", { .booldesc = apply_refl_desc } };
    FMOD_DSP_PARAMETER_DESC_BOOL apply_path_desc = { false };
    gParams[IPL_FMODCORE_SPATIALIZE_APPLY_PATHING] = { FMOD_DSP_PARAMETER_TYPE_BOOL, "ApplyPath", "", "Apply pathing.", { .booldesc = apply_path_desc } };
    FMOD_DSP_PARAMETER_DESC_INT hrtf_interp_desc = { 0, 1, 0, false, gHRTFInterpolationValues };
    gParams[IPL_FMODCORE_SPATIALIZE_HRTF_INTERPOLATION] = { FMOD_DSP_PARAMETER_TYPE_INT, "Interpolation", "", "HRTF interpolation.", { .intdesc = hrtf_interp_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT dist_atten_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DistAtt", "", "Distance attenuation.", { .floatdesc = dist_atten_desc } };
    FMOD_DSP_PARAMETER_DESC_INT da_type_desc = { 0, 4, 2, false, gRolloffTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE] = { FMOD_DSP_PARAMETER_TYPE_INT, "DAType", "", "Distance attenuation rolloff type.", { .intdesc = da_type_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT da_min_dist_desc = { 0.0f, 10000.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DAMinDist", "", "Distance attenuation min distance.", { .floatdesc = da_min_dist_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT da_max_dist_desc = { 0.0f, 10000.0f, 20.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DAMaxDist", "", "Distance attenuation max distance.", { .floatdesc = da_max_dist_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT air_abs_low_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_LOW] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsLow", "", "Air absorption (low frequency).", { .floatdesc = air_abs_low_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT air_abs_mid_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_MID] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsMid", "", "Air absorption (mid frequency).", { .floatdesc = air_abs_mid_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT air_abs_high_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_HIGH] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "AirAbsHigh", "", "Air absorption (high frequency).", { .floatdesc = air_abs_high_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT directivity_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "Directivity", "", "Directivity.", { .floatdesc = directivity_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT dipole_weight_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipoleWeight", "", "Directivity dipole weight.", { .floatdesc = dipole_weight_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT dipole_power_desc = { 1.0f, 4.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DipolePower", "", "Directivity dipole power.", { .floatdesc = dipole_power_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT occlusion_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_OCCLUSION] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "Occlusion", "", "Occlusion.", { .floatdesc = occlusion_desc } };
    FMOD_DSP_PARAMETER_DESC_INT trans_type_desc = { 0, 1, 0, false, gTransmissionTypeValues };
    gParams[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_TYPE] = { FMOD_DSP_PARAMETER_TYPE_INT, "TransType", "", "Transmission type.", { .intdesc = trans_type_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT trans_low_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_LOW] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransLow", "", "Transmission (low frequency).", { .floatdesc = trans_low_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT trans_mid_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_MID] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransMid", "", "Transmission (mid frequency).", { .floatdesc = trans_mid_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT trans_high_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_TRANSMISSION_HIGH] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "TransHigh", "", "Transmission (high frequency).", { .floatdesc = trans_high_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT direct_mix_level_desc = { 0.0f, 1.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "DirMixLevel", "", "Direct mix level.", { .floatdesc = direct_mix_level_desc } };
    FMOD_DSP_PARAMETER_DESC_BOOL refl_binaural_desc = { false };
    gParams[IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL] = { FMOD_DSP_PARAMETER_TYPE_BOOL, "ReflBinaural", "", "Apply HRTF to reflections.", { .booldesc = refl_binaural_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT refl_mix_level_desc = { 0.0f, 10.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "ReflMixLevel", "", "Reflections mix level.", { .floatdesc = refl_mix_level_desc } };
    FMOD_DSP_PARAMETER_DESC_BOOL path_binaural_desc = { false };
    gParams[IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL] = { FMOD_DSP_PARAMETER_TYPE_BOOL, "PathBinaural", "", "Apply HRTF to pathing.", { .booldesc = path_binaural_desc } };
    FMOD_DSP_PARAMETER_DESC_FLOAT path_mix_level_desc = { 0.0f, 10.0f, 1.0f };
    gParams[IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL] = { FMOD_DSP_PARAMETER_TYPE_FLOAT, "PathMixLevel", "", "Pathing mix level.", { .floatdesc = path_mix_level_desc } };
    FMOD_DSP_PARAMETER_DESC_DATA sim_outputs_desc = { FMOD_DSP_PARAMETER_DATA_TYPE_USER };
    gParams[IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS] = { FMOD_DSP_PARAMETER_TYPE_DATA, "SimOutputs", "", "Simulation outputs.", { .datadesc = sim_outputs_desc } };
    FMOD_DSP_PARAMETER_DESC_BOOL direct_binaural_desc = { true };
    gParams[IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL] = { FMOD_DSP_PARAMETER_TYPE_BOOL, "DirectBinaural", "", "Apply HRTF to direct path.", { .booldesc = direct_binaural_desc } };
    FMOD_DSP_PARAMETER_DESC_DATA dist_range_desc = { FMOD_DSP_PARAMETER_DATA_TYPE_ATTENUATION_RANGE };
    gParams[IPL_FMODCORE_SPATIALIZE_DISTANCE_ATTENUATION_RANGE] = { FMOD_DSP_PARAMETER_TYPE_DATA, "DistRange", "", "Distance attenuation range.", { .datadesc = dist_range_desc } };
    FMOD_DSP_PARAMETER_DESC_INT sim_out_handle_desc = { -1, 10000, -1 };
    gParams[IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE] = { FMOD_DSP_PARAMETER_TYPE_INT, "SimOutHandle", "", "Simulation outputs handle.", { .intdesc = sim_out_handle_desc } };
    FMOD_DSP_PARAMETER_DESC_INT output_format_desc = { 0, 2, 0, false, gOutputFormatValues };
    gParams[IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT] = { FMOD_DSP_PARAMETER_TYPE_INT, "OutputFormat", "", "Output Format", { .intdesc = output_format_desc } };
}

struct State
{
    FMOD_DSP_PARAMETER_3DATTRIBUTES source;
    FMOD_DSP_PARAMETER_OVERALLGAIN overallGain;
    ParameterApplyType applyDistanceAttenuation;
    ParameterApplyType applyAirAbsorption;
    ParameterApplyType applyDirectivity;
    ParameterApplyType applyOcclusion;
    ParameterApplyType applyTransmission;
    bool applyReflections;
    bool applyPathing;
    bool directBinaural;
    IPLHRTFInterpolation hrtfInterpolation;
    float distanceAttenuation;
    FMOD_DSP_PAN_3D_ROLLOFF_TYPE distanceAttenuationRolloffType;
    float distanceAttenuationMinDistance;
    float distanceAttenuationMaxDistance;
    float airAbsorption[3];
    float directivity;
    float dipoleWeight;
    float dipolePower;
    float occlusion;
    IPLTransmissionType transmissionType;
    float transmission[3];
    float directMixLevel;
    bool reflectionsBinaural;
    float reflectionsMixLevel;
    bool pathingBinaural;
    float pathingMixLevel;
    FMOD_DSP_PARAMETER_ATTENUATION_RANGE attenuationRange;
    std::atomic<bool> attenuationRangeSet;
    ParameterSpeakerFormatType outputFormat;
    IPLSource simulationSource[2];
    std::atomic<bool> newSimulationSourceWritten;
    float prevDirectMixLevel;
    float prevReflectionsMixLevel;
    float prevPathingMixLevel;
    IPLAudioBuffer inBuffer;
    IPLAudioBuffer outBuffer;
    IPLAudioBuffer directBuffer;
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer reflectionsBuffer;
    IPLAudioBuffer reflectionsSpatializedBuffer;
    IPLPanningEffect panningEffect;
    IPLPanningEffectSettings panningEffectSettingsBackup;
    IPLBinauralEffect binauralEffect;
    IPLDirectEffect directEffect;
    IPLDirectEffectSettings directEffectSettingsBackup;
    IPLReflectionEffect reflectionEffect;
    IPLReflectionEffectSettings reflectionEffectSettingsBackup;
    IPLPathEffect pathEffect;
    IPLPathEffectSettings pathEffectSettingsBackup;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLAmbisonicsDecodeEffectSettings ambisonicsEffectSettingsBackup;
};

FMOD_RESULT F_CALLBACK create(FMOD_DSP_STATE* state);
FMOD_RESULT F_CALLBACK release(FMOD_DSP_STATE* state);
FMOD_RESULT F_CALLBACK reset(FMOD_DSP_STATE* dsp);
FMOD_RESULT F_CALLBACK process(FMOD_DSP_STATE* dsp, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op);
FMOD_RESULT F_CALLBACK setFloat(FMOD_DSP_STATE* state, int index, float value);
FMOD_RESULT F_CALLBACK setInt(FMOD_DSP_STATE* state, int index, int value);
FMOD_RESULT F_CALLBACK setBool(FMOD_DSP_STATE* state, int index, FMOD_BOOL value);
FMOD_RESULT F_CALLBACK setData(FMOD_DSP_STATE* state, int index, void* value, unsigned int length);
FMOD_RESULT F_CALLBACK getFloat(FMOD_DSP_STATE* state, int index, float* value, char* valuestr);
FMOD_RESULT F_CALLBACK getInt(FMOD_DSP_STATE* state, int index, int* value, char* valuestr);
FMOD_RESULT F_CALLBACK getBool(FMOD_DSP_STATE* state, int index, FMOD_BOOL* value, char* valuestr);
FMOD_RESULT F_CALLBACK getData(FMOD_DSP_STATE* state, int index, void** value, unsigned int* length, char* valuestr);

}

FMOD_DSP_DESCRIPTION gSpatializeEffect = {
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Spatializer",
    STEAMAUDIO_FMODCORE_VERSION,
    1,
    1,
    SpatializeEffect::create,
    SpatializeEffect::release,
    SpatializeEffect::reset,
    nullptr,
    SpatializeEffect::process,
    nullptr,
    IPL_FMODCORE_SPATIALIZE_NUM_PARAMS,
    SpatializeEffect::gParamsArray,
    SpatializeEffect::setFloat,
    SpatializeEffect::setInt,
    SpatializeEffect::setBool,
    SpatializeEffect::setData,
    SpatializeEffect::getFloat,
    SpatializeEffect::getInt,
    SpatializeEffect::getBool,
    SpatializeEffect::getData,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

namespace SpatializeEffect {

FMOD_RESULT F_CALLBACK create(FMOD_DSP_STATE* state)
{
    state->plugindata = new State();
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK release(FMOD_DSP_STATE* state)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    delete effect;
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK reset(FMOD_DSP_STATE* dsp)
{
    auto effect = reinterpret_cast<State*>(dsp->plugindata);

    effect->newSimulationSourceWritten = false;
    effect->prevDirectMixLevel = 0.0f;
    effect->prevReflectionsMixLevel = 0.0f;
    effect->prevPathingMixLevel = 0.0f;
    effect->panningEffect = nullptr;
    effect->binauralEffect = nullptr;
    effect->directEffect = nullptr;
    effect->reflectionEffect = nullptr;
    effect->pathEffect = nullptr;
    effect->ambisonicsEffect = nullptr;

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK setFloat(FMOD_DSP_STATE* state, int index, float value)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION:
        effect->distanceAttenuation = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE:
        effect->distanceAttenuationMinDistance = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE:
        effect->distanceAttenuationMaxDistance = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_LOW:
        effect->airAbsorption[0] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_MID:
        effect->airAbsorption[1] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_HIGH:
        effect->airAbsorption[2] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY:
        effect->directivity = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT:
        effect->dipoleWeight = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER:
        effect->dipolePower = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_OCCLUSION:
        effect->occlusion = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_LOW:
        effect->transmission[0] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_MID:
        effect->transmission[1] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_HIGH:
        effect->transmission[2] = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL:
        effect->directMixLevel = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL:
        effect->reflectionsMixLevel = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL:
        effect->pathingMixLevel = value;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK setInt(FMOD_DSP_STATE* state, int index, int value)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION:
        effect->applyDistanceAttenuation = static_cast<ParameterApplyType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION:
        effect->applyAirAbsorption = static_cast<ParameterApplyType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY:
        effect->applyDirectivity = static_cast<ParameterApplyType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION:
        effect->applyOcclusion = static_cast<ParameterApplyType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION:
        effect->applyTransmission = static_cast<ParameterApplyType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_HRTF_INTERPOLATION:
        effect->hrtfInterpolation = static_cast<IPLHRTFInterpolation>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE:
        effect->distanceAttenuationRolloffType = static_cast<FMOD_DSP_PAN_3D_ROLLOFF_TYPE>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_TYPE:
        effect->transmissionType = static_cast<IPLTransmissionType>(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE:
        effect->simulationSource[1] = gSourceManager.getSource(value);
        break;
    case IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT:
        effect->outputFormat = static_cast<ParameterSpeakerFormatType>(value);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK setBool(FMOD_DSP_STATE* state, int index, FMOD_BOOL value)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL:
        effect->directBinaural = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS:
        effect->applyReflections = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_PATHING:
        effect->applyPathing = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL:
        effect->reflectionsBinaural = value;
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL:
        effect->pathingBinaural = value;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK setData(FMOD_DSP_STATE* state, int index, void* value, unsigned int length)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_SOURCE_POSITION:
        memcpy(&effect->source, value, length);
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCE_ATTENUATION_RANGE:
        memcpy(&effect->attenuationRange, value, length);
        effect->attenuationRangeSet = true;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK getFloat(FMOD_DSP_STATE* state, int index, float* value, char* valuestr)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION:
        *value = effect->distanceAttenuation;
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MINDISTANCE:
        *value = effect->distanceAttenuationMinDistance;
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_MAXDISTANCE:
        *value = effect->distanceAttenuationMaxDistance;
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_LOW:
        *value = effect->airAbsorption[0];
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_MID:
        *value = effect->airAbsorption[1];
        break;
    case IPL_FMODCORE_SPATIALIZE_AIRABSORPTION_HIGH:
        *value = effect->airAbsorption[2];
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY:
        *value = effect->directivity;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEWEIGHT:
        *value = effect->dipoleWeight;
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECTIVITY_DIPOLEPOWER:
        *value = effect->dipolePower;
        break;
    case IPL_FMODCORE_SPATIALIZE_OCCLUSION:
        *value = effect->occlusion;
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_LOW:
        *value = effect->transmission[0];
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_MID:
        *value = effect->transmission[1];
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_HIGH:
        *value = effect->transmission[2];
        break;
    case IPL_FMODCORE_SPATIALIZE_DIRECT_MIXLEVEL:
        *value = effect->directMixLevel;
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_MIXLEVEL:
        *value = effect->reflectionsMixLevel;
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_MIXLEVEL:
        *value = effect->pathingMixLevel;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK getInt(FMOD_DSP_STATE* state, int index, int* value, char* valuestr)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_APPLY_DISTANCEATTENUATION:
        *value = effect->applyDistanceAttenuation;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_AIRABSORPTION:
        *value = effect->applyAirAbsorption;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_DIRECTIVITY:
        *value = effect->applyDirectivity;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_OCCLUSION:
        *value = effect->applyOcclusion;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_TRANSMISSION:
        *value = effect->applyTransmission;
        break;
    case IPL_FMODCORE_SPATIALIZE_HRTF_INTERPOLATION:
        *value = effect->hrtfInterpolation;
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCEATTENUATION_ROLLOFFTYPE:
        *value = effect->distanceAttenuationRolloffType;
        break;
    case IPL_FMODCORE_SPATIALIZE_TRANSMISSION_TYPE:
        *value = effect->transmissionType;
        break;
    case IPL_FMODCORE_SPATIALIZE_SIMULATION_OUTPUTS_HANDLE:
        *value = -1; // This is a placeholder, we need to get the handle from the source manager
        break;
    case IPL_FMODCORE_SPATIALIZE_OUTPUT_FORMAT:
        *value = effect->outputFormat;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK getBool(FMOD_DSP_STATE* state, int index, FMOD_BOOL* value, char* valuestr)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_DIRECT_BINAURAL:
        *value = effect->directBinaural;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_REFLECTIONS:
        *value = effect->applyReflections;
        break;
    case IPL_FMODCORE_SPATIALIZE_APPLY_PATHING:
        *value = effect->applyPathing;
        break;
    case IPL_FMODCORE_SPATIALIZE_REFLECTIONS_BINAURAL:
        *value = effect->reflectionsBinaural;
        break;
    case IPL_FMODCORE_SPATIALIZE_PATHING_BINAURAL:
        *value = effect->pathingBinaural;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK getData(FMOD_DSP_STATE* state, int index, void** value, unsigned int* length, char* valuestr)
{
    auto effect = reinterpret_cast<State*>(state->plugindata);
    switch (index)
    {
    case IPL_FMODCORE_SPATIALIZE_SOURCE_POSITION:
        *value = &effect->source;
        *length = sizeof(effect->source);
        break;
    case IPL_FMODCORE_SPATIALIZE_DISTANCE_ATTENUATION_RANGE:
        *value = &effect->attenuationRange;
        *length = sizeof(effect->attenuationRange);
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK process(FMOD_DSP_STATE* dsp, unsigned int length, const FMOD_DSP_BUFFER_ARRAY* inBuffers, FMOD_DSP_BUFFER_ARRAY* outBuffers, FMOD_BOOL inputsIdle, FMOD_DSP_PROCESS_OPERATION op)
{
    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        if (initFmodOutBufferFormat(inBuffers, outBuffers, dsp, reinterpret_cast<State*>(dsp->plugindata)->outputFormat))
            return FMOD_OK;
        else
            return FMOD_ERR_DSP_DONTPROCESS;
    }

    if (inputsIdle)
        return FMOD_OK;

    auto effect = reinterpret_cast<State*>(dsp->plugindata);
    auto context = gContext;
    if (!context)
        return FMOD_ERR_NOTREADY;

    IPLSimulator simulator = nullptr;
    iplSimulatorCreate(context, &gSimulationSettings, &simulator);

    auto hrtf = gHRTF[0];
    if (!hrtf)
        return FMOD_ERR_NOTREADY;

    int numChannelsIn, numChannelsOut;
    FMOD_SPEAKERMODE speakerModeIn, speakerModeOut;
    dsp->functions->getspeakermode(dsp, &speakerModeIn, &speakerModeOut);

    IPLAudioBuffer inBuffer{};
    inBuffer.numSamples = length;
    inBuffer.data = inBuffers->buffers;

    IPLAudioBuffer outBuffer{};
    outBuffer.numSamples = length;
    outBuffer.data = outBuffers->buffers;

    if (op == FMOD_DSP_PROCESS_PERFORM)
    {
        if (effect->newSimulationSourceWritten)
        {
            effect->simulationSource[0] = effect->simulationSource[1];
            effect->newSimulationSourceWritten = false;
        }

        if (effect->simulationSource[0])
        {
            IPLSimulationInputs inputs{};
            inputs.source = calcCoordinates(effect->source.absolute);
            inputs.flags = IPL_SIMULATIONFLAGS_DIRECT;
            iplSourceSetInputs(effect->simulationSource[0], IPL_SIMULATIONFLAGS_DIRECT, &inputs);
        }
    }

    return FMOD_OK;
}

}
}