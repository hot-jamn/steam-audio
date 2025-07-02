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
#include "error_handling.h"
#include "performance_monitor.h"
#include "error_handling.h"

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Global State Management
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Enhanced global state with comprehensive monitoring and error handling.
 */
struct EnhancedGlobalState
{
    // Core Steam Audio objects
    IPLContext context;
    IPLScene scene;
    IPLSimulator simulator;
    IPLHRTFSettings hrtfSettings;
    IPLSimulationSettings simulationSettings;

    // HRTF management with double buffering
    struct HRTFState
    {
        IPLHRTFSettings settings;
        IPLAudioSettings audioSettings;
        std::atomic<bool> updatePending{ false };
        std::atomic<bool> disabled{ false };
        std::chrono::steady_clock::time_point lastUpdateTime;
        
        HRTFState() : lastUpdateTime(std::chrono::steady_clock::now()) {}
    } hrtfState;

    // Source management with enhanced tracking
    struct SourceManager
    {
        std::unordered_map<int, IPLSource> sources;
        std::mutex sourcesMutex;
        std::atomic<int> nextSourceId{ 1 };
        std::atomic<int> activeSourceCount{ 0 };
        
        // Performance metrics
        std::atomic<uint64_t> totalSourcesCreated{ 0 };
        std::atomic<uint64_t> totalSourcesDestroyed{ 0 };
        std::atomic<uint64_t> peakActiveSourceCount{ 0 };
        
        void updatePeakCount()
        {
            auto current = activeSourceCount.load();
            auto peak = peakActiveSourceCount.load();
            while (current > peak && !peakActiveSourceCount.compare_exchange_weak(peak, current)) {}
        }
    } sourceManager;

    // Error tracking and recovery
    struct ErrorState
    {
        std::atomic<uint64_t> totalErrors{ 0 };
        std::atomic<uint64_t> criticalErrors{ 0 };
        std::atomic<uint64_t> recoveredErrors{ 0 };
        std::atomic<bool> globalRecoveryMode{ false };
        std::chrono::steady_clock::time_point lastCriticalError;
        
        ErrorState() : lastCriticalError(std::chrono::steady_clock::now()) {}
        
        void reportError(ErrorSeverity severity)
        {
            totalErrors.fetch_add(1);
            if (severity == ErrorSeverity::Critical)
            {
                criticalErrors.fetch_add(1);
                lastCriticalError = std::chrono::steady_clock::now();
                
                // Enter global recovery mode if too many critical errors
                if (criticalErrors.load() > 5)
                {
                    globalRecoveryMode.store(true);
                }
            }
        }
        
        void reportRecovery()
        {
            recoveredErrors.fetch_add(1);
            
            // Exit recovery mode if enough time has passed since last critical error
            auto timeSinceLastCritical = std::chrono::steady_clock::now() - lastCriticalError;
            if (std::chrono::duration_cast<std::chrono::seconds>(timeSinceLastCritical).count() > 30)
            {
                globalRecoveryMode.store(false);
            }
        }
    } errorState;

    // Performance monitoring
    struct PerformanceState
    {
        std::atomic<uint64_t> totalFramesProcessed{ 0 };
        std::atomic<uint64_t> totalProcessingTimeUs{ 0 };
        std::atomic<uint64_t> peakProcessingTimeUs{ 0 };
        std::atomic<double> averageProcessingTimeUs{ 0.0 };
        
        // Memory tracking
        std::atomic<size_t> totalAllocatedBytes{ 0 };
        std::atomic<size_t> peakAllocatedBytes{ 0 };
        std::atomic<int> activeAllocations{ 0 };
        
