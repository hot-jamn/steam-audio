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

#if UNITY_EDITOR
using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    /// <summary>
    /// Custom editor for SteamAudioFMODCoreSource component.
    /// </summary>
    [CustomEditor(typeof(SteamAudioFMODCoreSource))]
    public class SteamAudioFMODCoreSourceEditor : Editor
    {
        private SerializedProperty enableSpatialization;
        private SerializedProperty enableReverb;
        private SerializedProperty enableMixerReturn;
        private SerializedProperty sourceId;
        private SerializedProperty applyHRTFToDirect;
        private SerializedProperty applyHRTFToReflections;
        private SerializedProperty applyHRTFToPathing;
        private SerializedProperty directMixLevel;
        private SerializedProperty reflectionsMixLevel;
        private SerializedProperty pathingMixLevel;

        private void OnEnable()
        {
            enableSpatialization = serializedObject.FindProperty("enableSpatialization");
            enableReverb = serializedObject.FindProperty("enableReverb");
            enableMixerReturn = serializedObject.FindProperty("enableMixerReturn");
            sourceId = serializedObject.FindProperty("sourceId");
            applyHRTFToDirect = serializedObject.FindProperty("applyHRTFToDirect");
            applyHRTFToReflections = serializedObject.FindProperty("applyHRTFToReflections");
            applyHRTFToPathing = serializedObject.FindProperty("applyHRTFToPathing");
            directMixLevel = serializedObject.FindProperty("directMixLevel");
            reflectionsMixLevel = serializedObject.FindProperty("reflectionsMixLevel");
            pathingMixLevel = serializedObject.FindProperty("pathingMixLevel");
        }

        public override void OnInspectorGUI()
        {
            var source = target as SteamAudioFMODCoreSource;
            serializedObject.Update();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Steam Audio FMOD Core Source", EditorStyles.boldLabel);
            EditorGUILayout.Space();

            // Status information
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.LabelField("Status", EditorStyles.boldLabel);
            
            var statusColor = source.IsRegistered ? Color.green : Color.red;
            var statusText = source.IsRegistered ? "Registered" : "Not Registered";
            
            var oldColor = GUI.color;
            GUI.color = statusColor;
            EditorGUILayout.LabelField("Registration Status:", statusText);
            GUI.color = oldColor;

            if (source.IsRegistered)
            {
                EditorGUILayout.LabelField("Source ID:", source.GetSourceId().ToString());
            }

            EditorGUILayout.LabelField("FMOD Core Initialized:", SteamAudioFMODCore.IsInitialized ? "Yes" : "No");
            EditorGUILayout.EndVertical();

            EditorGUILayout.Space();

            // FMOD Core Integration settings
            EditorGUILayout.LabelField("FMOD Core Integration", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(enableSpatialization, new GUIContent("Enable Spatialization", "Enable Steam Audio spatialization DSP effect"));
            EditorGUILayout.PropertyField(enableReverb, new GUIContent("Enable Reverb", "Enable Steam Audio reverb DSP effect"));
            EditorGUILayout.PropertyField(enableMixerReturn, new GUIContent("Enable Mixer Return", "Enable Steam Audio mixer return DSP effect"));

            EditorGUILayout.Space();

            // HRTF settings
            EditorGUILayout.LabelField("HRTF Settings", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(applyHRTFToDirect, new GUIContent("Apply HRTF to Direct", "Apply HRTF processing to direct sound"));
            EditorGUILayout.PropertyField(applyHRTFToReflections, new GUIContent("Apply HRTF to Reflections", "Apply HRTF processing to reflections"));
            EditorGUILayout.PropertyField(applyHRTFToPathing, new GUIContent("Apply HRTF to Pathing", "Apply HRTF processing to pathing"));

            EditorGUILayout.Space();

            // Mix levels
            EditorGUILayout.LabelField("Mix Levels", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(directMixLevel, new GUIContent("Direct Mix Level", "Mix level for direct sound"));
            EditorGUILayout.PropertyField(reflectionsMixLevel, new GUIContent("Reflections Mix Level", "Mix level for reflections"));
            EditorGUILayout.PropertyField(pathingMixLevel, new GUIContent("Pathing Mix Level", "Mix level for pathing"));

            EditorGUILayout.Space();

            // Control buttons
            EditorGUILayout.BeginHorizontal();
            
            if (GUILayout.Button("Refresh Registration"))
            {
                source.RefreshRegistration();
            }

            GUI.enabled = !SteamAudioFMODCore.IsInitialized;
            if (GUILayout.Button("Initialize FMOD Core"))
            {
                SteamAudioFMODCore.Initialize();
            }
            GUI.enabled = true;

            EditorGUILayout.EndHorizontal();

            // Warnings and help
            if (!source.IsRegistered && Application.isPlaying)
            {
                EditorGUILayout.HelpBox("Source is not registered with FMOD Core integration. Make sure SteamAudioFMODCore is initialized.", MessageType.Warning);
            }

            if (!SteamAudioFMODCore.IsInitialized && Application.isPlaying)
            {
                EditorGUILayout.HelpBox("FMOD Core integration is not initialized. Click 'Initialize FMOD Core' or ensure SteamAudioFMODCoreManager is in the scene.", MessageType.Warning);
            }

            var steamAudioSource = source.GetSteamAudioSource();
            if (steamAudioSource == null)
            {
                EditorGUILayout.HelpBox("SteamAudioSource component is required for FMOD Core integration.", MessageType.Error);
            }

            serializedObject.ApplyModifiedProperties();
        }
    }

    /// <summary>
    /// Custom editor for SteamAudioFMODCoreManager component.
    /// </summary>
    [CustomEditor(typeof(SteamAudioFMODCoreManager))]
    public class SteamAudioFMODCoreManagerEditor : Editor
    {
        private SerializedProperty autoInitialize;
        private SerializedProperty autoUpdateSettings;
        private SerializedProperty updateInterval;
        private SerializedProperty enableDebugLogging;
        private SerializedProperty registeredSourceCount;

        private void OnEnable()
        {
            autoInitialize = serializedObject.FindProperty("autoInitialize");
            autoUpdateSettings = serializedObject.FindProperty("autoUpdateSettings");
            updateInterval = serializedObject.FindProperty("updateInterval");
            enableDebugLogging = serializedObject.FindProperty("enableDebugLogging");
            registeredSourceCount = serializedObject.FindProperty("registeredSourceCount");
        }

        public override void OnInspectorGUI()
        {
            var manager = target as SteamAudioFMODCoreManager;
            serializedObject.Update();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Steam Audio FMOD Core Manager", EditorStyles.boldLabel);
            EditorGUILayout.Space();

            // Status information
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.LabelField("Status", EditorStyles.boldLabel);
            
            var initStatusColor = SteamAudioFMODCore.IsInitialized ? Color.green : Color.red;
            var initStatusText = SteamAudioFMODCore.IsInitialized ? "Initialized" : "Not Initialized";
            
            var oldColor = GUI.color;
            GUI.color = initStatusColor;
            EditorGUILayout.LabelField("FMOD Core Status:", initStatusText);
            GUI.color = oldColor;

            EditorGUILayout.LabelField("Registered Sources:", registeredSourceCount.intValue.ToString());
            EditorGUILayout.LabelField("Steam Audio Manager:", SteamAudioManager.Singleton != null ? "Available" : "Not Available");
            EditorGUILayout.EndVertical();

            EditorGUILayout.Space();

            // Settings
            EditorGUILayout.LabelField("Integration Settings", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(autoInitialize, new GUIContent("Auto Initialize", "Automatically initialize FMOD Core integration on Start"));
            EditorGUILayout.PropertyField(autoUpdateSettings, new GUIContent("Auto Update Settings", "Automatically update FMOD Core settings when Steam Audio settings change"));
            
            if (autoUpdateSettings.boolValue)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(updateInterval, new GUIContent("Update Interval", "How often to check for settings updates (seconds)"));
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.Space();

            // Debug settings
            EditorGUILayout.LabelField("Debug", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(enableDebugLogging, new GUIContent("Enable Debug Logging", "Enable debug logging for FMOD Core integration"));

            EditorGUILayout.Space();

            // Control buttons
            EditorGUILayout.LabelField("Controls", EditorStyles.boldLabel);
            
            EditorGUILayout.BeginHorizontal();
            
            GUI.enabled = !SteamAudioFMODCore.IsInitialized;
            if (GUILayout.Button("Initialize"))
            {
                manager.ForceInitialize();
            }
            GUI.enabled = true;

            if (GUILayout.Button("Update Settings"))
            {
                manager.ForceUpdateSettings();
            }

            if (GUILayout.Button("Refresh Sources"))
            {
                manager.ForceRefreshSources();
            }

            EditorGUILayout.EndHorizontal();

            // Source list
            if (Application.isPlaying && manager.GetRegisteredSourceCount() > 0)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Registered Sources", EditorStyles.boldLabel);
                
                var sources = manager.GetRegisteredSources();
                foreach (var source in sources)
                {
                    if (source != null)
                    {
                        EditorGUILayout.BeginHorizontal();
                        EditorGUILayout.LabelField(source.name);
                        EditorGUILayout.LabelField($"ID: {source.GetSourceId()}", GUILayout.Width(60));
                        if (GUILayout.Button("Select", GUILayout.Width(60)))
                        {
                            Selection.activeGameObject = source.gameObject;
                        }
                        EditorGUILayout.EndHorizontal();
                    }
                }
            }

            // Warnings and help
            if (!SteamAudioFMODCore.IsInitialized && Application.isPlaying)
            {
                EditorGUILayout.HelpBox("FMOD Core integration is not initialized. Click 'Initialize' to set up the integration.", MessageType.Warning);
            }

            if (SteamAudioManager.Singleton == null && Application.isPlaying)
            {
                EditorGUILayout.HelpBox("SteamAudioManager is not available. Make sure Steam Audio is properly set up in the scene.", MessageType.Error);
            }

            serializedObject.ApplyModifiedProperties();
        }
    }

    /// <summary>
    /// Menu items for Steam Audio FMOD Core integration.
    /// </summary>
    public static class SteamAudioFMODCoreMenu
    {
        [MenuItem("Steam Audio/FMOD Core/Create Manager", false, 100)]
        public static void CreateManager()
        {
            if (SteamAudioFMODCoreManager.HasInstance)
            {
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    "A FMOD Core Manager already exists in the scene.", "OK");
                Selection.activeGameObject = SteamAudioFMODCoreManager.Instance.gameObject;
                return;
            }

            var managerObject = new GameObject("Steam Audio FMOD Core Manager");
            managerObject.AddComponent<SteamAudioFMODCoreManager>();
            
            Selection.activeGameObject = managerObject;
            
            EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                "FMOD Core Manager created successfully. Configure the settings in the Inspector.", "OK");
        }

        [MenuItem("Steam Audio/FMOD Core/Add Source Component", false, 101)]
        public static void AddSourceComponent()
        {
            var selectedObjects = Selection.gameObjects;
            if (selectedObjects.Length == 0)
            {
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    "Please select one or more GameObjects to add the FMOD Core Source component.", "OK");
                return;
            }

            int addedCount = 0;
            foreach (var obj in selectedObjects)
            {
                if (obj.GetComponent<SteamAudioSource>() == null)
                {
                    obj.AddComponent<SteamAudioSource>();
                }

                if (obj.GetComponent<SteamAudioFMODCoreSource>() == null)
                {
                    obj.AddComponent<SteamAudioFMODCoreSource>();
                    addedCount++;
                }
            }

            EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                $"Added FMOD Core Source component to {addedCount} GameObject(s).", "OK");
        }

        [MenuItem("Steam Audio/FMOD Core/Initialize Integration", false, 102)]
        public static void InitializeIntegration()
        {
            if (!Application.isPlaying)
            {
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    "FMOD Core integration can only be initialized during Play mode.", "OK");
                return;
            }

            if (SteamAudioFMODCore.IsInitialized)
            {
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    "FMOD Core integration is already initialized.", "OK");
                return;
            }

            try
            {
                SteamAudioFMODCore.Initialize();
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    "FMOD Core integration initialized successfully.", "OK");
            }
            catch (System.Exception e)
            {
                EditorUtility.DisplayDialog("Steam Audio FMOD Core", 
                    $"Failed to initialize FMOD Core integration: {e.Message}", "OK");
            }
        }
    }
}
#endif