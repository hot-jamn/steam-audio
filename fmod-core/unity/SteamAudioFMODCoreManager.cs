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

using System.Collections.Generic;
using UnityEngine;

namespace SteamAudio
{
    /// <summary>
    /// Manager component for Steam Audio FMOD Core integration.
    /// Handles initialization, source management, and settings updates.
    /// </summary>
    [AddComponentMenu("Steam Audio/Steam Audio FMOD Core Manager")]
    public class SteamAudioFMODCoreManager : MonoBehaviour
    {
        [Header("Integration Settings")]
        [Tooltip("Automatically initialize FMOD Core integration on Start")]
        public bool autoInitialize = true;

        [Tooltip("Update FMOD Core settings when Steam Audio settings change")]
        public bool autoUpdateSettings = true;

        [Tooltip("Update interval for settings synchronization (seconds)")]
        [Range(0.1f, 5.0f)]
        public float updateInterval = 1.0f;

        [Header("Debug")]
        [Tooltip("Enable debug logging for FMOD Core integration")]
        public bool enableDebugLogging = false;

        [Tooltip("Show source count in inspector")]
        [SerializeField]
        private int registeredSourceCount = 0;

        // Private members
        private static SteamAudioFMODCoreManager sInstance = null;
        private List<SteamAudioFMODCoreSource> mRegisteredSources = new List<SteamAudioFMODCoreSource>();
        private float mLastUpdateTime = 0.0f;
        private bool mWasInitialized = false;

