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
using UnityEngine.Windows;
using UnityEditor;

namespace SteamAudio
{
    public enum ReverbType
    {
        Realtime,
        Baked
    }
    

    [AddComponentMenu("Steam Audio/Steam Audio Listener")]
    public class SteamAudioListener : MonoBehaviour
    {
        [Header("Baked Static Listener Settings")]
        public SteamAudioBakedListener currentBakedListener = null;

        [Header("Reverb Settings")]
        public bool applyReverb = false;
        public ReverbType reverbType = ReverbType.Realtime;

        [Header("Baked Reverb Settings")]
        public bool useAllProbeBatches = false;
        public SteamAudioProbeBatch[] probeBatches = null;

        [SerializeField]
        int mTotalDataSize = 0;
        [SerializeField]
        int[] mProbeDataSizes = null;
        [SerializeField]
        BakedDataIdentifier mIdentifier = new BakedDataIdentifier { };
        [SerializeField]
        SteamAudioProbeBatch[] mProbeBatchesUsed = null;

        public string bakePath = "output.wav";
        private string BakePath => Application.dataPath + "/" + bakePath;

#if STEAMAUDIO_ENABLED
        Simulator mSimulator = null;
        Source mSource = null;

        public int GetTotalDataSize()
        {
            return mTotalDataSize;
        }

        public int[] GetProbeDataSizes()
        {
            return mProbeDataSizes;
        }

        public int GetSizeForProbeBatch(int index)
        {
            return mProbeDataSizes[index];
        }

        public SteamAudioProbeBatch[] GetProbeBatchesUsed()
        {
            if (mProbeBatchesUsed == null)
            {
                CacheProbeBatchesUsed();
            }

            return mProbeBatchesUsed;
        }

        private void Awake()
        {
            Reinitialize();
        }

        public void Reinitialize()
        {
            mSimulator = SteamAudioManager.Simulator;

            var settings = SteamAudioManager.GetSimulationSettings(false);
            mSource = new Source(SteamAudioManager.Simulator, settings);

            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }
        
        // [Button]
        public void BakeIR()
        {
            SetInputs(SimulationFlags.Reflections);
            var outputs = mSource.GetOutputs(SimulationFlags.Reflections);
            var rawData = new byte[outputs.reflections.irSize];
            Marshal.Copy(outputs.reflections.ir, rawData, 0, outputs.reflections.irSize);

            // WAV properties
            var sampleRate = SteamAudioManager.AudioSettings.samplingRate; // Choose appropriate rate, e.g., 44100 Hz
            var bitsPerSample = SteamAudioManager.AudioSettings.frameSize / outputs.reflections.numChannels / 8; // Choose appropriate size, e.g., 16-bit
            var channels = outputs.reflections.numChannels;
            
            Debug.Log($"Sample Rate: {sampleRate} Bits Per Sample: {bitsPerSample} Channels: {channels}");
            
            var wavData = AddWavHeader(rawData, sampleRate, bitsPerSample, channels);
            File.WriteAllBytes(BakePath, wavData);

            Debug.Log("Baked IR saved as WAV.");
        }

