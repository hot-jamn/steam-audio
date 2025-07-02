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

using UnityEngine;
using System.Collections;

namespace SteamAudio.Examples
{
    /// <summary>
    /// Example script demonstrating Steam Audio FMOD Core integration setup and usage.
    /// This script shows how to programmatically set up and manage FMOD Core audio sources
    /// with Steam Audio spatialization.
    /// </summary>
    public class SteamAudioFMODCoreExample : MonoBehaviour
    {
        [Header("Example Configuration")]
        [Tooltip("Automatically set up the example on Start")]
        public bool autoSetup = true;

        [Tooltip("Create example audio sources")]
        public bool createExampleSources = true;

        [Tooltip("Number of example sources to create")]
        [Range(1, 10)]
        public int numberOfSources = 3;

        [Tooltip("Radius for positioning example sources")]
        public float sourceRadius = 5.0f;

        [Header("Audio Source Settings")]
        [Tooltip("Enable spatialization for example sources")]
        public bool enableSpatialization = true;

        [Tooltip("Enable reverb for example sources")]
        public bool enableReverb = true;

        [Tooltip("Enable reflections for example sources")]
        public bool enableReflections = true;

        [Tooltip("Enable pathing for example sources")]
        public bool enablePathing = false;

        [Header("Runtime Controls")]
        [Tooltip("Toggle all sources on/off")]
        public bool sourcesEnabled = true;

        [Tooltip("Master mix level for all sources")]
        [Range(0.0f, 1.0f)]
        public float masterMixLevel = 1.0f;

        // Private members
        private SteamAudioFMODCoreManager mManager;
        private SteamAudioFMODCoreSource[] mExampleSources;
        private bool mLastSourcesEnabled = true;
        private float mLastMasterMixLevel = 1.0f;

        // --------------------------------------------------------------------------------------------------------------------
        // Unity Lifecycle
        // --------------------------------------------------------------------------------------------------------------------

        private void Start()
        {
            if (autoSetup)
            {
                StartCoroutine(SetupExample());
            }
        }

