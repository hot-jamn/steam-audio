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

// Forward declaration of parameter array from basic spatialize effect
namespace SteamAudioFMODCore {
    extern FMOD_DSP_PARAMETER_DESC* gSpatializeParameterDescsArray[IPL_FMODCORE_SPATIALIZE_NUM_PARAMS];
}

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Spatializer Effect State
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Enhanced spatializer effect state with improved error handling and performance monitoring.
 */
struct EnhancedSpatializerEffectState
{
    // Steam Audio objects
    IPLSource source;
    IPLDirectEffect directEffect;
    IPLReflectionEffect reflectionEffect;
    IPLPathEffect pathEffect;
    IPLAmbisonicsDecodeEffect ambisonicsEffect;
    IPLBinauralEffect binauralEffect;

    // Audio settings (Steam Audio 4.6.1 uses IPLAudioSettings instead of IPLAudioFormat)
    IPLAudioSettings audioSettings;
    int inputChannels;
    int outputChannels;

    // Audio buffers
    IPLAudioBuffer monoBuffer;
    IPLAudioBuffer ambisonicsBuffer;
    IPLAudioBuffer outputBuffer;

    // Parameters with validation
    struct ValidatedParameters
    {
        bool applyDistanceAttenuation;
        bool applyAirAbsorption;
        bool applyDirectivity;
        bool applyOcclusion;
        bool applyTransmission;
        bool applyReflections;
        bool applyPathing;
        bool directBinaural;
        bool reflectionsBinaural;
        bool pathingBinaural;
        
        float directMixLevel;           // [0.0, 1.0]
        float reflectionsMixLevel;      // [0.0, 10.0]
        float pathingMixLevel;          // [0.0, 10.0]
        
        int32_t simulationOutputsHandle; // [-1, 10000]
        ParameterSpeakerFormatType outputFormatType;

        /**
         * @brief Validate all parameters are within acceptable ranges.
         * @return True if all parameters are valid.
         */
        bool validate() const
        {
            return ParameterValidator::validateRange(directMixLevel, 0.0f, 1.0f, "directMixLevel") &&
                   ParameterValidator::validateRange(reflectionsMixLevel, 0.0f, 10.0f, "reflectionsMixLevel") &&
                   ParameterValidator::validateRange(pathingMixLevel, 0.0f, 10.0f, "pathingMixLevel") &&
                   ParameterValidator::validateRange(simulationOutputsHandle, -1, 10000, "simulationOutputsHandle");
        }
    } parameters;

    // Simulation outputs
    IPLSimulationOutputs simulationOutputs;
    
    // State tracking with thread safety
    std::atomic<bool> initialized{ false };
    std::atomic<bool> needsReset{ false };
    std::atomic<int> currentHRTFIndex{ 0 };
    std::atomic<int> currentReverbSourceIndex{ 0 };
    std::atomic<int> currentReflectionMixerIndex{ 0 };
    
    // Error recovery state
    std::atomic<int> consecutiveErrors{ 0 };
    std::atomic<bool> errorRecoveryMode{ false };
    std::chrono::steady_clock::time_point lastErrorTime;
    
    // Performance tracking
    mutable std::mutex performanceMutex;
    uint64_t totalProcessCalls{ 0 };
    uint64_t totalProcessTime{ 0 };
    uint64_t maxProcessTime{ 0 };

    /**
     * @brief Constructor with proper initialization.
     */
    EnhancedSpatializerEffectState()
        : source(nullptr)
        , directEffect(nullptr)
        , reflectionEffect(nullptr)
        , pathEffect(nullptr)
        , ambisonicsEffect(nullptr)
        , binauralEffect(nullptr)
        , audioSettings{}
        , inputChannels(0)
        , outputChannels(0)
        , monoBuffer{}
        , ambisonicsBuffer{}
        , outputBuffer{}
        , simulationOutputs{}
        , lastErrorTime(std::chrono::steady_clock::now())
    {
        // Initialize parameters with safe defaults
        parameters.applyDistanceAttenuation = false;
        parameters.applyAirAbsorption = false;
        parameters.applyDirectivity = false;
        parameters.applyOcclusion = false;
        parameters.applyTransmission = false;
        parameters.applyReflections = false;
        parameters.applyPathing = false;
        parameters.directBinaural = false;
        parameters.reflectionsBinaural = false;
        parameters.pathingBinaural = false;
        parameters.directMixLevel = 1.0f;
        parameters.reflectionsMixLevel = 1.0f;
        parameters.pathingMixLevel = 1.0f;
        parameters.simulationOutputsHandle = -1;
        parameters.outputFormatType = PARAMETER_FROM_MIXER;
    }

