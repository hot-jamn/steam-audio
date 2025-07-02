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

#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Error Codes and Types
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Steam Audio FMOD Core specific error codes.
 */
enum class ErrorCode
{
    Success = 0,                    ///< Operation completed successfully
    InvalidParameter,               ///< Invalid parameter passed to function
    InvalidState,                   ///< Object is in invalid state for operation
    InitializationFailed,           ///< Initialization failed
    ContextNotAvailable,            ///< Steam Audio context not available
    HRTFNotAvailable,              ///< HRTF not available
    MemoryAllocationFailed,         ///< Memory allocation failed
    AudioBufferAllocationFailed,    ///< Audio buffer allocation failed
    EffectCreationFailed,          ///< Steam Audio effect creation failed
    SourceCreationFailed,          ///< Steam Audio source creation failed
    ParameterValidationFailed,      ///< Parameter validation failed
    ThreadSafetyViolation,         ///< Thread safety violation detected
    ResourceLeakDetected,          ///< Resource leak detected
    PerformanceThresholdExceeded,  ///< Performance threshold exceeded
    UnexpectedError                ///< Unexpected error occurred
};

/**
 * @brief Error severity levels.
 */
enum class ErrorSeverity
{
    Info,       ///< Informational message
    Warning,    ///< Warning that doesn't prevent operation
    Error,      ///< Error that prevents operation
    Critical    ///< Critical error that may cause instability
};

/**
 * @brief Error context information.
 */
struct ErrorContext
{
    std::string function;           ///< Function where error occurred
    std::string file;              ///< Source file where error occurred
    int line;                      ///< Line number where error occurred
    std::string message;           ///< Human-readable error message
    ErrorCode code;                ///< Error code
    ErrorSeverity severity;        ///< Error severity
    std::chrono::system_clock::time_point timestamp; ///< When error occurred
    std::string additionalInfo;    ///< Additional context information

