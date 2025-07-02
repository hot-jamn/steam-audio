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

#pragma once

#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Performance Monitoring and Profiling
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Performance metrics for monitoring DSP processing performance.
 */
struct PerformanceMetrics
{
    std::atomic<uint64_t> totalProcessCalls{ 0 };           ///< Total number of process calls
    std::atomic<uint64_t> totalProcessTimeNs{ 0 };         ///< Total processing time in nanoseconds
    std::atomic<uint64_t> maxProcessTimeNs{ 0 };           ///< Maximum single process time in nanoseconds
    std::atomic<uint64_t> minProcessTimeNs{ UINT64_MAX };  ///< Minimum single process time in nanoseconds
    std::atomic<uint64_t> totalSamplesProcessed{ 0 };      ///< Total number of audio samples processed
    std::atomic<uint64_t> memoryAllocations{ 0 };          ///< Number of memory allocations
    std::atomic<uint64_t> memoryDeallocations{ 0 };        ///< Number of memory deallocations
    std::atomic<uint64_t> totalMemoryAllocated{ 0 };       ///< Total memory allocated in bytes
    std::atomic<uint64_t> peakMemoryUsage{ 0 };            ///< Peak memory usage in bytes
    std::atomic<uint64_t> currentMemoryUsage{ 0 };         ///< Current memory usage in bytes

    // Default constructor
    PerformanceMetrics() = default;

    /**
     * @brief Get average processing time per call in nanoseconds.
     * @return Average processing time, or 0 if no calls have been made.
     */
    double getAverageProcessTimeNs() const
    {
        auto calls = totalProcessCalls.load();
        return calls > 0 ? static_cast<double>(totalProcessTimeNs.load()) / calls : 0.0;
    }

    /**
     * @brief Get processing efficiency as samples per nanosecond.
     * @return Samples processed per nanosecond, or 0 if no processing time recorded.
     */
    double getProcessingEfficiency() const
    {
        auto time = totalProcessTimeNs.load();
        return time > 0 ? static_cast<double>(totalSamplesProcessed.load()) / time : 0.0;
    }

    /**
     * @brief Get memory efficiency as allocations per megabyte.
     * @return Allocations per MB, or 0 if no memory allocated.
     */
    double getMemoryEfficiency() const
    {
        auto memory = totalMemoryAllocated.load();
        return memory > 0 ? static_cast<double>(memoryAllocations.load()) / (memory / 1024.0 / 1024.0) : 0.0;
    }

    /**
     * @brief Reset all metrics to initial state.
     */
    void reset()
    {
        totalProcessCalls.store(0);
        totalProcessTimeNs.store(0);
        maxProcessTimeNs.store(0);
        minProcessTimeNs.store(UINT64_MAX);
        totalSamplesProcessed.store(0);
        memoryAllocations.store(0);
        memoryDeallocations.store(0);
        totalMemoryAllocated.store(0);
        peakMemoryUsage.store(0);
        currentMemoryUsage.store(0);
    }
    // Copy constructor for use in containers
    PerformanceMetrics(const PerformanceMetrics& other)
        : totalProcessCalls(other.totalProcessCalls.load())
        , totalProcessTimeNs(other.totalProcessTimeNs.load())
        , maxProcessTimeNs(other.maxProcessTimeNs.load())
        , minProcessTimeNs(other.minProcessTimeNs.load())
        , totalSamplesProcessed(other.totalSamplesProcessed.load())
        , memoryAllocations(other.memoryAllocations.load())
        , memoryDeallocations(other.memoryDeallocations.load())
        , totalMemoryAllocated(other.totalMemoryAllocated.load())
        , peakMemoryUsage(other.peakMemoryUsage.load())
        , currentMemoryUsage(other.currentMemoryUsage.load())
    {
    }