    /**
     * @brief Destructor with proper cleanup and leak detection.
     */
    ~EnhancedSpatializerEffectState()
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
        if (pathEffect)
        {
            iplPathEffectRelease(&pathEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(pathEffect);
        }
        if (reflectionEffect)
        {
            iplReflectionEffectRelease(&reflectionEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(reflectionEffect);
        }
        if (directEffect)
        {
            iplDirectEffectRelease(&directEffect);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(directEffect);
        }
        if (source)
        {
            iplSourceRelease(&source);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(source);
        }

        // Clean up audio buffers
        if (monoBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &monoBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Spatializer", monoBuffer.numChannels * monoBuffer.numSamples * sizeof(float));
        }
        if (ambisonicsBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &ambisonicsBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Spatializer", ambisonicsBuffer.numChannels * ambisonicsBuffer.numSamples * sizeof(float));
        }
        if (outputBuffer.data && gContext)
        {
            iplAudioBufferFree(gContext, &outputBuffer);
            STEAMAUDIO_FMODCORE_RECORD_DEALLOC("Spatializer", outputBuffer.numChannels * outputBuffer.numSamples * sizeof(float));
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
                "Steam Audio context not available for spatializer initialization"
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
                "Spatializer effect initialized successfully (input: %d, output: %d, frame: %d)",
                inChannels, outChannels, frameSize
            );

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InitializationFailed,
                ErrorSeverity::Critical,
                "Exception during spatializer initialization: %s", e.what()
            );
            cleanup();
            return false;
        }
        catch (...)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Critical,
                "Unknown exception during spatializer initialization"
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
        if (consecutiveErrors.load() > 5 && timeSinceLastError.count() < 1000)
        {
            errorRecoveryMode.store(true);
            needsReset.store(true);
            
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::PerformanceThresholdExceeded,
                ErrorSeverity::Warning,
                "Entering error recovery mode due to %d consecutive errors",
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
        // Create direct effect
        IPLDirectEffectSettings directSettings{};
        directSettings.numChannels = inputChannels;
        
        auto status = iplDirectEffectCreate(gContext, &audioSettings, &directSettings, &directEffect);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::EffectCreationFailed,
                ErrorSeverity::Error,
                "Failed to create direct effect: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::SteamAudioEffect, directEffect, sizeof(void*), "IPLDirectEffect");

        // Create binaural effect if needed
        if (parameters.directBinaural && gHRTF[0])
        {
            IPLBinauralEffectSettings binauralSettings{};
            binauralSettings.hrtf = gHRTF[0];
            
            status = iplBinauralEffectCreate(gContext, &audioSettings, &binauralSettings, &binauralEffect);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::EffectCreationFailed,
                    ErrorSeverity::Warning,
                    "Failed to create binaural effect: %d", static_cast<int>(status)
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
        
        // Allocate mono buffer
        auto status = iplAudioBufferAllocate(gContext, 1, frameSize, &monoBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate mono audio buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("Spatializer", frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, monoBuffer.data, frameSize * sizeof(float), "MonoBuffer");

        // Allocate output buffer
        status = iplAudioBufferAllocate(gContext, outputChannels, frameSize, &outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::AudioBufferAllocationFailed,
                ErrorSeverity::Error,
                "Failed to allocate output audio buffer: %d", static_cast<int>(status)
            );
            return false;
        }
        STEAMAUDIO_FMODCORE_RECORD_ALLOC("Spatializer", outputChannels * frameSize * sizeof(float));
        STEAMAUDIO_FMODCORE_TRACK_ALLOC(ResourceTracker::ResourceType::AudioBuffer, outputBuffer.data, outputChannels * frameSize * sizeof(float), "OutputBuffer");

        return true;
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Forward Declarations
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT processAudioWithSteamAudio(EnhancedSpatializerEffectState* state,
                                       FMOD_DSP_STATE* dsp,
                                       unsigned int length,
                                       const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                       FMOD_DSP_BUFFER_ARRAY* outBuffers);

// --------------------------------------------------------------------------------------------------------------------
// Enhanced DSP Callbacks
// --------------------------------------------------------------------------------------------------------------------

FMOD_RESULT F_CALLBACK enhancedSpatializeCreate(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Spatializer_Create", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");

    try
    {
        auto* state = new EnhancedSpatializerEffectState();
        dsp->plugindata = state;

        STEAMAUDIO_FMODCORE_TRACK_ALLOC(
            ResourceTracker::ResourceType::Other, 
            state, 
            sizeof(EnhancedSpatializerEffectState), 
            "EnhancedSpatializerEffectState"
        );

        return FMOD_OK;
    }
    catch (const std::bad_alloc&)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::MemoryAllocationFailed,
            ErrorSeverity::Critical,
            "Failed to allocate memory for spatializer effect state"
        );
        return FMOD_ERR_MEMORY;
    }
    catch (...)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Critical,
            "Unknown exception in spatializer create"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedSpatializeRelease(FMOD_DSP_STATE* dsp)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Spatializer_Release", 0);
    
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");