    ErrorContext(const std::string& func, const std::string& file, int line,
                 const std::string& msg, ErrorCode code, ErrorSeverity severity,
                 const std::string& info = "")
        : function(func)
        , file(file)
        , line(line)
        , message(msg)
        , code(code)
        , severity(severity)
        , timestamp(std::chrono::system_clock::now())
        , additionalInfo(info)
    {
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Parameter Validation
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Parameter validation utilities.
 */
class ParameterValidator
{
public:
    /**
     * @brief Validate that a pointer is not null.
     * @param ptr Pointer to validate.
     * @param paramName Name of parameter for error reporting.
     * @return True if valid, false otherwise.
     */
    static bool validateNotNull(const void* ptr, const std::string& paramName)
    {
        if (!ptr)
        {
            logValidationError("Parameter '" + paramName + "' cannot be null");
            return false;
        }
        return true;
    }

    /**
     * @brief Validate that a float value is within specified range.
     * @param value Value to validate.
     * @param minValue Minimum allowed value (inclusive).
     * @param maxValue Maximum allowed value (inclusive).
     * @param paramName Name of parameter for error reporting.
     * @return True if valid, false otherwise.
     */
    static bool validateRange(float value, float minValue, float maxValue, const std::string& paramName)
    {
        if (value < minValue || value > maxValue)
        {
            logValidationError("Parameter '" + paramName + "' value " + std::to_string(value) + 
                             " is outside valid range [" + std::to_string(minValue) + ", " + 
                             std::to_string(maxValue) + "]");
            return false;
        }
        return true;
    }

    /**
     * @brief Validate that an integer value is within specified range.
     * @param value Value to validate.
     * @param minValue Minimum allowed value (inclusive).
     * @param maxValue Maximum allowed value (inclusive).
     * @param paramName Name of parameter for error reporting.
     * @return True if valid, false otherwise.
     */
    static bool validateRange(int value, int minValue, int maxValue, const std::string& paramName)
    {
        if (value < minValue || value > maxValue)
        {
            logValidationError("Parameter '" + paramName + "' value " + std::to_string(value) + 
                             " is outside valid range [" + std::to_string(minValue) + ", " + 
                             std::to_string(maxValue) + "]");
            return false;
        }
        return true;
    }

    /**
     * @brief Validate that a buffer has sufficient size.
     * @param bufferSize Actual buffer size.
     * @param requiredSize Required buffer size.
     * @param bufferName Name of buffer for error reporting.
     * @return True if valid, false otherwise.
     */
    static bool validateBufferSize(size_t bufferSize, size_t requiredSize, const std::string& bufferName)
    {
        if (bufferSize < requiredSize)
        {
            logValidationError("Buffer '" + bufferName + "' size " + std::to_string(bufferSize) + 
                             " is smaller than required size " + std::to_string(requiredSize));
            return false;
        }
        return true;
    }

    /**
     * @brief Validate audio format parameters.
     * @param numChannels Number of audio channels.
     * @param sampleRate Sample rate in Hz.
     * @param frameSize Frame size in samples.
     * @return True if valid, false otherwise.
     */
    static bool validateAudioFormat(int numChannels, int sampleRate, int frameSize)
    {
        bool valid = true;
        
        if (!validateRange(numChannels, 1, 8, "numChannels"))
            valid = false;
            
        if (!validateRange(sampleRate, 8000, 192000, "sampleRate"))
            valid = false;
            
        if (!validateRange(frameSize, 1, 8192, "frameSize"))
            valid = false;
            
        return valid;
    }

private:
    static void logValidationError(const std::string& message);
};

// --------------------------------------------------------------------------------------------------------------------
// Resource Management and Leak Detection
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Resource tracker for detecting memory and object leaks.
 */
class ResourceTracker
{
public:
    /**
     * @brief Resource types that can be tracked.
     */
    enum class ResourceType
    {
        AudioBuffer,
        SteamAudioEffect,
        SteamAudioSource,
        HRTF,
        Context,
        Other
    };

    /**
     * @brief Resource information for tracking.
     */
    struct ResourceInfo
    {
        ResourceType type;
        void* address;
        size_t size;
        std::string description;
        std::chrono::system_clock::time_point creationTime;
        std::string creationLocation;

        ResourceInfo() = default;
        
        ResourceInfo(ResourceType t, void* addr, size_t sz, const std::string& desc, const std::string& location)
            : type(t), address(addr), size(sz), description(desc)
            , creationTime(std::chrono::system_clock::now()), creationLocation(location)
        {
        }
    };

    /**
     * @brief Get singleton instance of resource tracker.
     * @return Reference to singleton instance.
     */
    static ResourceTracker& getInstance()
    {
        static ResourceTracker instance;
        return instance;
    }

    /**
     * @brief Track a resource allocation.
     * @param type Type of resource.
     * @param address Address of allocated resource.
     * @param size Size of resource in bytes.
     * @param description Human-readable description.
     * @param location Source location where allocation occurred.
     */
    void trackAllocation(ResourceType type, void* address, size_t size, 
                        const std::string& description, const std::string& location)
    {
        if (!mTrackingEnabled.load())
            return;

        std::lock_guard<std::mutex> lock(mResourceMutex);
        mTrackedResources[address] = ResourceInfo(type, address, size, description, location);
        mTotalAllocations.fetch_add(1);
        mCurrentAllocations.fetch_add(1);
    }

    /**
     * @brief Track a resource deallocation.
     * @param address Address of deallocated resource.
     */
    void trackDeallocation(void* address)
    {
        if (!mTrackingEnabled.load())
            return;

        std::lock_guard<std::mutex> lock(mResourceMutex);
        auto it = mTrackedResources.find(address);
        if (it != mTrackedResources.end())
        {
            mTrackedResources.erase(it);
            mCurrentAllocations.fetch_sub(1);
        }
        else
        {
            // Potential double-free or untracked deallocation
            logResourceError("Attempted to deallocate untracked resource at address " + 
                           std::to_string(reinterpret_cast<uintptr_t>(address)));
        }
    }

    /**
     * @brief Check for resource leaks.
     * @return Vector of leaked resources.
     */
    std::vector<ResourceInfo> checkForLeaks() const
    {
        std::lock_guard<std::mutex> lock(mResourceMutex);
        std::vector<ResourceInfo> leaks;
        
        for (const auto& pair : mTrackedResources)
        {
            leaks.push_back(pair.second);
        }
        
        return leaks;
    }

    /**
     * @brief Get current allocation count.
     * @return Number of currently allocated resources.
     */
    size_t getCurrentAllocationCount() const
    {
        return mCurrentAllocations.load();
    }

    /**
     * @brief Get total allocation count.
     * @return Total number of allocations made.
     */
    size_t getTotalAllocationCount() const
    {
        return mTotalAllocations.load();
    }

    /**
     * @brief Enable or disable resource tracking.
     * @param enabled True to enable tracking, false to disable.
     */
    void setTrackingEnabled(bool enabled)
    {
        mTrackingEnabled.store(enabled);
    }

    /**
     * @brief Check if resource tracking is enabled.
     * @return True if tracking is enabled.
     */
    bool isTrackingEnabled() const
    {
        return mTrackingEnabled.load();
    }

private:
    mutable std::mutex mResourceMutex;
    std::unordered_map<void*, ResourceInfo> mTrackedResources;
    std::atomic<size_t> mTotalAllocations{ 0 };
    std::atomic<size_t> mCurrentAllocations{ 0 };
    std::atomic<bool> mTrackingEnabled{ false };

    ResourceTracker() = default;
    ~ResourceTracker() = default;
    ResourceTracker(const ResourceTracker&) = delete;
    ResourceTracker& operator=(const ResourceTracker&) = delete;

    void logResourceError(const std::string& message);
};

// --------------------------------------------------------------------------------------------------------------------
// Error Handler and Logging
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Error handler for managing and reporting errors.
 */
class ErrorHandler
{
public:
    /**
     * @brief Error callback function type.
     */
    using ErrorCallback = std::function<void(const ErrorContext&)>;

    /**
     * @brief Get singleton instance of error handler.
     * @return Reference to singleton instance.
     */
    static ErrorHandler& getInstance()
    {
        static ErrorHandler instance;
        return instance;
    }

    /**
     * @brief Report an error.
     * @param context Error context information.
     */
    void reportError(const ErrorContext& context)
    {
        // Update error statistics
        mTotalErrors.fetch_add(1);
        if (context.severity == ErrorSeverity::Critical)
        {
            mCriticalErrors.fetch_add(1);
        }

        // Store recent errors (limited history)
        {
            std::lock_guard<std::mutex> lock(mErrorHistoryMutex);
            mErrorHistory.push_back(context);
            if (mErrorHistory.size() > mMaxErrorHistory)
            {
                mErrorHistory.erase(mErrorHistory.begin());
            }
        }

        // Call registered callbacks
        {
            std::lock_guard<std::mutex> lock(mCallbackMutex);
            for (const auto& callback : mErrorCallbacks)
            {
                try
                {
                    callback(context);
                }
                catch (...)
                {
                    // Ignore exceptions in error callbacks to prevent infinite recursion
                }
            }
        }
    }

    /**
     * @brief Register an error callback.
     * @param callback Callback function to register.
     */
    void registerErrorCallback(const ErrorCallback& callback)
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mErrorCallbacks.push_back(callback);
    }

    /**
     * @brief Clear all error callbacks.
     */
    void clearErrorCallbacks()
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mErrorCallbacks.clear();
    }