    // Assignment operator
    PerformanceMetrics& operator=(const PerformanceMetrics& other)
    {
        if (this != &other)
        {
            totalProcessCalls.store(other.totalProcessCalls.load());
            totalProcessTimeNs.store(other.totalProcessTimeNs.load());
            maxProcessTimeNs.store(other.maxProcessTimeNs.load());
            minProcessTimeNs.store(other.minProcessTimeNs.load());
            totalSamplesProcessed.store(other.totalSamplesProcessed.load());
            memoryAllocations.store(other.memoryAllocations.load());
            memoryDeallocations.store(other.memoryDeallocations.load());
            totalMemoryAllocated.store(other.totalMemoryAllocated.load());
            peakMemoryUsage.store(other.peakMemoryUsage.load());
            currentMemoryUsage.store(other.currentMemoryUsage.load());
        }
        return *this;
    }
};

/**
 * @brief RAII timer for measuring processing time.
 */
class ScopedTimer
{
public:
    /**
     * @brief Constructor starts the timer.
     * @param metrics Metrics object to update when timer is destroyed.
     * @param sampleCount Number of samples being processed (for efficiency calculation).
     */
    explicit ScopedTimer(PerformanceMetrics& metrics, uint32_t sampleCount = 0)
        : mMetrics(metrics)
        , mSampleCount(sampleCount)
        , mStartTime(std::chrono::high_resolution_clock::now())
    {
    }

    /**
     * @brief Destructor stops the timer and updates metrics.
     */
    ~ScopedTimer()
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - mStartTime);
        auto durationNs = static_cast<uint64_t>(duration.count());

        // Update metrics atomically
        mMetrics.totalProcessCalls.fetch_add(1);
        mMetrics.totalProcessTimeNs.fetch_add(durationNs);
        mMetrics.totalSamplesProcessed.fetch_add(mSampleCount);

        // Update min/max with compare-and-swap loop
        updateMinTime(durationNs);
        updateMaxTime(durationNs);
    }

private:
    PerformanceMetrics& mMetrics;
    uint32_t mSampleCount;
    std::chrono::high_resolution_clock::time_point mStartTime;

    void updateMinTime(uint64_t newTime)
    {
        uint64_t currentMin = mMetrics.minProcessTimeNs.load();
        while (newTime < currentMin && 
               !mMetrics.minProcessTimeNs.compare_exchange_weak(currentMin, newTime))
        {
            // Loop until successful update or newTime is no longer minimum
        }
    }

    void updateMaxTime(uint64_t newTime)
    {
        uint64_t currentMax = mMetrics.maxProcessTimeNs.load();
        while (newTime > currentMax && 
               !mMetrics.maxProcessTimeNs.compare_exchange_weak(currentMax, newTime))
        {
            // Loop until successful update or newTime is no longer maximum
        }
    }
};

/**
 * @brief Memory allocation tracker for monitoring memory usage.
 */
class MemoryTracker
{
public:
    /**
     * @brief Record a memory allocation.
     * @param size Size of allocation in bytes.
     * @param metrics Metrics object to update.
     */
    static void recordAllocation(size_t size, PerformanceMetrics& metrics)
    {
        metrics.memoryAllocations.fetch_add(1);
        metrics.totalMemoryAllocated.fetch_add(size);
        
        auto newUsage = metrics.currentMemoryUsage.fetch_add(size) + size;
        updatePeakMemory(newUsage, metrics);
    }

    /**
     * @brief Record a memory deallocation.
     * @param size Size of deallocation in bytes.
     * @param metrics Metrics object to update.
     */
    static void recordDeallocation(size_t size, PerformanceMetrics& metrics)
    {
        metrics.memoryDeallocations.fetch_add(1);
        metrics.currentMemoryUsage.fetch_sub(size);
    }

private:
    static void updatePeakMemory(uint64_t newUsage, PerformanceMetrics& metrics)
    {
        uint64_t currentPeak = metrics.peakMemoryUsage.load();
        while (newUsage > currentPeak && 
               !metrics.peakMemoryUsage.compare_exchange_weak(currentPeak, newUsage))
        {
            // Loop until successful update or newUsage is no longer peak
        }
    }
};

/**
 * @brief Performance monitor for tracking multiple DSP effects.
 */
