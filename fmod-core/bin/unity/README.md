# Steam Audio FMOD Core Unity Integration

This directory contains Unity C# scripts for integrating Steam Audio with FMOD Core API. The integration allows you to use Unity's Steam Audio probe placement and baking workflow while utilizing FMOD Core for runtime audio processing and spatialization.

## Overview

The FMOD Core integration provides:
- **Context Sharing**: Shares Steam Audio context between Unity and FMOD Core plugins
- **Source Management**: Manages Steam Audio sources for FMOD Core DSP effects
- **Parameter Synchronization**: Automatically synchronizes Steam Audio simulation results with FMOD Core parameters
- **Unity Inspector Integration**: Provides easy-to-use Unity components with Inspector UI

## Components

### Core Integration Classes

#### `SteamAudioFMODCore`
Static class that provides the core integration functionality:
- Initializes FMOD Core integration with Steam Audio context
- Manages source registration and parameter updates
- Handles context and HRTF sharing between Unity and FMOD Core

#### `SteamAudioFMODCoreSource`
Unity component for individual audio sources:
- Connects Unity's `SteamAudioSource` with FMOD Core DSP effects
- Manages source registration and parameter updates
- Provides Inspector UI for configuration

#### `SteamAudioFMODCoreManager`
Singleton manager component:
- Handles overall integration lifecycle
- Manages multiple sources
- Provides automatic initialization and settings updates
- Includes debug and monitoring features

### Editor Integration

#### `SteamAudioFMODCoreEditor`
Custom Inspector editors:
- Enhanced UI for FMOD Core components
- Status monitoring and debugging tools
- Menu items for easy setup and management

## Setup Instructions

### 1. Prerequisites

Before using the FMOD Core integration, ensure you have:
- Steam Audio Unity integration installed and configured
- FMOD Core API integrated in your project
- Steam Audio FMOD Core native plugins built and available

### 2. Basic Setup

1. **Add Manager to Scene**:
   ```
   Steam Audio → FMOD Core → Create Manager
   ```
   This creates a `SteamAudioFMODCoreManager` in your scene.

2. **Configure Audio Sources**:
   - Select GameObjects with audio sources
   - Use menu: `Steam Audio → FMOD Core → Add Source Component`
   - Or manually add `SteamAudioFMODCoreSource` component

3. **Initialize Integration**:
   - The manager will auto-initialize by default
   - Or manually call: `Steam Audio → FMOD Core → Initialize Integration`

### 3. Component Configuration

#### SteamAudioFMODCoreManager Settings

```csharp
public class SteamAudioFMODCoreManager : MonoBehaviour
{
    [Header("Integration Settings")]
    public bool autoInitialize = true;           // Auto-initialize on Start
    public bool autoUpdateSettings = true;       // Auto-update settings
    public float updateInterval = 1.0f;          // Update frequency
    
    [Header("Debug")]
    public bool enableDebugLogging = false;      // Debug output
}
```

#### SteamAudioFMODCoreSource Settings

```csharp
public class SteamAudioFMODCoreSource : MonoBehaviour
{
    [Header("FMOD Core Integration")]
    public bool enableSpatialization = true;     // Enable spatializer DSP
    public bool enableReverb = true;             // Enable reverb DSP
    public bool enableMixerReturn = false;       // Enable mixer return DSP
    
    [Header("DSP Parameters")]
    public bool applyHRTFToDirect = true;        // HRTF for direct sound
    public bool applyHRTFToReflections = true;   // HRTF for reflections
    public bool applyHRTFToPathing = true;       // HRTF for pathing
    
    [Header("Mix Levels")]
    [Range(0.0f, 1.0f)]
    public float directMixLevel = 1.0f;          // Direct sound level
    [Range(0.0f, 1.0f)]
    public float reflectionsMixLevel = 1.0f;     // Reflections level
    [Range(0.0f, 1.0f)]
    public float pathingMixLevel = 1.0f;         // Pathing level
}
```

## Usage Examples

### Basic Usage

```csharp
// Initialize FMOD Core integration
SteamAudioFMODCore.Initialize();

// Add a source to FMOD Core integration
var steamAudioSource = GetComponent<SteamAudioSource>();
int sourceId = SteamAudioFMODCore.AddSource(steamAudioSource);

// Remove source when done
SteamAudioFMODCore.RemoveSource(sourceId);
```

### Manager-Based Usage

```csharp
// Ensure manager exists
var manager = SteamAudioFMODCoreManager.EnsureManager();

// Manager handles initialization automatically
// Sources register themselves automatically when enabled

// Force refresh all sources
manager.RefreshAllSources();

// Get status information
bool isInitialized = SteamAudioFMODCore.IsInitialized;
int sourceCount = manager.GetRegisteredSourceCount();
```