        private byte[] AddWavHeader(byte[] audioData, int sampleRate, int bitsPerSample, int channels)
        {
            var byteRate = sampleRate * channels * (bitsPerSample / 8);
            var blockAlign = channels * (bitsPerSample / 8);

            // WAV Header
            var header = new byte[44];
            Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("RIFF"), 0, header, 0, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(36 + audioData.Length), 0, header, 4, 4); // File size
            Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("WAVE"), 0, header, 8, 4);
            Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("fmt "), 0, header, 12, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(16), 0, header, 16, 4); // Format chunk size
            Buffer.BlockCopy(BitConverter.GetBytes((short)1), 0, header, 20, 2); // Audio format (1 for PCM)
            Buffer.BlockCopy(BitConverter.GetBytes((short)channels), 0, header, 22, 2); // Channels
            Buffer.BlockCopy(BitConverter.GetBytes(sampleRate), 0, header, 24, 4); // Sample rate
            Buffer.BlockCopy(BitConverter.GetBytes(byteRate), 0, header, 28, 4); // Byte rate
            Buffer.BlockCopy(BitConverter.GetBytes((short)blockAlign), 0, header, 32, 2); // Block align
            Buffer.BlockCopy(BitConverter.GetBytes((short)bitsPerSample), 0, header, 34, 2); // Bits per sample
            Buffer.BlockCopy(System.Text.Encoding.ASCII.GetBytes("data"), 0, header, 36, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(audioData.Length), 0, header, 40, 4); // Data chunk size

            // Combine header and data
            var wavData = new byte[header.Length + audioData.Length];
            Buffer.BlockCopy(header, 0, wavData, 0, header.Length);
            Buffer.BlockCopy(audioData, 0, wavData, header.Length, audioData.Length);

            return wavData;
        }

        private void OnDestroy()
        {
            if (mSource != null)
            {
                mSource.Release();
            }
        }

        private void Start()
        {
            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }

        private void OnEnable()
        {
            if (applyReverb)
            {
                mSource.AddToSimulator(mSimulator);
                SteamAudioManager.AddListener(this);
                SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
            }
        }

        private void OnDisable()
        {
            if (applyReverb)
            {
                SteamAudioManager.RemoveListener(this);
                mSource.RemoveFromSimulator(mSimulator);
                SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
            }
        }

        private void Update()
        {
            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }

        public BakedDataIdentifier GetBakedDataIdentifier()
        {
            var identifier = new BakedDataIdentifier { };
            identifier.type = BakedDataType.Reflections;
            identifier.variation = BakedDataVariation.Reverb;
            return identifier;
        }

        public void SetInputs(SimulationFlags flags)
        {
            var inputs = new SimulationInputs { };
            inputs.source.origin = Common.ConvertVector(transform.position);
            inputs.source.ahead = Common.ConvertVector(transform.forward);
            inputs.source.up = Common.ConvertVector(transform.up);
            inputs.source.right = Common.ConvertVector(transform.right);
            inputs.distanceAttenuationModel.type = DistanceAttenuationModelType.Default;
            inputs.airAbsorptionModel.type = AirAbsorptionModelType.Default;
            inputs.reverbScaleLow = 1.0f;
            inputs.reverbScaleMid = 1.0f;
            inputs.reverbScaleHigh = 1.0f;
            inputs.hybridReverbTransitionTime = SteamAudioSettings.Singleton.hybridReverbTransitionTime;
            inputs.hybridReverbOverlapPercent = SteamAudioSettings.Singleton.hybridReverbOverlapPercent / 100.0f;
            inputs.baked = (reverbType != ReverbType.Realtime) ? Bool.True : Bool.False;
            if (reverbType == ReverbType.Baked)
            {
                inputs.bakedDataIdentifier = GetBakedDataIdentifier();
            }

            inputs.flags = 0;
            if (applyReverb)
            {
                inputs.flags = inputs.flags | SimulationFlags.Reflections;
            }

            inputs.directFlags = 0;

            mSource.SetInputs(flags, inputs);
        }

        public void UpdateOutputs(SimulationFlags flags)
        {}

        private void OnDrawGizmosSelected()
        {
            var oldColor = Gizmos.color;
            var oldMatrix = Gizmos.matrix;

            Gizmos.color = Color.magenta;

            if (mProbeBatchesUsed != null)
            {
                foreach (var probeBatch in mProbeBatchesUsed)
                {
                    if (probeBatch == null)
                        continue;

                    Gizmos.matrix = probeBatch.transform.localToWorldMatrix;
                    Gizmos.DrawWireCube(new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(1, 1, 1));
                }
            }

            Gizmos.matrix = oldMatrix;
            Gizmos.color = oldColor;
        }

        public void UpdateBakedDataStatistics()
        {
            if (mProbeBatchesUsed == null)
                return;

            mProbeDataSizes = new int[mProbeBatchesUsed.Length];
            mTotalDataSize = 0;

            for (var i = 0; i < mProbeBatchesUsed.Length; ++i)
            {
                mProbeDataSizes[i] = mProbeBatchesUsed[i].GetSizeForLayer(mIdentifier);
                mTotalDataSize += mProbeDataSizes[i];
            }
        }

        public void BeginBake()
        {
            CacheIdentifier();
            CacheProbeBatchesUsed();

            var tasks = new BakedDataTask[1];
            tasks[0].gameObject = gameObject;
            tasks[0].component = this;
            tasks[0].name = "Reverb";
            tasks[0].identifier = mIdentifier;
            tasks[0].probeBatches = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
            tasks[0].probeBatchNames = new string[tasks[0].probeBatches.Length];
            tasks[0].probeBatchAssets = new SerializedData[tasks[0].probeBatches.Length];
            for (var i = 0; i < tasks[0].probeBatchNames.Length; ++i)
            {
                tasks[0].probeBatchNames[i] = tasks[0].probeBatches[i].gameObject.name;
                tasks[0].probeBatchAssets[i] = tasks[0].probeBatches[i].GetAsset();
            }

            Baker.BeginBake(tasks);
        }

        void CacheIdentifier()
        {
            mIdentifier.type = BakedDataType.Reflections;
            mIdentifier.variation = BakedDataVariation.Reverb;
        }

        void CacheProbeBatchesUsed()
        {
            mProbeBatchesUsed = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
        }
#endif
    }
}