    try
    {
        auto* state = static_cast<EnhancedSpatializerEffectState*>(dsp->plugindata);
        
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
            "Exception in spatializer release"
        );
        return FMOD_ERR_INTERNAL;
    }
}

FMOD_RESULT F_CALLBACK enhancedSpatializeProcess(FMOD_DSP_STATE* dsp,
                                                 unsigned int length,
                                                 const FMOD_DSP_BUFFER_ARRAY* inBuffers,
                                                 FMOD_DSP_BUFFER_ARRAY* outBuffers,
                                                 FMOD_BOOL inputsIdle,
                                                 FMOD_DSP_PROCESS_OPERATION op)
{
    STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL("Spatializer_Process", length);
    
    // Validate parameters
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp, "dsp");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(dsp->plugindata, "dsp->plugindata");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(inBuffers, "inBuffers");
    STEAMAUDIO_FMODCORE_VALIDATE_PTR(outBuffers, "outBuffers");

    auto* state = static_cast<EnhancedSpatializerEffectState*>(dsp->plugindata);

    // Handle query operation
    if (op == FMOD_DSP_PROCESS_QUERY)
    {
        if (!initFmodOutBufferFormat(inBuffers, outBuffers, dsp, state->parameters.outputFormatType))
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InvalidParameter,
                ErrorSeverity::Error,
                "Failed to initialize output buffer format"
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
        // Process audio with Steam Audio
        auto startTime = std::chrono::high_resolution_clock::now();
        FMOD_RESULT result = processAudioWithSteamAudio(state, dsp, length, inBuffers, outBuffers);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        
        // Update performance metrics
        {
            std::lock_guard<std::mutex> lock(state->performanceMutex);
            state->totalProcessCalls++;
            state->totalProcessTime += processingTime;
            state->maxProcessTime = (state->maxProcessTime > static_cast<uint64_t>(processingTime)) ?
                                   state->maxProcessTime : static_cast<uint64_t>(processingTime);
        }
        
        return result;
    }
    catch (const std::exception& e)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::UnexpectedError,
            ErrorSeverity::Error,
            "Exception during audio processing: %s", e.what()
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
 * @brief Process audio with Steam Audio effects.
 * @param state Effect state.
 * @param dsp DSP state.
 * @param length Frame length.
 * @param inBuffers Input buffers.
 * @param outBuffers Output buffers.
 * @return FMOD result code.
 */
