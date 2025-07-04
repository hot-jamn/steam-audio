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

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Plugin Declarations
// --------------------------------------------------------------------------------------------------------------------

// Enhanced spatializer effect
extern FMOD_DSP_DESCRIPTION gEnhancedSpatializeEffect;

// Enhanced reverb effect
extern FMOD_DSP_DESCRIPTION gEnhancedReverbEffect;

// Enhanced mixer return effect
extern FMOD_DSP_DESCRIPTION gEnhancedMixerReturnEffect;

// Original effects (for compatibility) - only when building both versions
#ifdef STEAMAUDIO_FMODCORE_BUILD_ORIGINAL
extern FMOD_DSP_DESCRIPTION gSpatializeEffect;
extern FMOD_DSP_DESCRIPTION gReverbEffect;
extern FMOD_DSP_DESCRIPTION gMixerReturnEffect;
#endif

// --------------------------------------------------------------------------------------------------------------------
// Enhanced Plugin List
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Enhanced plugin list with comprehensive error handling and performance monitoring.
 *
 * This list includes enhanced versions of all Steam Audio FMOD Core effects, and optionally
 * original versions for backward compatibility when both are built.
 * Enhanced versions provide:
 * - Comprehensive error handling with graceful degradation
 * - Real-time performance monitoring with zero overhead when disabled
 * - Automatic resource management with leak detection
 * - Advanced parameter validation and recovery mechanisms
 * - Thread-safe operation with atomic state management
 * - Detailed logging and diagnostics for production debugging
 */

FMOD_PLUGINLIST gEnhancedPluginList[] =
{
    { FMOD_PLUGINTYPE_DSP, &gEnhancedSpatializeEffect },     // Enhanced Spatializer
    { FMOD_PLUGINTYPE_DSP, &gEnhancedReverbEffect },         // Enhanced Reverb
    { FMOD_PLUGINTYPE_DSP, &gEnhancedMixerReturnEffect },    // Enhanced Mixer Return


#ifdef STEAMAUDIO_FMODCORE_BUILD_ORIGINAL
    // Original effects (for backward compatibility)
    { FMOD_PLUGINTYPE_DSP, &gSpatializeEffect },             // Steam Audio Spatializer
    { FMOD_PLUGINTYPE_DSP, &gReverbEffect },                 // Steam Audio Reverb
    { FMOD_PLUGINTYPE_DSP, &gMixerReturnEffect },            // Steam Audio Mixer Return
#endif
        { FMOD_PLUGINTYPE_MAX, nullptr }

};

FMOD_DSP_DESCRIPTION* gEnhancedDescriptions[] =
{
    // Enhanced effects (recommended for production use)
    &gEnhancedSpatializeEffect,     // Steam Audio Enhanced Spatializer
    &gEnhancedReverbEffect,         // Steam Audio Enhanced Reverb
    &gEnhancedMixerReturnEffect,    // Steam Audio Enhanced Mixer Return
    
#ifdef STEAMAUDIO_FMODCORE_BUILD_ORIGINAL
    // Original effects (for backward compatibility)
    &gSpatializeEffect,             // Steam Audio Spatializer
    &gReverbEffect,                 // Steam Audio Reverb
    &gMixerReturnEffect,            // Steam Audio Mixer Return
#endif
};

/**
 * @brief Get the number of enhanced plugins available.
 * @return Number of plugins in the enhanced list (excluding null terminator).
 */
int GetEnhancedPluginCount()
{
    int count = 0;
    while (gEnhancedDescriptions[count] != nullptr)
    {
        count++;
    }
    return count;
}

/**
 * @brief Get a specific enhanced plugin by index.
 * @param index Plugin index (0-based).
 * @return Plugin description, or nullptr if index is out of range.
 */
FMOD_DSP_DESCRIPTION* GetEnhancedPlugin(int index)
{
    if (index < 0 || index >= GetEnhancedPluginCount())
    {
        return nullptr;
    }
    
    return gEnhancedDescriptions[index];
}

/**
 * @brief Find an enhanced plugin by name.
 * @param name Plugin name to search for.
 * @return Plugin description, or nullptr if not found.
 */
FMOD_DSP_DESCRIPTION* FindEnhancedPlugin(const char* name)
{
    if (!name)
    {
        return nullptr;
    }
    
    for (int i = 0; i < GetEnhancedPluginCount(); ++i)
    {
        auto* plugin = gEnhancedDescriptions[i];
        if (plugin && plugin->name && strcmp(plugin->name, name) == 0)
        {
            return plugin;
        }
    }
    
    return nullptr;
}

/**
 * @brief Check if a plugin is an enhanced version.
 * @param plugin Plugin description to check.
 * @return True if the plugin is an enhanced version.
 */
bool IsEnhancedPlugin(FMOD_DSP_DESCRIPTION* plugin)
{
    if (!plugin || !plugin->name)
    {
        return false;
    }
    
    // Enhanced plugins have "Enhanced" in their name
    return strstr(plugin->name, "Enhanced") != nullptr;
}

/**
 * @brief Get plugin information for diagnostics.
 * @param index Plugin index.
 * @return Plugin information structure.
 */
struct PluginInfo
{
    const char* name;
    unsigned int version;
    int numInputs;
    int numOutputs;
    int numParameters;
    bool isEnhanced;
    bool isAvailable;
};