        void updateProcessingTime(uint64_t timeUs, uint64_t frameCount)
        {
            totalProcessingTimeUs.fetch_add(timeUs);
            totalFramesProcessed.fetch_add(frameCount);
            
            // Update peak
            auto peak = peakProcessingTimeUs.load();
            while (timeUs > peak && !peakProcessingTimeUs.compare_exchange_weak(peak, timeUs)) {}
            
            // Update average
            auto totalTime = totalProcessingTimeUs.load();
            auto totalFrames = totalFramesProcessed.load();
            if (totalFrames > 0)
            {
                averageProcessingTimeUs.store(static_cast<double>(totalTime) / totalFrames);
            }
        }
        
        void updateMemoryUsage(size_t bytes, bool allocation)
        {
            if (allocation)
            {
                totalAllocatedBytes.fetch_add(bytes);
                activeAllocations.fetch_add(1);
                
                auto current = totalAllocatedBytes.load();
                auto peak = peakAllocatedBytes.load();
                while (current > peak && !peakAllocatedBytes.compare_exchange_weak(peak, current)) {}
            }
            else
            {
                totalAllocatedBytes.fetch_sub(bytes);
                activeAllocations.fetch_sub(1);
            }
        }
    } performanceState;

    // Initialization state
    std::atomic<bool> initialized{ false };
    std::atomic<bool> initializationInProgress{ false };
    std::mutex initializationMutex;

    /**
     * @brief Constructor with proper initialization.
     */
    EnhancedGlobalState()
        : context(nullptr)
        , scene(nullptr)
        , simulator(nullptr)
        , hrtfSettings{}
        , simulationSettings{}
    {
    }

    /**
     * @brief Destructor with proper cleanup.
     */
    ~EnhancedGlobalState()
    {
        cleanup();
    }

