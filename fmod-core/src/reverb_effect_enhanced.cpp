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
#include "performance_monitor.h"
#include "error_handling.h"

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Reverb Effect State
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Enhanced reverb effect state with comprehensive error handling and performance monitoring.
 */
struct EnhancedReverbEffectState
{
    // Steam Audio objects
    IPLReflectionEffect reflectionEffect;
    IPLBinauralEffect binauralEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;

    // Audio settings (Steam Audio 4.6.1 uses IPLAudioSettings instead of IPLAudioFormat)
    IPLAudioSettings audioSettings;
    int inputChannels;
    int outputChannels;

    // Audio buffers
    IPLAudioBuffer inputBuffer;
    IPLAudioBuffer outputBuffer;
    IPLAudioBuffer ambisonicsBuffer;

    // Parameters with validation
    struct ValidatedParameters
    {
        bool applyBinaural;
        float mixLevel;                     // [0.0, 10.0]
        ParameterSpeakerFormatType outputFormatType;

        /**
         * @brief Validate all parameters are within acceptable ranges.
         * @return True if all parameters are valid.
         */
        bool validate() const
        {
            return ParameterValidator::validateRange(mixLevel, 0.0f, 10.0f, "mixLevel");
        }
    } parameters;

    // State tracking with thread safety
    std::atomic<bool> initialized{ false };
    std::atomic<bool> needsReset{ false };
    std::atomic<int> currentHRTFIndex{ 0 };
    
    // Error recovery state
    std::atomic<int> consecutiveErrors{ 0 };
    std::atomic<bool> errorRecoveryMode{ false };
    std::chrono::steady_clock::time_point lastErrorTime;

    /**
     * @brief Constructor with proper initialization.
     */
    EnhancedReverbEffectState()
        : reflectionEffect(nullptr)
        , binauralEffect(nullptr)
        , ambisonicsEffect(nullptr)
        , audioSettings{}
        , inputChannels(0)
        , outputChannels(0)
        , inputBuffer{}
        , outputBuffer{}
        , ambisonicsBuffer{}
        , lastErrorTime(std::chrono::steady_clock::now())
    {
        // Initialize parameters with safe defaults
        parameters.applyBinaural = false;
        parameters.mixLevel = 1.0f;
        parameters.outputFormatType = PARAMETER_FROM_MIXER;
    }

    /**
     * @brief Destructor with proper cleanup and leak detection.
     */
    ~EnhancedReverbEffectState()
    {
        cleanup();
    }

