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

    extern FMOD_DSP_DESCRIPTION gSpatializeEffect;
    extern FMOD_DSP_DESCRIPTION gMixerReturnEffect;
    extern FMOD_DSP_DESCRIPTION gReverbEffect;

    static FMOD_PLUGINLIST gPluginList[] =
    {
        { FMOD_PLUGINTYPE_DSP, &gSpatializeEffect },
        { FMOD_PLUGINTYPE_DSP, &gMixerReturnEffect },
        { FMOD_PLUGINTYPE_DSP, &gReverbEffect },
        { FMOD_PLUGINTYPE_MAX, nullptr }
    };

    namespace SpatializeEffect { extern void initSpatializeParameterDescs(); }
    namespace MixerReturnEffect { extern void initMixerReturnParameterDescs(); }
    namespace ReverbEffect { extern void initReverbParameterDescs(); }
}

extern "C" {

F_EXPORT FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList()
{
    SteamAudioFMODCore::SpatializeEffect::initSpatializeParameterDescs();
    SteamAudioFMODCore::MixerReturnEffect::initMixerReturnParameterDescs();
    SteamAudioFMODCore::ReverbEffect::initReverbParameterDescs();
    return SteamAudioFMODCore::gPluginList;
}

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_Spatialize_GetDSPDescription()
{
    SteamAudioFMODCore::SpatializeEffect::initSpatializeParameterDescs();
    return &SteamAudioFMODCore::gSpatializeEffect;
}

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_MixerReturn_GetDSPDescription()
{
    SteamAudioFMODCore::MixerReturnEffect::initMixerReturnParameterDescs();
    return &SteamAudioFMODCore::gMixerReturnEffect;
}

F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMOD_SteamAudio_Reverb_GetDSPDescription()
{
    SteamAudioFMODCore::ReverbEffect::initReverbParameterDescs();
    return &SteamAudioFMODCore::gReverbEffect;
}

}