    /**
     * @brief Initialize the enhanced global state.
     * @param contextSettings Steam Audio context settings.
     * @return True if initialization succeeded.
     */
    bool initialize(const IPLContextSettings& contextSettings)
    {
        std::lock_guard<std::mutex> lock(initializationMutex);
        
        if (initialized.load())
        {
            return true;
        }
        
        if (initializationInProgress.load())
        {
            // Wait for initialization to complete
            while (initializationInProgress.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return initialized.load();
        }
        
        initializationInProgress.store(true);
        
        try
        {
            // Create Steam Audio context
            IPLContextSettings mutableSettings = contextSettings;
            auto status = iplContextCreate(&mutableSettings, &this->context);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::InitializationFailed,
                    ErrorSeverity::Critical,
                    "Failed to create Steam Audio context: %d", static_cast<int>(status)
                );
                initializationInProgress.store(false);
                return false;
            }
            
            STEAMAUDIO_FMODCORE_TRACK_ALLOC(
                ResourceTracker::ResourceType::Context,
                this->context,
                sizeof(void*),
                "IPLContext"
            );

            // Initialize HRTF settings
            initializeHRTFSettings();

            // Initialize simulation settings
            initializeSimulationSettings();

            initialized.store(true);
            initializationInProgress.store(false);

            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::Success,
                ErrorSeverity::Info,
                "Enhanced global state initialized successfully"
            );

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::InitializationFailed,
                ErrorSeverity::Critical,
                "Exception during enhanced global state initialization: %s", e.what()
            );
            cleanup();
            initializationInProgress.store(false);
            return false;
        }
        catch (...)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Critical,
                "Unknown exception during enhanced global state initialization"
            );
            cleanup();
            initializationInProgress.store(false);
            return false;
        }
    }

    /**
     * @brief Clean up all resources.
     */
    void cleanup()
    {
        // Clean up sources
        {
            std::lock_guard<std::mutex> lock(sourceManager.sourcesMutex);
            for (auto& pair : sourceManager.sources)
            {
                if (pair.second)
                {
                    iplSourceRelease(&pair.second);
                    STEAMAUDIO_FMODCORE_TRACK_DEALLOC(pair.second);
                }
            }
            sourceManager.sources.clear();
            sourceManager.activeSourceCount.store(0);
        }

        // Clean up Steam Audio objects
        if (simulator)
        {
            iplSimulatorRelease(&simulator);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(simulator);
            simulator = nullptr;
        }
        
        if (scene)
        {
            iplSceneRelease(&scene);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(scene);
            scene = nullptr;
        }
        
        if (this->context)
        {
            iplContextRelease(&this->context);
            STEAMAUDIO_FMODCORE_TRACK_DEALLOC(this->context);
            this->context = nullptr;
        }

        initialized.store(false);
    }

    /**
     * @brief Create a new source with enhanced tracking.
     * @param sourceSettings Source settings.
     * @return Source ID, or -1 on failure.
     */
    int createSource(const IPLSourceSettings& sourceSettings)
    {
        if (!initialized.load() || errorState.globalRecoveryMode.load())
        {
            return -1;
        }

        std::lock_guard<std::mutex> lock(sourceManager.sourcesMutex);
        
        try
        {
            IPLSource newSource = nullptr;
            auto status = iplSourceCreate(simulator, const_cast<IPLSourceSettings*>(&sourceSettings), &newSource);
            if (status != IPL_STATUS_SUCCESS)
            {
                STEAMAUDIO_FMODCORE_REPORT_ERROR(
                    ErrorCode::SourceCreationFailed,
                    ErrorSeverity::Error,
                    "Failed to create Steam Audio source: %d", static_cast<int>(status)
                );
                errorState.reportError(ErrorSeverity::Error);
                return -1;
            }

            int sourceId = sourceManager.nextSourceId.fetch_add(1);
            sourceManager.sources[sourceId] = newSource;
            sourceManager.activeSourceCount.fetch_add(1);
            sourceManager.totalSourcesCreated.fetch_add(1);
            sourceManager.updatePeakCount();

            STEAMAUDIO_FMODCORE_TRACK_ALLOC(
                ResourceTracker::ResourceType::SteamAudioSource,
                newSource,
                sizeof(void*),
                "IPLSource"
            );

            return sourceId;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Error,
                "Exception during source creation: %s", e.what()
            );
            errorState.reportError(ErrorSeverity::Error);
            return -1;
        }
    }

    /**
     * @brief Destroy a source with enhanced tracking.
     * @param sourceId Source ID to destroy.
     * @return True if successful.
     */
    bool destroySource(int sourceId)
    {
        std::lock_guard<std::mutex> lock(sourceManager.sourcesMutex);
        
        auto it = sourceManager.sources.find(sourceId);
        if (it == sourceManager.sources.end())
        {
            return false;
        }

        try
        {
            if (it->second)
            {
                iplSourceRelease(&it->second);
                STEAMAUDIO_FMODCORE_TRACK_DEALLOC(it->second);
            }
            
            sourceManager.sources.erase(it);
            sourceManager.activeSourceCount.fetch_sub(1);
            sourceManager.totalSourcesDestroyed.fetch_add(1);

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Error,
                "Exception during source destruction: %s", e.what()
            );
            errorState.reportError(ErrorSeverity::Error);
            return false;
        }
    }

    /**
     * @brief Get a source by ID with enhanced safety.
     * @param sourceId Source ID.
     * @return Source pointer, or nullptr if not found.
     */
    IPLSource getSource(int sourceId)
    {
        if (!initialized.load() || errorState.globalRecoveryMode.load())
        {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(sourceManager.sourcesMutex);
        
        auto it = sourceManager.sources.find(sourceId);
        return (it != sourceManager.sources.end()) ? it->second : nullptr;
    }

    /**
     * @brief Update HRTF with enhanced error handling.
     * @param newSettings New HRTF settings.
     * @return True if successful.
     */
    bool updateHRTF(const IPLHRTFSettings& newSettings)
    {
        if (!initialized.load())
        {
            return false;
        }

        try
        {
            hrtfState.settings = newSettings;
            hrtfState.updatePending.store(true);
            hrtfState.lastUpdateTime = std::chrono::steady_clock::now();

            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::Success,
                ErrorSeverity::Info,
                "HRTF update scheduled successfully"
            );

            return true;
        }
        catch (const std::exception& e)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::UnexpectedError,
                ErrorSeverity::Error,
                "Exception during HRTF update: %s", e.what()
            );
            errorState.reportError(ErrorSeverity::Error);
            return false;
        }
    }

    /**
     * @brief Get comprehensive performance statistics.
     * @return Performance statistics structure.
     */
    struct PerformanceStats
    {
        uint64_t totalFramesProcessed;
        double averageProcessingTimeUs;
        uint64_t peakProcessingTimeUs;
        size_t totalAllocatedBytes;
        size_t peakAllocatedBytes;
        int activeAllocations;
        int activeSourceCount;
        int peakActiveSourceCount;
        uint64_t totalSourcesCreated;
        uint64_t totalErrors;
        uint64_t criticalErrors;
        uint64_t recoveredErrors;
        bool globalRecoveryMode;
    };

    PerformanceStats getPerformanceStats() const
    {
        return {
            performanceState.totalFramesProcessed.load(),
            performanceState.averageProcessingTimeUs.load(),
            performanceState.peakProcessingTimeUs.load(),
            performanceState.totalAllocatedBytes.load(),
            performanceState.peakAllocatedBytes.load(),
            performanceState.activeAllocations.load(),
            sourceManager.activeSourceCount.load(),
            static_cast<int>(sourceManager.peakActiveSourceCount.load()),
            sourceManager.totalSourcesCreated.load(),
            errorState.totalErrors.load(),
            errorState.criticalErrors.load(),
            errorState.recoveredErrors.load(),
            errorState.globalRecoveryMode.load()
        };
    }