FMOD_RESULT processAudioWithSteamAudio(EnhancedSpatializerEffectState* state,
                                       FMOD_DSP_STATE* dsp,
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
                    "Failed to recreate binaural effect with new HRTF: %d", static_cast<int>(status)
                );
                state->binauralEffect = nullptr;
            }
        }
    }

    // Get listener coordinates
    auto listenerCoordinates = calcListenerCoordinates(dsp);

    // Get source position (this would come from FMOD's 3D attributes)
    FMOD_3D_ATTRIBUTES sourceAttributes{};
    // In a real implementation, this would be obtained from FMOD's parameter system
    auto sourceCoordinates = calcCoordinates(sourceAttributes);

    // Process direct sound
    if (state->directEffect)
    {
        IPLDirectEffectParams directParams{};
        directParams.flags = static_cast<IPLDirectEffectFlags>(0);
        
        if (state->parameters.applyDistanceAttenuation)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
        if (state->parameters.applyAirAbsorption)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
        if (state->parameters.applyDirectivity)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
        if (state->parameters.applyOcclusion)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION);
        if (state->parameters.applyTransmission)
            directParams.flags = static_cast<IPLDirectEffectFlags>(directParams.flags | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);

        // Set default values (would be replaced with simulation results)
        directParams.distanceAttenuation = 1.0f;
        directParams.airAbsorption[0] = 1.0f;
        directParams.airAbsorption[1] = 1.0f;
        directParams.airAbsorption[2] = 1.0f;
        directParams.directivity = 1.0f;
        directParams.occlusion = 1.0f;
        directParams.transmission[0] = 1.0f;
        directParams.transmission[1] = 1.0f;
        directParams.transmission[2] = 1.0f;

        // Set up input buffer
        IPLAudioBuffer inputBuffer{};
        inputBuffer.numChannels = inBuffers->buffernumchannels[0];
        inputBuffer.numSamples = length;
        inputBuffer.data = reinterpret_cast<float**>(&inBuffers->buffers[0]);

        auto status = iplDirectEffectApply(state->directEffect, &directParams, &inputBuffer, &state->outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Warning,
                "Direct effect processing failed: %d", static_cast<int>(status)
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
    if (state->parameters.directBinaural && state->binauralEffect && !gHRTFDisabled.load())
    {
        IPLBinauralEffectParams binauralParams{};
        binauralParams.direction = unitVector(IPLVector3{ 
            sourceCoordinates.origin.x - listenerCoordinates.origin.x,
            sourceCoordinates.origin.y - listenerCoordinates.origin.y,
            sourceCoordinates.origin.z - listenerCoordinates.origin.z
        });
        binauralParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
        binauralParams.spatialBlend = 1.0f;
        binauralParams.hrtf = gHRTF[state->currentHRTFIndex.load()];

        auto status = iplBinauralEffectApply(state->binauralEffect, &binauralParams, &state->outputBuffer, &state->outputBuffer);
        if (status != IPL_STATUS_SUCCESS)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Warning,
                "Binaural effect processing failed: %d", static_cast<int>(status)
            );
        }
    }

    // Copy output to FMOD buffer
    if (state->outputBuffer.data && outBuffers->buffers[0])
    {
        auto channelsToCopy = (state->outputBuffer.numChannels < static_cast<int>(outBuffers->buffernumchannels[0])) ?
                             state->outputBuffer.numChannels : static_cast<int>(outBuffers->buffernumchannels[0]);
        
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            for (unsigned int i = 0; i < length; ++i)
            {
                static_cast<float*>(outBuffers->buffers[0])[ch * length + i] =
                    state->outputBuffer.data[ch][i] * state->parameters.directMixLevel;
            }
        }
    }

    // Reset consecutive errors on successful processing
    state->consecutiveErrors.store(0);

    return FMOD_OK;
}

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Spatialize Effect Description
// --------------------------------------------------------------------------------------------------------------------

FMOD_DSP_DESCRIPTION gEnhancedSpatializeEffect =
{
    0x00020229,
    "Steam Audio Enhanced Spatial",
    0x00010000,
    1,
    1,
    enhancedSpatializeCreate,
    enhancedSpatializeRelease,
    nullptr,
    nullptr,
    enhancedSpatializeProcess,
    nullptr,
    IPL_FMODCORE_SPATIALIZE_NUM_PARAMS,
    gSpatializeParameterDescsArray,
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