    /**
     * @brief Clean up all resources with proper error handling.
     */
    void cleanup()
    {
        // Clean up Steam Audio objects
        if (ambisonicsEffect)
        {
            iplAmbisonicsDecodeEffectRelease(&ambisonicsEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(ambisonicsEffect);
        }
        if (binauralEffect)
        {
            iplBinauralEffectRelease(&binauralEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(binauralEffect);
        }
        if (reflectionEffect)
        {
            iplReflectionEffectRelease(&reflectionEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(reflectionEffect);
        }

        // Clean up audio buffers
        if (inputBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &inputBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Reverb", inputBuffer.numChannels * inputBuffer.numSamples * sizeof(float));
        }
        if (outputBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &outputBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Reverb", outputBuffer.numChannels * outputBuffer.numSamples * sizeof(float));
        }
        if (ambisonicsBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &ambisonicsBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Reverb", ambisonicsBuffer.numChannels * ambisonicsBuffer.numSamples * sizeof(float));
        }

        initialized.store(false);
    }

    /**
     * @brief Initialize Steam Audio objects with comprehensive error handling.
     * @param inputChannels Number of input channels.
     * @param outputChannels Number of output channels.
     * @param frameSize Frame size in samples.
     * @return True if initialization succeeded.
     */
    bool initializeSteamAudio(int inChannels, int outChannels, int frameSize)
    {
        if (!gContext)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::ContextNotAvailable,
                ErrorSeverity::Error,
                "Steam Audio context not available for reverb initialization"
            );
            return false;
        }

        // Validate audio format
        if (!ParameterValidator::validateAudioFormat(inChannels, 44100, frameSize))
        {
            return false;
        }

        try
        {
            // Set up audio formats
            setupAudioFormats(inChannels, outChannels);

            // Create Steam Audio objects
            if (!createSteamAudioObjects(frameSize))
            {
                cleanup();
                return false;
            }

            // Allocate audio buffers
            if (!allocateAudioBuffers(frameSize))
            {
                cleanup();
                return false;
            }

            initialized.store(true);
            consecutiveErrors.store(0);
            errorRecoveryMode.store(false);

            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::Success,
                ErrorSeverity::Info,
                "Reverb effect initialized successfully (input: %d, output: %d, frame: %d)",
                inChannels, outChannels, frameSize
            );

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InitializationFailed,
                ErrorSeverity::Critical,
                "Exception during reverb initialization: %s", e.what()
            );
            cleanup();
            return false;
        }
        catch (...)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Critical,
                "Unknown exception during reverb initialization"
            );
            cleanup();
            return false;
        }
    }

    /**
     * @brief Handle error recovery logic.
     * @param error Error that occurred.
     * @return True if processing should continue, false if it should be aborted.
     */
    bool handleError(ErrorCode /* error */)
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceLastError = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastErrorTime);
        
        consecutiveErrors.fetch_add(1);
        lastErrorTime = currentTime;

        // If too many consecutive errors in short time, enter recovery mode
        if (consecutiveErrors.load() > 3 && timeSinceLastError.count() < 1000)
        {
            errorRecoveryMode.store(true);
            needsReset.store(true);
            
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::PerformanceThresholdExceeded,
                ErrorSeverity::Warning,
                "Reverb entering error recovery mode due to %d consecutive errors",
                consecutiveErrors.load()
            );
            
            return false;
        }

        // Reset error count if enough time has passed
        if (timeSinceLastError.count() > 5000)
        {
            consecutiveErrors.store(0);
            errorRecoveryMode.store(false);
        }

        return true;
    }