    /**
     * @brief Get recent error history.
     * @return Vector of recent errors.
     */
    std::vector<ErrorContext> getErrorHistory() const
    {
        std::lock_guard<std::mutex> lock(mErrorHistoryMutex);
        return mErrorHistory;
    }

    /**
     * @brief Get total error count.
     * @return Total number of errors reported.
     */
    size_t getTotalErrorCount() const
    {
        return mTotalErrors.load();
    }

    /**
     * @brief Get critical error count.
     * @return Number of critical errors reported.
     */
    size_t getCriticalErrorCount() const
    {
        return mCriticalErrors.load();
    }

    /**
     * @brief Clear error statistics and history.
     */
    void clearErrorHistory()
    {
        std::lock_guard<std::mutex> lock(mErrorHistoryMutex);
        mErrorHistory.clear();
        mTotalErrors.store(0);
        mCriticalErrors.store(0);
    }

private:
    mutable std::mutex mErrorHistoryMutex;
    mutable std::mutex mCallbackMutex;
    std::vector<ErrorContext> mErrorHistory;
    std::vector<ErrorCallback> mErrorCallbacks;
    std::atomic<size_t> mTotalErrors{ 0 };
    std::atomic<size_t> mCriticalErrors{ 0 };
    static constexpr size_t mMaxErrorHistory = 100;

    ErrorHandler() = default;
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
};

// --------------------------------------------------------------------------------------------------------------------
// Convenience Macros
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Macro to report an error with automatic source location.
 */
#define STEAMAUDIO_FMODCORE_REPORT_ERROR(code, severity, message, ...) \
    do { \
        char buffer[1024]; \
        snprintf(buffer, sizeof(buffer), message, ##__VA_ARGS__); \
        SteamAudioFMODCore::ErrorContext errorContext(__FUNCTION__, __FILE__, __LINE__, \
                                                buffer, code, severity); \
        SteamAudioFMODCore::ErrorHandler::getInstance().reportError(errorContext); \
    } while(0)

/**
 * @brief Macro to validate a parameter and report error if invalid.
 */
#define STEAMAUDIO_FMODCORE_VALIDATE_PARAM(condition, paramName) \
    do { \
        if (!(condition)) { \
            STEAMAUDIO_FMODCORE_REPORT_ERROR( \
                SteamAudioFMODCore::ErrorCode::InvalidParameter, \
                SteamAudioFMODCore::ErrorSeverity::Error, \
                "Parameter validation failed: %s", paramName); \
            return FMOD_ERR_INVALID_PARAM; \
        } \
    } while(0)

/**
 * @brief Macro to validate a pointer parameter.
 */
#define STEAMAUDIO_FMODCORE_VALIDATE_PTR(ptr, paramName) \
    STEAMAUDIO_FMODCORE_VALIDATE_PARAM( \
        SteamAudioFMODCore::ParameterValidator::validateNotNull(ptr, paramName), \
        paramName)

/**
 * @brief Macro to track resource allocation.
 */
#define STEAMAUDIO_FMODCORE_TRACK_ALLOC(type, address, size, description) \
    SteamAudioFMODCore::ResourceTracker::getInstance().trackAllocation( \
        type, address, size, description, __FUNCTION__ ":" + std::to_string(__LINE__))

/**
 * @brief Macro to track resource deallocation.
 */
#define STEAMAUDIO_FMODCORE_TRACK_DEALLOC(address) \
    SteamAudioFMODCore::ResourceTracker::getInstance().trackDeallocation(address)

} // namespace SteamAudioFMODCore