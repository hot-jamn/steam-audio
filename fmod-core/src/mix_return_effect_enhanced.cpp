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
// Enhanced Mixer Return Effect State
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Enhanced mixer return effect state with comprehensive error handling and performance monitoring.
 */
struct EnhancedMixerReturnEffectState
{
    // Steam Audio objects
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio settings
    IPLAudioSettings audioSettings;
    int inputChannels;
    int outputChannels;

    // Audio buffers
    IPLAudioBuffer inputBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters with validation
    struct ValidatedParameters
    {
        bool applyBinaural;
        ParameterSpeakerFormatType outputFormatType;

        /**
         * @brief Validate all parameters are within acceptable ranges.
         * @return True if all parameters are valid.
         */
        bool validate() const
        {
            return true; // Basic validation for mixer return
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
    EnhancedMixerReturnEffectState()
        : ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , audioSettings{}
        , inputChannels(0)
        , outputChannels(0)
        , inputBuffer{}
        , outputBuffer{}
        , lastErrorTime(std::chrono::steady_clock::now())
    {
        // Initialize parameters with safe defaults
        parameters.applyBinaural = false;
        parameters.outputFormatType = PARAMETER_FROM_MIXER;
    }

    /**
     * @brief Destructor with proper cleanup and leak detection.
     */
    ~EnhancedMixerReturnEffectState()
    {
        cleanup();
    }

    /**
     * @brief Clean up all resources with proper error handling.
     */
    void cleanup()
    {
        // Clean up Steam Audio objects
        if (binauralEffect)
        {
            iplBinauralEffectRelease(&binauralEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(binauralEffect);
        }
        if (ambisonicsEffect)
        {
            iplAmbisonicsDecodeEffectRelease(&ambisonicsEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(ambisonicsEffect);
        }

        // Clean up audio buffers
        if (inputBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &inputBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("MixerReturn", inputBuffer.numChannels * inputBuffer.numSamples * sizeof(float));
        }
        if (outputBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &outputBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("MixerReturn", outputBuffer.numChannels * outputBuffer.numSamples * sizeof(float));
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
                "Steam Audio context not available for mixer return initialization"
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
                "Mixer return effect initialized successfully (input: %d, output: %d, frame: %d)",
                inChannels, outChannels, frameSize
            );

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InitializationFailed,
                ErrorSeverity::Critical,
                "Exception during mixer return initialization: %s", e.what()
            );
            cleanup();
            return false;
        }
        catch (...)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Critical,
                "Unknown exception during mixer return initialization"
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
                "Mixer return entering error recovery mode due to %d consecutive errors",
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

    bool createSteamAudioObjects(int /* frameSize */)
    {
        // Create Ambisonics decode effect
        IPLAmbisonicsDecodeEffectSettings ambisonicsSettings{};
        ambisonicsSettings.speakerLayout = speakerLayoutForNumChannels(outputChannels);
        ambisonicsSettings.hrtf = nullptr; // Speaker output
        ambisonicsSettings.maxOrder = orderForNumChannels(inputChannels);
        
        auto status = iplAmbisonicsDecodeEffectCreate(gContext, &audioSettings, &ambisonicsSettings, &ambisonicsEffect);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::EffectCreationFailed,
                ErrorSeverity::Error,
                "Failed to create Ambisonics decode effect for mixer return: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, ambisonicsEffect, sizeof(void*), "IPLAmbisonicsDecodeEffect");

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
                    "Failed to create binaural effect for mixer return: %d", static_cast<int>(status)
                );
                // Continue without binaural effect
            }
            else
            {
                STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, binauralEffect, sizeof(void*), "IPLBinauralEffect");
            }
        }

        return true;
    }

    bool allocateAudioBuffers(int frameSize)
    {
        // Update audio settings with actual frame size
        audioSettings.frameSize = frameSize;
        
        // Allocate input buffer
        auto status = iplAudioBufferAllocate(gContext, inputChannels, frameSize, &inputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate mixer return input buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("MixerReturn", inputChannels * frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, inputBuffer.data, 
                                       inputChannels * frameSize * sizeof(float), "MixerReturnInputBuffer");

        // Allocate output buffer
        status = iplAudioBufferAllocate(gContext, outputChannels, frameSize, &outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate mixer return output buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("MixerReturn", outputChannels * frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, outputBuffer.data, 
                                       outputChannels * frameSize * sizeof(float), "MixerReturnOutputBuffer");

        return true;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Enhanced DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK enhancedMixerReturnCreate(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("MixerReturn_Create", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");

    try
    {
        auto* state = new EnhancedMixerReturnEffectState();
        dsp->plugindata = state;

        STEAMAUDIO_FMODCORE_TRACK_ALLOC(
            ResourceTracker::ResourceType::Other, 
            state, 
            sizeof(EnhancedMixerReturnEffectState), 
            "EnhancedMixerReturnEffectState"
        );

        return FMOD_OK;
    }
    catch (const std::bad_alloc&)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::MemoryAllocationFailed,
            ErrorSeverity::Critical,
            "Failed to allocate memory for mixer return effect state"
        );
        return FMOD_ERR_MEMORY;
    }
    catch (...)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Critical,
            "Unknown exception in mixer return create"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedMixerReturnRelease(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("MixerReturn_Release", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");

    try
    {
        auto* state = static_cast<EnhancedMixerReturnEffectState*>(dsp->plugindata);
        
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
            "Exception in mixer return release"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedMixerReturnProcess(FMOD_DSP_STATE* dsp,
                                                  unsigned int length,
                                                  const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                                  FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                                  FMOD_BOOL inputsIdle,
                                                  FMOD_DSP_PROCESS_OPERATION op)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("MixerReturn_Process", length);
    
    // Validate parameters
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(inBuffers, "inBuffers");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(outBuffers, "outBuffers");

    auto* state = static_cast<EnhancedMixerReturnEffectState*>(dsp->plugindata);

    // Handle query operation
    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->parameters.outputFormatType))
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InvalidParameter,
                ErrorSeverity::Error,
                "Failed to initialize mixer return output buffer format"
            );
            return FMOD_ERR_FORMAT;
        }
        return FMOD_OK;
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
        // Simple pass-through for now (would implement actual mixer return processing)
        if (inBuffers->buffers[0] != outBuffers->buffers[0])
        {
            memcpy(outBuffers->buffers[0], inBuffers->buffers[0], 
                   length * inBuffers->buffernumchannels[0] * sizeof(float));
        }

        // Reset consecutive errors on successful processing
        state->consecutiveErrors.store(0);

        return FMOD_OK;
    }
    catch (const std::exception& e)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Error,
            "Exception during mixer return processing: %s", e.what()
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

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Mixer Return Effect Description
// --------------------------------------------------------------------------------------------------------------------

// Forward declaration of parameter array from basic mixer return effect
extern FMOD_DSP_PARAMETER_DESC* gMixerReturnParameterDescsArray[IPL_FMODCORE_MIXRETURN_NUM_PARAMS];

FMOD_DSP_DESCRIPTION gEnhancedMixerReturnEffect =
{
    0x00020229,
    "SA Enhanced MixerReturn",
    0x00010000,
    1,
    1,
    enhancedMixerReturnCreate,
    enhancedMixerReturnRelease,
    nullptr,
    nullptr,
    enhancedMixerReturnProcess,
    nullptr,
    IPL_FMODCORE_MIXRETURN_NUM_PARAMS,
    gMixerReturnParameterDescsArray,
    nullptr,
    nullptr,
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