### Runtime Configuration

```csharp
// Get FMOD Core source component
var fmodCoreSource = GetComponent<SteamAudioFMODCoreSource>();

// Check registration status
if (fmodCoreSource.IsRegistered)
{
    int sourceId = fmodCoreSource.GetSourceId();
    Debug.Log($"Source registered with ID: {sourceId}");
}

// Refresh registration
fmodCoreSource.RefreshRegistration();

// Update settings
fmodCoreSource.directMixLevel = 0.8f;
fmodCoreSource.enableReverb = false;
```

## Integration Architecture

### Context Sharing

The integration shares Steam Audio context between Unity and FMOD Core:

```
Unity Steam Audio Manager
    ↓ (shares context)
FMOD Core Integration
    ↓ (provides context to)
FMOD Core DSP Plugins
```

### Source Management

Sources are managed through a handle-based system:

1. Unity `SteamAudioSource` is registered with FMOD Core integration
2. Integration returns a source ID handle
3. FMOD Core DSP effects use the source ID to access Steam Audio simulation data
4. Parameters are automatically synchronized between Unity and FMOD Core

### Data Flow

```
Unity Scene → Steam Audio Simulation → Unity SteamAudioSource
                                            ↓
FMOD Core Integration ← Source Registration ←
        ↓
FMOD Core DSP Effects ← Parameter Updates ←
        ↓
Audio Output
```

## Troubleshooting

### Common Issues

1. **"FMOD Core integration not initialized"**
   - Ensure `SteamAudioFMODCoreManager` is in the scene
   - Check that Steam Audio Manager is initialized first
   - Try manual initialization: `SteamAudioFMODCore.Initialize()`

2. **"Source not registered"**
   - Ensure `SteamAudioSource` component exists
   - Check that FMOD Core integration is initialized
   - Try refreshing registration: `source.RefreshRegistration()`

3. **"Steam Audio Manager not available"**
   - Ensure Steam Audio is properly set up in the scene
   - Check Steam Audio Settings configuration
   - Verify Steam Audio Manager exists and is initialized

### Debug Tools

Enable debug logging in `SteamAudioFMODCoreManager`:
```csharp
manager.enableDebugLogging = true;
```

Use Inspector tools:
- Status monitoring in component inspectors
- Source registration status
- Manual control buttons for testing

### Performance Considerations

- The integration updates settings periodically (default: 1 second)
- Adjust `updateInterval` in manager for different update frequencies
- Disable `autoUpdateSettings` if you don't need automatic synchronization
- Use `RefreshAllSources()` sparingly as it re-registers all sources

## API Reference

### SteamAudioFMODCore Static Methods

```csharp
// Initialization
static void Initialize()
static void UpdateSettings()

// Source Management
static int AddSource(SteamAudioSource steamAudioSource)
static void RemoveSource(int sourceId)

// Status
static bool IsInitialized { get; }
```

### SteamAudioFMODCoreSource Methods

```csharp
// Registration
void RefreshRegistration()

// Status
int GetSourceId()
bool IsRegistered { get; }
SteamAudioSource GetSteamAudioSource()
```

### SteamAudioFMODCoreManager Methods

```csharp
// Initialization
void InitializeFMODCore()
void UpdateSettingsIfNeeded()

// Source Management
void RegisterSource(SteamAudioFMODCoreSource source)
void UnregisterSource(SteamAudioFMODCoreSource source)
void RefreshAllSources()

// Status
int GetRegisteredSourceCount()
List<SteamAudioFMODCoreSource> GetRegisteredSources()

// Static Utilities
static SteamAudioFMODCoreManager EnsureManager()
static bool HasInstance { get; }
```

## Integration with FMOD Core DSP Effects

The Unity integration works with the following FMOD Core DSP effects:

1. **Steam Audio Spatializer** (`steamaudio_fmodcore_spatialize`)
   - Handles direct sound spatialization
   - Applies distance attenuation, air absorption, directivity
   - Processes occlusion and transmission effects

2. **Steam Audio Reverb** (`steamaudio_fmodcore_reverb`)
   - Processes reflection simulation results
   - Applies binaural reverb rendering
   - Handles reflection mix levels

3. **Steam Audio Mixer Return** (`steamaudio_fmodcore_mixreturn`)
   - Processes mixed reflection and pathing audio
   - Handles Ambisonics decoding
   - Applies final spatial processing

Each DSP effect receives the source ID parameter to access the appropriate Steam Audio simulation data managed by the Unity integration.