class PerformanceMonitor
{
public:
    /**
     * @brief Get singleton instance of performance monitor.
     * @return Reference to singleton instance.
     */
    static PerformanceMonitor& getInstance()
    {
        static PerformanceMonitor instance;
        return instance;
    }

    /**
     * @brief Get metrics for a specific effect type.
     * @param effectName Name of the effect (e.g., "Spatializer", "Reverb").
     * @return Reference to metrics for the effect.
     */
    PerformanceMetrics& getMetrics(const std::string& effectName)
    {
        std::lock_guard<std::mutex> lock(mMetricsMutex);
        return mEffectMetrics[effectName];
    }

    /**
     * @brief Reset all metrics for all effects.
     */
    void resetAllMetrics()
    {
        std::lock_guard<std::mutex> lock(mMetricsMutex);
        for (auto& pair : mEffectMetrics)
        {
            pair.second.reset();
        }
    }

    /**
     * @brief Get metrics for all effects.
     * @return Map of effect names to their metrics.
     */
    std::unordered_map<std::string, PerformanceMetrics> getAllMetrics() const
    {
        std::lock_guard<std::mutex> lock(mMetricsMutex);
        return mEffectMetrics;
    }

    /**
     * @brief Check if performance monitoring is enabled.
     * @return True if monitoring is enabled.
     */
    bool isMonitoringEnabled() const
    {
        return mMonitoringEnabled.load();
    }

    /**
     * @brief Enable or disable performance monitoring.
     * @param enabled True to enable monitoring, false to disable.
     */
    void setMonitoringEnabled(bool enabled)
    {
        mMonitoringEnabled.store(enabled);
    }

private:
    mutable std::mutex mMetricsMutex;
    std::unordered_map<std::string, PerformanceMetrics> mEffectMetrics;
    std::atomic<bool> mMonitoringEnabled{ false };

    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
};

// --------------------------------------------------------------------------------------------------------------------
// Convenience Macros
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Macro to create a scoped timer for performance measurement.
 * @param effectName Name of the effect being measured.
 * @param sampleCount Number of samples being processed.
 */
#define STEAMAUDIO_FMODCORE_PROFILE(effectName, sampleCount) \
    SteamAudioFMODCore::ScopedTimer timer( \
        SteamAudioFMODCore::PerformanceMonitor::getInstance().getMetrics(effectName), \
        sampleCount \
    ); \
    (void)timer; // Suppress unused variable warning

/**
 * @brief Macro to record memory allocation.
 * @param effectName Name of the effect allocating memory.
 * @param size Size of allocation in bytes.
 */
#define STEAMAUDIO_FMODCORE_RECORD_ALLOC(effectName, size) \
    SteamAudioFMODCore::MemoryTracker::recordAllocation( \
        size, \
        SteamAudioFMODCore::PerformanceMonitor::getInstance().getMetrics(effectName) \
    )

/**
 * @brief Macro to record memory deallocation.
 * @param effectName Name of the effect deallocating memory.
 * @param size Size of deallocation in bytes.
 */
#define STEAMAUDIO_FMODCORE_RECORD_DEALLOC(effectName, size) \
    SteamAudioFMODCore::MemoryTracker::recordDeallocation( \
        size, \
        SteamAudioFMODCore::PerformanceMonitor::getInstance().getMetrics(effectName) \
    )

/**
 * @brief Macro to conditionally profile based on monitoring state.
 * @param effectName Name of the effect being measured.
 * @param sampleCount Number of samples being processed.
 */
#define STEAMAUDIO_FMODCORE_PROFILE_CONDITIONAL(effectName, sampleCount) \
    std::unique_ptr<SteamAudioFMODCore::ScopedTimer> timer; \
    if (SteamAudioFMODCore::PerformanceMonitor::getInstance().isMonitoringEnabled()) { \
        timer = std::make_unique<SteamAudioFMODCore::ScopedTimer>( \
            SteamAudioFMODCore::PerformanceMonitor::getInstance().getMetrics(effectName), \
            sampleCount \
        ); \
    }

} // namespace SteamAudioFMODCore