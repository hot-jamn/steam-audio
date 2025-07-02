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
using UnityEngine;

namespace SteamAudio
{
    /// <summary>
    /// FMOD Core audio source component with Steam Audio integration.
    /// This component manages the connection between Unity's SteamAudioSource and FMOD Core DSP effects.
    /// </summary>
    [AddComponentMenu("Steam Audio/Steam Audio FMOD Core Source")]
    [RequireComponent(typeof(SteamAudioSource))]
    public class SteamAudioFMODCoreSource : MonoBehaviour
    {
        [Header("FMOD Core Integration")]
        [Tooltip("Enable Steam Audio spatialization for this FMOD Core source")]
        public bool enableSpatialization = true;

        [Tooltip("Enable Steam Audio reverb processing for this FMOD Core source")]
        public bool enableReverb = true;

        [Tooltip("Enable Steam Audio mixer return processing for this FMOD Core source")]
        public bool enableMixerReturn = false;

        [Header("DSP Parameters")]
        [Tooltip("Source ID for FMOD Core DSP parameter control")]
        [SerializeField]
        private int sourceId = -1;

        [Tooltip("Apply HRTF processing to direct sound")]
        public bool applyHRTFToDirect = true;

        [Tooltip("Apply HRTF processing to reflections")]
        public bool applyHRTFToReflections = true;

        [Tooltip("Apply HRTF processing to pathing")]
        public bool applyHRTFToPathing = true;

        [Header("Mix Levels")]
        [Range(0.0f, 1.0f)]
        [Tooltip("Direct sound mix level")]
        public float directMixLevel = 1.0f;

        [Range(0.0f, 1.0f)]
        [Tooltip("Reflections mix level")]
        public float reflectionsMixLevel = 1.0f;

        [Range(0.0f, 1.0f)]
        [Tooltip("Pathing mix level")]
        public float pathingMixLevel = 1.0f;

        // Private members
        private SteamAudioSource mSteamAudioSource;
        private bool mIsRegistered = false;

        // --------------------------------------------------------------------------------------------------------------------
        // Unity Lifecycle
        // --------------------------------------------------------------------------------------------------------------------

        private void Awake()
        {
            mSteamAudioSource = GetComponent<SteamAudioSource>();
            if (mSteamAudioSource == null)
            {
                Debug.LogError("SteamAudioFMODCoreSource: SteamAudioSource component is required.");
                enabled = false;
                return;
            }
        }

        private void Start()
        {
            // Initialize FMOD Core integration if not already done
            if (!SteamAudioFMODCore.IsInitialized)
            {
                SteamAudioFMODCore.Initialize();
            }

            RegisterSource();
        }

        private void OnEnable()
        {
            if (mSteamAudioSource != null && SteamAudioFMODCore.IsInitialized)
            {
                RegisterSource();
            }
        }

        private void OnDisable()
        {
            UnregisterSource();
        }

        private void OnDestroy()
        {
            UnregisterSource();
        }

        private void Update()
        {
            // Update parameters if needed
            if (mIsRegistered && sourceId >= 0)
            {
                UpdateDSPParameters();
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Source Management
        // --------------------------------------------------------------------------------------------------------------------

        private void RegisterSource()
        {
            if (mIsRegistered || mSteamAudioSource == null)
                return;

            if (!SteamAudioFMODCore.IsInitialized)
            {
                Debug.LogWarning("SteamAudioFMODCoreSource: FMOD Core integration not initialized. Call SteamAudioFMODCore.Initialize() first.");
                return;
            }

            sourceId = SteamAudioFMODCore.AddSource(mSteamAudioSource);
            if (sourceId >= 0)
            {
                mIsRegistered = true;
                Debug.Log($"SteamAudioFMODCoreSource: Registered source with ID {sourceId}");
            }
            else
            {
                Debug.LogError("SteamAudioFMODCoreSource: Failed to register source with FMOD Core integration.");
            }
        }

        private void UnregisterSource()
        {
            if (!mIsRegistered || sourceId < 0)
                return;

            SteamAudioFMODCore.RemoveSource(sourceId);
            sourceId = -1;
            mIsRegistered = false;
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Parameter Management
        // --------------------------------------------------------------------------------------------------------------------

        private void UpdateDSPParameters()
        {
            // This method would be used to update FMOD Core DSP parameters
            // In a real implementation, you would use FMOD Core API to set parameters
            // based on the current Steam Audio simulation results and component settings
            
            // Example parameter updates (would require FMOD Core API integration):
            // - Set source position and orientation
            // - Update mix levels
            // - Configure HRTF settings
            // - Apply simulation results (occlusion, reverb, etc.)
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Public Interface
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Get the FMOD Core source ID for this component.
        /// </summary>
        public int GetSourceId()
        {
            return sourceId;
        }

        /// <summary>
        /// Check if this source is registered with FMOD Core integration.
        /// </summary>
        public bool IsRegistered
        {
            get { return mIsRegistered; }
        }

        /// <summary>
        /// Get the associated SteamAudioSource component.
        /// </summary>
        public SteamAudioSource GetSteamAudioSource()
        {
            return mSteamAudioSource;
        }

        /// <summary>
        /// Force re-registration of this source with FMOD Core integration.
        /// Useful when Steam Audio settings change.
        /// </summary>
        public void RefreshRegistration()
        {
            UnregisterSource();
            RegisterSource();
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Inspector Validation
        // --------------------------------------------------------------------------------------------------------------------

        private void OnValidate()
        {
            // Clamp values to valid ranges
            directMixLevel = Mathf.Clamp01(directMixLevel);
            reflectionsMixLevel = Mathf.Clamp01(reflectionsMixLevel);
            pathingMixLevel = Mathf.Clamp01(pathingMixLevel);
        }
    }
}