        // --------------------------------------------------------------------------------------------------------------------
        // Singleton Access
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Get the singleton instance of the FMOD Core manager.
        /// </summary>
        public static SteamAudioFMODCoreManager Instance
        {
            get { return sInstance; }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Unity Lifecycle
        // --------------------------------------------------------------------------------------------------------------------

        private void Awake()
        {
            // Ensure singleton
            if (sInstance != null && sInstance != this)
            {
                Debug.LogWarning("SteamAudioFMODCoreManager: Multiple instances detected. Destroying duplicate.");
                Destroy(gameObject);
                return;
            }

            sInstance = this;
            DontDestroyOnLoad(gameObject);

            if (enableDebugLogging)
            {
                Debug.Log("SteamAudioFMODCoreManager: Awake - Manager created.");
            }
        }

        private void Start()
        {
            if (autoInitialize)
            {
                InitializeFMODCore();
            }
        }

        private void Update()
        {
            // Update settings periodically if auto-update is enabled
            if (autoUpdateSettings && Time.time - mLastUpdateTime >= updateInterval)
            {
                UpdateSettingsIfNeeded();
                mLastUpdateTime = Time.time;
            }

            // Update registered source count for inspector
            registeredSourceCount = mRegisteredSources.Count;
        }

        private void OnDestroy()
        {
            if (sInstance == this)
            {
                sInstance = null;
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Initialization
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Initialize FMOD Core integration.
        /// </summary>
        public void InitializeFMODCore()
        {
            if (SteamAudioFMODCore.IsInitialized)
            {
                if (enableDebugLogging)
                {
                    Debug.Log("SteamAudioFMODCoreManager: FMOD Core integration already initialized.");
                }
                return;
            }

            // Wait for SteamAudioManager to be ready
            if (SteamAudioManager.Singleton == null)
            {
                if (enableDebugLogging)
                {
                    Debug.LogWarning("SteamAudioFMODCoreManager: SteamAudioManager not ready. Will retry initialization.");
                }
                Invoke(nameof(InitializeFMODCore), 0.1f); // Retry in 100ms
                return;
            }

            try
            {
                SteamAudioFMODCore.Initialize();
                mWasInitialized = true;

                if (enableDebugLogging)
                {
                    Debug.Log("SteamAudioFMODCoreManager: FMOD Core integration initialized successfully.");
                }

                // Refresh all registered sources
                RefreshAllSources();
            }
            catch (System.Exception e)
            {
                Debug.LogError($"SteamAudioFMODCoreManager: Failed to initialize FMOD Core integration: {e.Message}");
            }
        }

        /// <summary>
        /// Update FMOD Core settings if Steam Audio settings have changed.
        /// </summary>
        public void UpdateSettingsIfNeeded()
        {
            if (!SteamAudioFMODCore.IsInitialized)
                return;

            // Check if SteamAudioManager is still available
            if (SteamAudioManager.Singleton == null)
            {
                if (mWasInitialized && enableDebugLogging)
                {
                    Debug.LogWarning("SteamAudioFMODCoreManager: SteamAudioManager became unavailable.");
                }
                return;
            }

            try
            {
                SteamAudioFMODCore.UpdateSettings();

                if (enableDebugLogging)
                {
                    Debug.Log("SteamAudioFMODCoreManager: Updated FMOD Core settings.");
                }
            }
            catch (System.Exception e)
            {
                Debug.LogError($"SteamAudioFMODCoreManager: Failed to update FMOD Core settings: {e.Message}");
            }
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Source Management
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Register a FMOD Core source with the manager.
        /// </summary>
        public void RegisterSource(SteamAudioFMODCoreSource source)
        {
            if (source == null || mRegisteredSources.Contains(source))
                return;

            mRegisteredSources.Add(source);

            if (enableDebugLogging)
            {
                Debug.Log($"SteamAudioFMODCoreManager: Registered source '{source.name}'. Total sources: {mRegisteredSources.Count}");
            }
        }

        /// <summary>
        /// Unregister a FMOD Core source from the manager.
        /// </summary>
        public void UnregisterSource(SteamAudioFMODCoreSource source)
        {
            if (source == null)
                return;

            mRegisteredSources.Remove(source);

            if (enableDebugLogging)
            {
                Debug.Log($"SteamAudioFMODCoreManager: Unregistered source '{source.name}'. Total sources: {mRegisteredSources.Count}");
            }
        }

        /// <summary>
        /// Refresh all registered sources (re-register them with FMOD Core).
        /// </summary>
        public void RefreshAllSources()
        {
            if (!SteamAudioFMODCore.IsInitialized)
                return;

            foreach (var source in mRegisteredSources)
            {
                if (source != null && source.gameObject.activeInHierarchy)
                {
                    source.RefreshRegistration();
                }
            }

            if (enableDebugLogging)
            {
                Debug.Log($"SteamAudioFMODCoreManager: Refreshed {mRegisteredSources.Count} sources.");
            }
        }

        /// <summary>
        /// Get the number of registered sources.
        /// </summary>
        public int GetRegisteredSourceCount()
        {
            return mRegisteredSources.Count;
        }

        /// <summary>
        /// Get all registered sources.
        /// </summary>
        public List<SteamAudioFMODCoreSource> GetRegisteredSources()
        {
            return new List<SteamAudioFMODCoreSource>(mRegisteredSources);
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Public Interface
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Force initialization of FMOD Core integration.
        /// </summary>
        [ContextMenu("Initialize FMOD Core")]
        public void ForceInitialize()
        {
            InitializeFMODCore();
        }

        /// <summary>
        /// Force update of FMOD Core settings.
        /// </summary>
        [ContextMenu("Update Settings")]
        public void ForceUpdateSettings()
        {
            UpdateSettingsIfNeeded();
        }

        /// <summary>
        /// Force refresh of all sources.
        /// </summary>
        [ContextMenu("Refresh All Sources")]
        public void ForceRefreshSources()
        {
            RefreshAllSources();
        }

        // --------------------------------------------------------------------------------------------------------------------
        // Static Utility
        // --------------------------------------------------------------------------------------------------------------------

        /// <summary>
        /// Ensure a FMOD Core manager exists in the scene.
        /// Creates one if it doesn't exist.
        /// </summary>
        public static SteamAudioFMODCoreManager EnsureManager()
        {
            if (sInstance != null)
                return sInstance;

            var managerObject = new GameObject("Steam Audio FMOD Core Manager");
            var manager = managerObject.AddComponent<SteamAudioFMODCoreManager>();
            
            Debug.Log("SteamAudioFMODCoreManager: Created manager automatically.");
            
            return manager;
        }

        /// <summary>
        /// Check if a manager instance exists.
        /// </summary>
        public static bool HasInstance
        {
            get { return sInstance != null; }
        }
    }
}