        private void Update()
        {
            // Handle runtime control changes
            if (sourcesEnabled != mLastSourcesEnabled)
            {
                ToggleAllSources(sourcesEnabled);
                mLastSourcesEnabled = sourcesEnabled;
            }

            if (Mathf.Abs(masterMixLevel - mLastMasterMixLevel) > 0.01f)
            {
                UpdateMasterMixLevel(masterMixLevel);
                mLastMasterMixLevel = masterMixLevel;
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Example Setup
        // --------------------------------------------------------------------------------------------------------------------

        private IEnumerator SetupExample()
        {
            Debug.Log("SteamAudioFMODCoreExample: Setting up FMOD Core integration example...");

            // Step 1: Ensure Steam Audio Manager exists
            yield return StartCoroutine(WaitForSteamAudioManager());

            // Step 2: Create or find FMOD Core Manager
            SetupFMODCoreManager();

            // Step 3: Wait for FMOD Core initialization
            yield return StartCoroutine(WaitForFMODCoreInitialization());

            // Step 4: Create example sources if requested
            if (createExampleSources)
            {
                CreateExampleSources();
            }

            // Step 5: Configure example sources
            ConfigureExampleSources();

            Debug.Log("SteamAudioFMODCoreExample: Setup complete!");
        }

        private IEnumerator WaitForSteamAudioManager()
        {
            Debug.Log("SteamAudioFMODCoreExample: Waiting for Steam Audio Manager...");

            while (SteamAudioManager.Singleton == null)
            {
                yield return new WaitForSeconds(0.1f);
            }

            Debug.Log("SteamAudioFMODCoreExample: Steam Audio Manager found.");
        }

        private void SetupFMODCoreManager()
        {
            Debug.Log("SteamAudioFMODCoreExample: Setting up FMOD Core Manager...");

            mManager = SteamAudioFMODCoreManager.Instance;
            if (mManager == null)
            {
                mManager = SteamAudioFMODCoreManager.EnsureManager();
                Debug.Log("SteamAudioFMODCoreExample: Created new FMOD Core Manager.");
            }
            else
            {
                Debug.Log("SteamAudioFMODCoreExample: Using existing FMOD Core Manager.");
            }

            // Configure manager settings
            mManager.autoInitialize = true;
            mManager.autoUpdateSettings = true;
            mManager.enableDebugLogging = true;
        }

        private IEnumerator WaitForFMODCoreInitialization()
        {
            Debug.Log("SteamAudioFMODCoreExample: Waiting for FMOD Core initialization...");

            // Force initialization if not already done
            if (!SteamAudioFMODCore.IsInitialized)
            {
                mManager.InitializeFMODCore();
            }

            // Wait for initialization to complete
            float timeout = 5.0f;
            float elapsed = 0.0f;

            while (!SteamAudioFMODCore.IsInitialized && elapsed < timeout)
            {
                yield return new WaitForSeconds(0.1f);
                elapsed += 0.1f;
            }

            if (SteamAudioFMODCore.IsInitialized)
            {
                Debug.Log("SteamAudioFMODCoreExample: FMOD Core initialization complete.");
            }
            else
            {
                Debug.LogError("SteamAudioFMODCoreExample: FMOD Core initialization timed out!");
            }
        }

        private void CreateExampleSources()
        {
            Debug.Log($"SteamAudioFMODCoreExample: Creating {numberOfSources} example sources...");

            mExampleSources = new SteamAudioFMODCoreSource[numberOfSources];

            for (int i = 0; i < numberOfSources; i++)
            {
                // Create source GameObject
                var sourceObject = new GameObject($"Example FMOD Core Source {i + 1}");
                sourceObject.transform.parent = transform;

                // Position sources in a circle
                float angle = (float)i / numberOfSources * 2.0f * Mathf.PI;
                var position = new Vector3(
                    Mathf.Cos(angle) * sourceRadius,
                    0.0f,
                    Mathf.Sin(angle) * sourceRadius
                );
                sourceObject.transform.position = transform.position + position;

                // Add required components
                var steamAudioSource = sourceObject.AddComponent<SteamAudioSource>();
                var fmodCoreSource = sourceObject.AddComponent<SteamAudioFMODCoreSource>();

                // Store reference
                mExampleSources[i] = fmodCoreSource;

                Debug.Log($"SteamAudioFMODCoreExample: Created source {i + 1} at position {position}");
            }
        }

        private void ConfigureExampleSources()
        {
            if (mExampleSources == null)
                return;

            Debug.Log("SteamAudioFMODCoreExample: Configuring example sources...");

            foreach (var source in mExampleSources)
            {
                if (source == null)
                    continue;

                // Configure FMOD Core source settings
                source.enableSpatialization = enableSpatialization;
                source.enableReverb = enableReverb;
                source.directMixLevel = masterMixLevel;
                source.reflectionsMixLevel = enableReflections ? masterMixLevel : 0.0f;
                source.pathingMixLevel = enablePathing ? masterMixLevel : 0.0f;

                // Configure Steam Audio source settings
                var steamAudioSource = source.GetSteamAudioSource();
                if (steamAudioSource != null)
                {
                    steamAudioSource.directBinaural = true;
                    steamAudioSource.distanceAttenuation = true;
                    steamAudioSource.airAbsorption = true;
                    steamAudioSource.occlusion = true;
                    steamAudioSource.reflections = enableReflections;
                    steamAudioSource.pathing = enablePathing;
                }
            }

            Debug.Log("SteamAudioFMODCoreExample: Source configuration complete.");
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Runtime Controls
        // --------------------------------------------------------------------------------------------------------------------

        private void ToggleAllSources(bool enabled)
        {
            if (mExampleSources == null)
                return;

            Debug.Log($"SteamAudioFMODCoreExample: {(enabled ? "Enabling" : "Disabling")} all sources.");

            foreach (var source in mExampleSources)
            {
                if (source != null)
                {
                    source.gameObject.SetActive(enabled);
                }
            }
        }

        private void UpdateMasterMixLevel(float level)
        {
            if (mExampleSources == null)
                return;

            foreach (var source in mExampleSources)
            {
                if (source != null)
                {
                    source.directMixLevel = level;
                    source.reflectionsMixLevel = enableReflections ? level : 0.0f;
                    source.pathingMixLevel = enablePathing ? level : 0.0f;
                }
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Public Interface
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Manually trigger example setup.
        /// </summary>
        [ContextMenu("Setup Example")]
        public void SetupExampleManually()
        {
            StartCoroutine(SetupExample());
        }

        /// <summary>
        /// Refresh all example sources.
        /// </summary>
        [ContextMenu("Refresh Sources")]
        public void RefreshSources()
        {
            if (mManager != null)
            {
                mManager.RefreshAllSources();
            }
        }

        /// <summary>
        /// Get the FMOD Core manager instance.
        /// </summary>
        public SteamAudioFMODCoreManager GetManager()
        {
            return mManager;
        }

        /// <summary>
        /// Get all example sources.
        /// </summary>
        public SteamAudioFMODCoreSource[] GetExampleSources()
        {
            return mExampleSources;
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Debug and Monitoring
        // --------------------------------------------------------------------------------------------------------------------

        private void OnDrawGizmosSelected()
        {
            // Draw source positions
            if (mExampleSources != null)
            {
                Gizmos.color = Color.cyan;
                foreach (var source in mExampleSources)
                {
                    if (source != null)
                    {
                        Gizmos.DrawWireSphere(source.transform.position, 0.5f);
                        Gizmos.DrawLine(transform.position, source.transform.position);
                    }
                }
            }

            // Draw source radius
            Gizmos.color = Color.yellow;
            Gizmos.DrawWireCircle(transform.position, sourceRadius);
        }

        private void OnGUI()
        {
            if (!Application.isPlaying)
                return;

            // Display status information
            GUILayout.BeginArea(new Rect(10, 10, 300, 200));
            GUILayout.BeginVertical(GUI.skin.box);

            GUILayout.Label("Steam Audio FMOD Core Example", GUI.skin.label);
            GUILayout.Space(5);

            GUILayout.Label($"Steam Audio Manager: {(SteamAudioManager.Singleton != null ? "Ready" : "Not Ready")}");
            GUILayout.Label($"FMOD Core Initialized: {SteamAudioFMODCore.IsInitialized}");

            if (mManager != null)
            {
                GUILayout.Label($"Registered Sources: {mManager.GetRegisteredSourceCount()}");
            }

            if (mExampleSources != null)
            {
                int activeCount = 0;
                foreach (var source in mExampleSources)
                {
                    if (source != null && source.gameObject.activeInHierarchy)
                        activeCount++;
                }
                GUILayout.Label($"Active Example Sources: {activeCount}/{mExampleSources.Length}");
            }

            GUILayout.EndVertical();
            GUILayout.EndArea();
        }
    }
}