private:
    void setupAudioFormats(int inChannels, int outChannels)
    {
        // Set up audio settings for Steam Audio 4.6.1
        this->inputChannels = inChannels;
        this->outputChannels = outChannels;
        
        audioSettings.samplingRate = 48000; // Default sampling rate
        audioSettings.frameSize = 512; // Default frame size, will be updated during processing
    }

    bool createSteamAudioObjects(int frameSize)
    {
        // Create reflection effect
        IPLReflectionEffectSettings reflectionSettings{};
        reflectionSettings.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        reflectionSettings.irSize = frameSize * 4; // Typical IR size
        reflectionSettings.numChannels = numChannelsForOrder(orderForNumChannels(inputChannels));
        
        auto status = iplReflectionEffectCreate(gContext, &audioSettings, &reflectionSettings, &reflectionEffect);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::EffectCreationFailed,
                ErrorSeverity::Error,
                "Failed to create reflection effect: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, reflectionEffect, sizeof(void*), "IPLReflectionEffect");

        // Create binaural effect if needed
        if (parameters.applyBinaural && gHRTF[0])
        {
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[0];
            
            status = iplBinauralEffectCreate(gContext, &audioSettings, &binauralSettings, &binauralEffect);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::EffectCreationFailed,
                    ErrorSeverity::Warning,
                    "Failed to create binaural effect for reverb: %d", static_cast<int>(status)
                );
                // Continue without binaural effect
            }
            else
            {
                STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, binauralEffect, sizeof(void*), "IPLBinauralEffect");
            }
        }

        // Create Ambisonics decode effect if needed
        if (!parameters.applyBinaural)
        {
            IPLAmbisonicsDecodeEffectSettings ambisonicsSettings{};
            ambisonicsSettings.speakerLayout = speakerLayoutForNumChannels(outputChannels);
            ambisonicsSettings.hrtf = nullptr; // Speaker output
            ambisonicsSettings.maxOrder = orderForNumChannels(inputChannels);
            
            status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &ambisonicsSettings, &ambisonicsEffect);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::EffectCreationFailed,
                    ErrorSeverity::Warning,
                    "Failed to create Ambisonics decode effect: %d", static_cast<int>(status)
                );
                // Continue without Ambisonics decoding
            }
            else
            {
                STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, ambisonicsEffect, sizeof(void*), "IPLAmbisonicsDecodeEffect");
            }
        }

        return true;
    }

    bool allocateAudioBuffers(int frameSize)
    {
        // Update audio settings with actual frame size
        audioSettings.frameSize = frameSize;
        
        // Allocate input buffer
        auto status = iplAudioBufferAllocate(gContext, numChannelsForOrder(orderForNumChannels(inputChannels)), frameSize, &inputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate reverb input buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("Reverb", numChannelsForOrder(orderForNumChannels(inputChannels)) * frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, inputBuffer.data, 
                                       numChannelsForOrder(orderForNumChannels(inputChannels)) * frameSize * sizeof(float), "ReverbInputBuffer");

        // Allocate output buffer
        status = iplAudioBufferAllocate(gContext, outputChannels, frameSize, &outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate reverb output buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("Reverb", outputChannels * frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, outputBuffer.data, 
                                       outputChannels * frameSize * sizeof(float), "ReverbOutputBuffer");

        // Allocate Ambisonics buffer if needed
        if (ambisonicsEffect)
        {
            status = iplAudioBufferAllocate(gContext, numChannelsForOrder(orderForNumChannels(inputChannels)), frameSize, &ambisonicsBuffer);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::AudioBufferAllocationFailed,
                    ErrorSeverity::Error,
                    "Failed to allocate reverb Ambisonics buffer: %d", static_cast<int>(status)
                );
                return false;
            }
            STEAMAUDIO_FMODCORE_RECORD_ALLOC("Reverb", numChannelsForOrder(orderForNumChannels(inputChannels)) * frameSize * sizeof(float));
            STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, ambisonicsBuffer.data, 
                                           numChannelsForOrder(orderForNumChannels(inputChannels)) * frameSize * sizeof(float), "ReverbAmbisonicsBuffer");
        }

        return true;
    }
};

// Forward declaration
FMOD_RESULT processReverbWithSteamAudio(EnhancedReverbEffectState* state,
                                        FMOD_DSP_STATE* dsp,
                                        unsigned int length,
                                        const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                        FMOD_DSP_BUFFER_ARRAY* outBuffers);

