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

using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace SteamAudio
{
    /// <summary>
    /// FMOD Core integration for Steam Audio.
    /// Provides Unity integration for FMOD Core DSP effects with Steam Audio simulation.
    /// </summary>
    public static class SteamAudioFMODCore
    {
        // --------------------------------------------------------------------------------------------------------------------
        // Native Plugin Interface
        // --------------------------------------------------------------------------------------------------------------------

        [DllImport("steamaudio_unity_fmodcore")]
        public static extern void iplUnitySetContext(IntPtr context);

        [DllImport("steamaudio_unity_fmodcore")]
        public static extern void iplUnitySetHRTF(IntPtr hrtf);

        [DllImport("steamaudio_unity_fmodcore")]
        public static extern void iplUnitySetSimulationSettings(SimulationSettings simulationSettings);

        [DllImport("steamaudio_unity_fmodcore")]
        public static extern int iplUnityAddSource(IntPtr source);

        [DllImport("steamaudio_unity_fmodcore")]
        public static extern void iplUnityRemoveSource(int sourceId);

        // --------------------------------------------------------------------------------------------------------------------
        // Initialization
        // --------------------------------------------------------------------------------------------------------------------

        private static bool sInitialized = false;

        /// <summary>
        /// Initialize FMOD Core integration with Steam Audio context.
        /// This should be called after SteamAudioManager is initialized.
        /// </summary>
        public static void Initialize()
        {
            if (sInitialized)
                return;

            if (SteamAudioManager.Singleton == null)
            {
                Debug.LogError("SteamAudioFMODCore: SteamAudioManager must be initialized before FMOD Core integration.");
                return;
            }

            // Share Steam Audio context with FMOD Core plugin
            iplUnitySetContext(SteamAudioManager.Context.Get());
            iplUnitySetHRTF(SteamAudioManager.CurrentHRTF.Get());

            // Set simulation settings
            var simulationSettings = SteamAudioManager.GetSimulationSettings(false);
            iplUnitySetSimulationSettings(simulationSettings);

            sInitialized = true;
            Debug.Log("SteamAudioFMODCore: Initialized successfully.");
        }

        /// <summary>
        /// Update FMOD Core integration settings.
        /// Call this when Steam Audio settings change.
        /// </summary>
        public static void UpdateSettings()
        {
            if (!sInitialized)
                return;

            iplUnitySetHRTF(SteamAudioManager.CurrentHRTF.Get());

            var simulationSettings = SteamAudioManager.GetSimulationSettings(false);
            iplUnitySetSimulationSettings(simulationSettings);
        }

        /// <summary>
        /// Add a Steam Audio source to the FMOD Core integration.
        /// Returns a source ID that can be used with FMOD Core DSP parameters.
        /// </summary>
        public static int AddSource(SteamAudioSource steamAudioSource)
        {
            if (!sInitialized)
            {
                Debug.LogError("SteamAudioFMODCore: Must call Initialize() before adding sources.");
                return -1;
            }

            if (steamAudioSource == null || steamAudioSource.GetSource() == null)
            {
                Debug.LogError("SteamAudioFMODCore: Invalid SteamAudioSource.");
                return -1;
            }

            return iplUnityAddSource(steamAudioSource.GetSource().Get());
        }

        /// <summary>
        /// Remove a Steam Audio source from the FMOD Core integration.
        /// </summary>
        public static void RemoveSource(int sourceId)
        {
            if (!sInitialized)
                return;

            if (sourceId >= 0)
            {
                iplUnityRemoveSource(sourceId);
            }
        }

        /// <summary>
        /// Check if FMOD Core integration is initialized.
        /// </summary>
        public static bool IsInitialized
        {
            get { return sInitialized; }
        }
    }
}