PluginInfo GetEnhancedPluginInfo(int index)
{
    PluginInfo info = {};
    
    auto* plugin = GetEnhancedPlugin(index);
    if (plugin)
    {
        info.name = plugin->name ? plugin->name : "Unknown";
        info.version = plugin->version;
        info.numInputs = 1; // Enhanced plugins typically have 1 input
        info.numOutputs = 1; // Enhanced plugins typically have 1 output
        info.numParameters = plugin->numparameters;
        info.isEnhanced = IsEnhancedPlugin(plugin);
        info.isAvailable = true;
    }
    else
    {
        info.name = "Invalid";
        info.isAvailable = false;
    }
    
    return info;
}

/**
 * @brief Validate all enhanced plugins.
 * @return True if all plugins are valid and properly configured.
 */
bool ValidateEnhancedPlugins()
{
    bool allValid = true;
    
    for (int i = 0; i < GetEnhancedPluginCount(); ++i)
    {
        auto* plugin = gEnhancedDescriptions[i];
        if (!plugin)
        {
            allValid = false;
            continue;
        }
        
        // Validate plugin structure
        if (!plugin->name || strlen(plugin->name) == 0)
        {
            allValid = false;
            continue;
        }
        
        if (plugin->version == 0)
        {
            allValid = false;
            continue;
        }
        
        if (!plugin->create || !plugin->release || !plugin->process)
        {
            allValid = false;
            continue;
        }
        
        // Validate parameter descriptions
        if (plugin->numparameters > 0 && !plugin->paramdesc)
        {
            allValid = false;
            continue;
        }
        
        for (int j = 0; j < plugin->numparameters; ++j)
        {
            auto* param = plugin->paramdesc[j];
            if (!param || !param->name || strlen(param->name) == 0)
            {
                allValid = false;
                break;
            }
        }
    }
    
    return allValid;
}

/**
 * @brief Get enhanced plugin statistics for monitoring.
 * @return Plugin statistics structure.
 */
struct PluginStats
{
    int totalPlugins;
    int enhancedPlugins;
    int originalPlugins;
    bool allValid;
    const char* lastValidationError;
};

PluginStats GetEnhancedPluginStats()
{
    PluginStats stats = {};
    
    stats.totalPlugins = GetEnhancedPluginCount();
    stats.allValid = ValidateEnhancedPlugins();
    stats.lastValidationError = stats.allValid ? nullptr : "Plugin validation failed";
    
    // Count enhanced vs original plugins
    for (int i = 0; i < stats.totalPlugins; ++i)
    {
        auto* plugin = gEnhancedDescriptions[i];
        if (IsEnhancedPlugin(plugin))
        {
            stats.enhancedPlugins++;
        }
        else
        {
            stats.originalPlugins++;
        }
    }
    
    return stats;
}

/**
 * @brief Log enhanced plugin information for diagnostics.
 */
void LogEnhancedPluginInfo()
{
    auto stats = GetEnhancedPluginStats();
    
    STEAMAUDIO_FMODCORE_REPORT_ERROR(
        ErrorCode::Success,
        ErrorSeverity::Info,
        "Enhanced Plugin System: %d total plugins (%d enhanced, %d original), validation: %s",
        stats.totalPlugins,
        stats.enhancedPlugins,
        stats.originalPlugins,
        stats.allValid ? "PASSED" : "FAILED"
    );
    
    for (int i = 0; i < stats.totalPlugins; ++i)
    {
        auto info = GetEnhancedPluginInfo(i);
        if (info.isAvailable)
        {
            STEAMAUDIO_FMODCORE_REPORT_ERROR(
                ErrorCode::Success,
                ErrorSeverity::Info,
                "Plugin[%d]: %s v%u (%d->%d channels, %d params) %s",
                i,
                info.name,
                info.version,
                info.numInputs,
                info.numOutputs,
                info.numParameters,
                info.isEnhanced ? "[ENHANCED]" : "[ORIGINAL]"
            );
        }
    }
}
/*
FMOD_PLUGINLIST* F_CALL FMODGetPluginDescriptionList()
{
    using namespace SteamAudioFMODCore;

    // Log plugin information on first access
    static bool logged = false;
    if (!logged)
    {
        LogEnhancedPluginInfo();
        logged = true;
    }

    return gEnhancedPluginList;
}
*/

} // namespace SteamAudioFMODCore
// --------------------------------------------------------------------------------------------------------------------
// C API for FMOD Plugin Discovery
// --------------------------------------------------------------------------------------------------------------------

extern "C"
{
    /**
     * @brief Get the number of enhanced plugins.
     * @return Number of plugins available.
     */
    F_EXPORT int F_CALL FMODGetDSPDescriptionCount()
    {
        return SteamAudioFMODCore::GetEnhancedPluginCount();
    }
    
    /**
     * @brief Get a specific enhanced plugin by index.
     * @param index Plugin index (0-based).
     * @return Plugin description, or nullptr if index is out of range.
     */
    F_EXPORT FMOD_DSP_DESCRIPTION* F_CALL FMODGetEnhancedDSPDescription(int index)
    {
        return SteamAudioFMODCore::GetEnhancedPlugin(index);
    }
    
    /**
     * @brief Validate all enhanced plugins.
     * @return 1 if all plugins are valid, 0 otherwise.
     */
    F_EXPORT int F_CALL FMODValidateDSPDescriptions()
    {
        return SteamAudioFMODCore::ValidateEnhancedPlugins() ? 1 : 0;
    }
}