// --------------------------------------------------------------------------------------------------------------------
// Enhanced DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK enhancedReverbCreate(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Reverb_Create", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");

    try
    {
        auto* state = new EnhancedReverbEffectState();
        dsp->plugindata = state;

        STEAMAUDIO_FMODCORE_TRACK_ALLOC(
            ResourceTracker::ResourceType::Other, 
            state, 
            sizeof(EnhancedReverbEffectState), 
            "EnhancedReverbEffectState"
        );

        return FMOD_OK;
    }
    catch (const std::bad_alloc&)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::MemoryAllocationFailed,
            ErrorSeverity::Critical,
            "Failed to allocate memory for reverb effect state"
        );
        return FMOD_ERR_MEMORY;
    }
    catch (...)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Critical,
            "Unknown exception in reverb create"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedReverbRelease(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Reverb_Release", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");

    try
    {
        auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);
        
        STEAMAUDIO_FMODCORE_TRACK_DEALLOC(state);
        
        delete state;
        dsp->plugindata = nullptr;

        return FMOD_OK;
    }
    catch (...)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Error,
            "Exception in reverb release"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedReverbProcess(FMOD_DSP_STATE* dsp,
                                             unsigned int length,
                                             const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                             FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                             FMOD_BOOL inputsIdle,
                                             FMOD_DSP_PROCESS_OPERATION op)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Reverb_Process", length);
    
    // Validate parameters
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(inBuffers, "inBuffers");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(outBuffers, "outBuffers");

    auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);

    // Handle query operation
    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->parameters.outputFormatType))
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InvalidParameter,
                ErrorSeverity::Error,
                "Failed to initialize reverb output buffer format"
            );
            return FMOD_ERR_FORMAT;
        }
        return FMOD_OK;
    }

    // Validate parameters
    if (!state->parameters.validate())
    {
        if (!state->handleError(ErrorCode::ParameterValidationFailed))
        {
            // Pass through audio unchanged in error recovery mode
            if (inBuffers->buffers[0] != outBuffers->buffers[0])
            {
                memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                       length * inBuffers->buffernumchannels[0] * sizeof(float));
            }
            return FMOD_OK;
        }
    }

    // Check for error recovery mode
    if (state->errorRecoveryMode.load())
    {
        // Simple pass-through in recovery mode
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    // Check if context is available
    if (!gContext || inputsIdle)
    {
        // Pass through if no context or input is idle
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }
        return FMOD_OK;
    }

    // Initialize if needed
    if (!state->initialized.load() || state->needsReset.load())
    {
        if (!state->initializeSteamAudio(inBuffers->buffernumchannels[0], outBuffers->buffernumchannels[0], length))
        {
            if (!state->handleError(ErrorCode::InitializationFailed))
            {
                // Pass through on initialization failure
                if (inBuffers->buffers[0] != outBuffers->buffers[0])
                {
                    memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                           length * inBuffers->buffernumchannels[0] * sizeof(float));
                }
                return FMOD_OK;
            }
        }
        state->needsReset.store(false);
    }

    try
    {
        // Process reverb with Steam Audio
        return processReverbWithSteamAudio(state, dsp, length, inBuffers, outBuffers);
    }
    catch (const std::exception& e)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Error,
            "Exception during reverb processing: %s", e.what()
        );
        
        if (!state->handleError(ErrorCode::UnexpectedError))
        {
            // Pass through on exception
            if (inBuffers->buffers[0] != outBuffers->buffers[0])
            {
                memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                       length * inBuffers->buffernumchannels[0] * sizeof(float));
            }
        }
        return FMOD_OK;
    }
}

/**
 * @brief Process reverb with Steam Audio effects.
 * @param state Effect state.
 * @param dsp DSP state.
 * @param length Frame length.
 * @param inBuffers Input buffers.
 * @param outBuffers Output buffers.
 * @return FMOD result code.
 */