private:
    void initializeHRTFSettings()
    {
        hrtfState.settings.type = IPL_HRTFTYPE_DEFAULT;
        hrtfState.settings.volume = 1.0f;
        hrtfState.settings.normType = IPL_HRTFNORMTYPE_NONE;
        
        hrtfState.audioSettings.samplingRate = 44100;
        hrtfState.audioSettings.frameSize = 1024;
    }

    void initializeSimulationSettings()
    {
        simulationSettings.flags = static_cast<IPLSimulationFlags>(
            IPL_SIMULATIONFLAGS_DIRECT | 
            IPL_SIMULATIONFLAGS_REFLECTIONS | 
            IPL_SIMULATIONFLAGS_PATHING
        );
        simulationSettings.sceneType = IPL_SCENETYPE_DEFAULT;
        simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        simulationSettings.maxNumOcclusionSamples = 32;
        simulationSettings.maxNumRays = 4096;
        simulationSettings.numDiffuseSamples = 32;
        simulationSettings.maxDuration = 2.0f;
        simulationSettings.maxOrder = 1;
        simulationSettings.maxNumSources = 32;
        simulationSettings.numThreads = 1;
        simulationSettings.rayBatchSize = 16;
    }
};

// Global enhanced state instance
static std::unique_ptr<EnhancedGlobalState> gEnhancedState;
static std::mutex gEnhancedStateMutex;

// --------------------------------------------------------------------------------------------------------------------
// Enhanced API Functions
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initialize the enhanced Steam Audio FMOD Core integration.
 * @param contextSettings Steam Audio context settings.
 * @return True if initialization succeeded.
 */
bool InitializeEnhanced(const IPLContextSettings& contextSettings)
{
    std::lock_guard<std::mutex> lock(gEnhancedStateMutex);
    
    if (gEnhancedState)
    {
        return gEnhancedState->initialized.load();
    }

    try
    {
        gEnhancedState = std::make_unique<EnhancedGlobalState>();
        return gEnhancedState->initialize(contextSettings);
    }
    catch (const std::exception& e)
    {
        STEAMAUDIO_FMODCORE_REPORT_ERROR(
            ErrorCode::InitializationFailed,
            ErrorSeverity::Critical,
            "Exception during enhanced initialization: %s", e.what()
        );
        return false;
    }
}