FMOD_RESULT processReverbWithSteamAudio(EnhancedReverbEffectState* state,
                                        FMOD_DSP_STATE* /* dsp */,
                                        unsigned int length,
                                        const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                        FMOD_DSP_BUFFER_ARRAY* outBuffers)
{
    // Update global state if needed
    if (gNewHRTFWritten.load())
    {
        state->currentHRTFIndex.store(1 - state->currentHRTFIndex.load());
        
        if (state->binauralEffect && gHRTF[state->currentHRTFIndex.load()])
        {
            iplBinauralEffectRelease(&state->binauralEffect);
            
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[state->currentHRTFIndex.load()];
            
            auto status = iplBinauralEffectCreate(gContext, &state->audioSettings, &binauralSettings, &state->binauralEffect);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::EffectCreationFailed,
                    ErrorSeverity::Warning,
                    "Failed to recreate binaural effect for reverb with new HRTF: %d", static_cast<int>(status)
                );
                state->binauralEffect = nullptr;
            }
        }
    }

    // Set up input buffer from FMOD
    IPLAudioBuffer fmodInputBuffer{};
    fmodInputBuffer.numChannels = inBuffers->buffernumchannels[0];
    fmodInputBuffer.numSamples = length;
    fmodInputBuffer.data = reinterpret_cast<float**>(&inBuffers->buffers[0]);

    // Process reflection effect
    if (state->reflectionEffect)
    {
        IPLReflectionEffectParams reflectionParams{};
        reflectionParams.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        
        // Calculate IR size using explicit signed conversion to avoid warnings
        reflectionParams.irSize = static_cast<IPLint32>(static_cast<int>(length) * 4);
        
        reflectionParams.delay = 0;
        
        // In a real implementation, this would use actual reflection IR data
        // For now, we'll process the input as-is
        auto status = iplReflectionEffectApply(state->reflectionEffect, &reflectionParams, 
                                             &fmodInputBuffer, &state->inputBuffer, nullptr);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Warning,
                "Reflection effect processing failed: %d", static_cast<int>(status)
            );
            
            // Fall back to pass-through
            if (inBuffers->buffers[0] != outBuffers->buffers[0])
            {
                memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                       length * inBuffers->buffernumchannels[0] * sizeof(float));
            }
            return FMOD_OK;
        }
    }

    // Apply binaural processing if enabled
    if (state->parameters.applyBinaural && state->binauralEffect && !gHRTFDisabled.load())
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = IPLVector3{ 0.0f, 0.0f, -1.0f }; // Default forward direction
        binauralParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
        binauralParams.spatialBlend = 1.0f;
        binauralParams.hrtf = gHRTF[state->currentHRTFIndex.load()];

        auto status = iplBinauralEffectApply(state->binauralEffect, &binauralParams, 
                                           &state->inputBuffer, &state->outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Warning,
                "Binaural effect processing failed for reverb: %d", static_cast<int>(status)
            );
        }
    }
    // Apply Ambisonics decoding if not using binaural
    else if (state->ambisonicsEffect)
    {
        IPLAmbisonicsDecodeEffectParams ambisonicsParams{};
        ambisonicsParams.order = orderForNumChannels(state->inputChannels);
        ambisonicsParams.hrtf = nullptr; // Speaker output
        ambisonicsParams.orientation = IPLCoordinateSpace3{ 
            IPLVector3{0.0f, 0.0f, 0.0f}, 
            IPLVector3{0.0f, 0.0f, -1.0f}, 
            IPLVector3{0.0f, 1.0f, 0.0f}, 
            IPLVector3{1.0f, 0.0f, 0.0f} 
        };
        ambisonicsParams.binaural = IPL_FALSE;

        auto status = iplAmbisonicsDecodeEffectApply(state->ambisonicsEffect, &ambisonicsParams, 
                                                   &state->inputBuffer, &state->outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Warning,
                "Ambisonics decode effect processing failed: %d", static_cast<int>(status)
            );
        }
    }
    else
    {
        // Direct copy if no processing
        if (state->inputBuffer.data && state->outputBuffer.data)
        {
            int channelsToCopy = (state->inputBuffer.numChannels < state->outputBuffer.numChannels) ?
                                state->inputBuffer.numChannels : state->outputBuffer.numChannels;
            for (int ch = 0; ch < channelsToCopy; ++ch)
            {
                memcpy(state->outputBuffer.data[ch], state->inputBuffer.data[ch], 
                       length * sizeof(float));
            }
        }
    }

    // Apply mix level and copy to FMOD output buffer
    if (state->outputBuffer.data && outBuffers->buffers[0])
    {
        int channelsToCopy = (state->outputBuffer.numChannels < static_cast<int>(outBuffers->buffernumchannels[0])) ?
                            state->outputBuffer.numChannels : static_cast<int>(outBuffers->buffernumchannels[0]);
        
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            for (unsigned int i = 0; i < length; ++i)
            {
                static_cast<float*>(outBuffers->buffers[0])[ch * length + i] = 
                    state->outputBuffer.data[ch][i] * state->parameters.mixLevel;
            }
        }
    }

    // Reset consecutive errors on successful processing
    state->consecutiveErrors.store(0);

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK enhancedReverbSetParameterFloat(FMOD_DSP_STATE* dsp, int index, float value)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Reverb_SetParameterFloat", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");

    auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case REVERB_PARAM_BINAURAL:
        state->parameters.applyBinaural = (value != 0.0f);
        state->needsReset.store(true);
        break;

    case REVERB_PARAM_MIXLEVEL:
        if (ParameterValidator::validateRange(value, 0.0f, 10.0f, "mixLevel"))
        {
            state->parameters.mixLevel = value;
        }
        else
        {
            return FMOD_ERR_INVALID_PARAM;
        }
        break;

    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK enhancedReverbGetParameterFloat(FMOD_DSP_STATE* dsp, int index, float* value, char* valuestr)
{
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(value, "value");

    auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case REVERB_PARAM_BINAURAL:
        *value = state->parameters.applyBinaural ? 1.0f : 0.0f;
        if (valuestr) strcpy(valuestr, state->parameters.applyBinaural ? "True" : "False");
        break;

    case REVERB_PARAM_MIXLEVEL:
        *value = state->parameters.mixLevel;
        if (valuestr) sprintf(valuestr, "%.2f", state->parameters.mixLevel);
        break;

    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK enhancedReverbSetParameterInt(FMOD_DSP_STATE* dsp, int index, int value)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Reverb_SetParameterInt", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");

    auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case REVERB_PARAM_OUTPUTFORMAT:
        if (value >= 0 && value < PARAMETER_NUM_SPEAKER_FORMAT_TYPES)
        {
            state->parameters.outputFormatType = static_cast<ParameterSpeakerFormatType>(value);
            state->needsReset.store(true);
        }
        else
        {
            return FMOD_ERR_INVALID_PARAM;
        }
        break;

    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK enhancedReverbGetParameterInt(FMOD_DSP_STATE* dsp, int index, int* value, char* valuestr)
{
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(value, "value");

    auto* state = static_cast<EnhancedReverbEffectState*>(dsp->plugindata);

    switch (index)
    {
    case REVERB_PARAM_OUTPUTFORMAT:
        *value = static_cast<int>(state->parameters.outputFormatType);
        if (valuestr)
        {
            switch (state->parameters.outputFormatType)
            {
            case PARAMETER_FROM_MIXER: strcpy(valuestr, "From Mixer"); break;
            default: strcpy(valuestr, "Unknown"); break;
            }
        }
        break;

    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
}

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Reverb Effect Description
// --------------------------------------------------------------------------------------------------------------------

FMOD_DSP_PARAMETER_DESC gEnhancedReverbBinauralParam =
{
    FMOD_DSP_PARAMETER_TYPE_BOOL,
    "Binaural",
    "",
    "Apply binaural rendering to reverb output."
};

FMOD_DSP_PARAMETER_DESC gEnhancedReverbMixLevelParam =
{
    FMOD_DSP_PARAMETER_TYPE_FLOAT,
    "Mix Level",
    "dB",
    "Mix level for reverb effect.",
    0.0f,
    10.0f,
    1.0f
};

FMOD_DSP_PARAMETER_DESC gEnhancedReverbOutputFormatParam =
{
    FMOD_DSP_PARAMETER_TYPE_INT,
    "Output Format",
    "",
    "Output speaker format.",
    0.0f,
    static_cast<float>(PARAMETER_NUM_SPEAKER_FORMAT_TYPES - 1),
    0.0f
};

FMOD_DSP_PARAMETER_DESC* gEnhancedReverbParameters[] =
{
    &gEnhancedReverbBinauralParam,
    &gEnhancedReverbMixLevelParam,
    &gEnhancedReverbOutputFormatParam
};

FMOD_DSP_DESCRIPTION gEnhancedReverbEffect =
{
    FMOD_PLUGIN_SDK_VERSION,
    "Steam Audio Enhanced Reverb",
    STEAMAUDIO_FMODCORE_VERSION,
    1,
    1,
    enhancedReverbCreate,
    enhancedReverbRelease,
    nullptr,
    nullptr,
    enhancedReverbProcess,
    nullptr,
    static_cast<int>(sizeof(gEnhancedReverbParameters) / sizeof(gEnhancedReverbParameters[0])),
    gEnhancedReverbParameters,
    nullptr,
    enhancedReverbSetParameterInt,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

} // namespace SteamAudioFMODCore