/**
 * @brief Shutdown the enhanced Steam Audio FMOD Core integration.
 */
void ShutdownEnhanced()
{
    std::lock_guard<std::mutex> lock(gEnhancedStateMutex);
    
    if (gEnhancedState)
    {
        gEnhancedState->cleanup();
        gEnhancedState.reset();
    }
}

/**
 * @brief Get the enhanced global state (thread-safe).
 * @return Pointer to enhanced global state, or nullptr if not initialized.
 */
EnhancedGlobalState* GetEnhancedGlobalState()
{
    std::lock_guard<std::mutex> lock(gEnhancedStateMutex);
    return gEnhancedState.get();
}

/**
 * @brief Create a source with enhanced tracking.
 * @param sourceSettings Source settings.
 * @return Source ID, or -1 on failure.
 */
int CreateEnhancedSource(const IPLSourceSettings& sourceSettings)
{
    auto* state = GetEnhancedGlobalState();
    if (!state)
    {
        return -1;
    }
    
    return state->createSource(sourceSettings);
}

/**
 * @brief Destroy a source with enhanced tracking.
 * @param sourceId Source ID.
 * @return True if successful.
 */
bool DestroyEnhancedSource(int sourceId)
{
    auto* state = GetEnhancedGlobalState();
    if (!state)
    {
        return false;
    }
    
    return state->destroySource(sourceId);
}

/**
 * @brief Get a source with enhanced safety.
 * @param sourceId Source ID.
 * @return Source pointer, or nullptr if not found.
 */
IPLSource GetEnhancedSource(int sourceId)
{
    auto* state = GetEnhancedGlobalState();
    if (!state)
    {
        return nullptr;
    }
    
    return state->getSource(sourceId);
}

/**
 * @brief Update performance metrics.
 * @param processingTimeUs Processing time in microseconds.
 * @param frameCount Number of frames processed.
 */
void UpdateEnhancedPerformanceMetrics(uint64_t processingTimeUs, uint64_t frameCount)
{
    auto* state = GetEnhancedGlobalState();
    if (state)
    {
        state->performanceState.updateProcessingTime(processingTimeUs, frameCount);
    }
}

/**
 * @brief Update memory usage metrics.
 * @param bytes Number of bytes.
 * @param allocation True for allocation, false for deallocation.
 */
void UpdateEnhancedMemoryMetrics(size_t bytes, bool allocation)
{
    auto* state = GetEnhancedGlobalState();
    if (state)
    {
        state->performanceState.updateMemoryUsage(bytes, allocation);
    }
}

/**
 * @brief Report an error to the enhanced error tracking system.
 * @param severity Error severity.
 */
void ReportEnhancedError(ErrorSeverity severity)
{
    auto* state = GetEnhancedGlobalState();
    if (state)
    {
        state->errorState.reportError(severity);
    }
}

/**
 * @brief Report error recovery to the enhanced tracking system.
 */
void ReportEnhancedRecovery()
{
    auto* state = GetEnhancedGlobalState();
    if (state)
    {
        state->errorState.reportRecovery();
    }
}

/**
 * @brief Get comprehensive performance statistics.
 * @return Performance statistics, or empty stats if not initialized.
 */
EnhancedGlobalState::PerformanceStats GetEnhancedPerformanceStats()
{
    auto* state = GetEnhancedGlobalState();
    if (state)
    {
        return state->getPerformanceStats();
    }
    
    return {}; // Return empty stats
}

/**
 * @brief Check if the system is in global recovery mode.
 * @return True if in recovery mode.
 */
bool IsEnhancedRecoveryMode()
{
    auto* state = GetEnhancedGlobalState();
    return state ? state->errorState.globalRecoveryMode.load() : false;
}

} // namespace SteamAudioFMODCore