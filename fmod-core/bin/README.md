# Steam Audio FMOD Core Build Products

This directory contains the complete build output for Steam Audio FMOD Core integration, including native plugins, Unity integration scripts, headers, documentation, and debug symbols.

## Directory Structure

```
bin/
├── lib/                    # Native plugin libraries
│   └── windows-x64/        # Platform-specific binaries
├── symbols/                # Debug symbols and PDB files
│   └── windows-x64/        # Platform-specific debug symbols
├── include/                # Header files for development
│   └── SteamAudio/
│       └── fmod-core/      # FMOD Core integration headers
├── docs/                   # Documentation
│   └── fmod-core/          # Implementation guides and analysis
├── unity/                  # Unity integration components
│   ├── Scripts/            # C# scripts for Unity
│   └── Examples/           # Example implementations
└── root/                   # License and third-party notices
```

## Build Products Overview

### Native Plugin Libraries (`lib/windows-x64/`)

#### Core FMOD Core Plugins

1. **`phonon_fmodcore.dll`** - Original Steam Audio FMOD Core Plugin
   - Standard implementation with basic Steam Audio integration
   - Provides spatializer, reverb, and mixer return DSP effects
   - Compatible with existing FMOD Core projects
   - Smaller memory footprint and simpler implementation

2. **`phonon_fmodcore_enhanced.dll`** - Enhanced Steam Audio FMOD Core Plugin
   - Advanced implementation with comprehensive error handling
   - Real-time performance monitoring and diagnostics
   - Automatic error recovery mechanisms
   - Thread-safe operations with atomic state management
   - Advanced parameter validation and recovery
   - Detailed logging for production debugging
   - Resource leak detection and tracking

3. **`phonon.dll`** - Steam Audio Core Library
   - Core Steam Audio engine and simulation
   - Required dependency for both plugin versions
   - Handles 3D audio simulation, HRTF processing, and acoustic modeling

#### Import Libraries

- **`phonon_fmodcore.lib`** - Import library for original plugin
- **`phonon_fmodcore_enhanced.lib`** - Import library for enhanced plugin

### Debug Symbols (`symbols/windows-x64/`)

- **`phonon_fmodcore.pdb`** - Debug symbols for original plugin
- **`phonon_fmodcore_enhanced.pdb`** - Debug symbols for enhanced plugin

### Development Headers (`include/SteamAudio/fmod-core/`)

- **`steamaudio_fmodcore.h`** - Main integration header
- **`performance_monitor.h`** - Performance monitoring utilities (enhanced version)
- **`error_handling.h`** - Error handling framework (enhanced version)

### Documentation (`docs/fmod-core/`)

- **`code_quality_analysis.md`** - Code quality metrics and analysis
- **`enhanced_implementation_guide.md`** - Guide for enhanced features

## Plugin Comparison: Original vs Enhanced

### Original Plugin (`phonon_fmodcore.dll`)

**Use Cases:**
- Production environments requiring maximum stability
- Projects with strict memory constraints
- Simple integration scenarios
- Legacy compatibility requirements

**Features:**
- ✅ Core Steam Audio spatialization
- ✅ Reflection and reverb processing
- ✅ Basic parameter validation
- ✅ Standard FMOD Core DSP integration
- ✅ Minimal overhead

**Limitations:**
- ❌ Limited error recovery
- ❌ Basic logging only
- ❌ No performance monitoring
- ❌ Minimal runtime diagnostics

### Enhanced Plugin (`phonon_fmodcore_enhanced.dll`)

**Use Cases:**
- Development and debugging environments
- Production systems requiring robust error handling
- Applications needing detailed performance monitoring
- Complex integration scenarios

**Features:**
- ✅ All original plugin features
- ✅ Comprehensive error handling with graceful degradation
- ✅ Real-time performance monitoring (zero overhead when disabled)
- ✅ Automatic resource management with leak detection
- ✅ Thread-safe operations with atomic state management
- ✅ Advanced parameter validation and recovery mechanisms
- ✅ Detailed logging and diagnostics for production debugging
- ✅ Runtime toggleable monitoring and profiling

**Enhanced Capabilities:**
- **Error Recovery**: Automatically handles and recovers from common error conditions
- **Performance Monitoring**: Real-time tracking of DSP performance with minimal overhead
- **Resource Tracking**: Detects and prevents memory leaks and resource issues
- **Thread Safety**: Atomic operations ensure safe multi-threaded access
- **Advanced Validation**: Comprehensive parameter checking with automatic correction
- **Production Debugging**: Detailed logging that can be enabled in production builds

## FMOD Core DSP Effects

Both plugin versions provide the following DSP effects:

### 1. Steam Audio Spatializer (`steamaudio_fmodcore_spatialize`)

**Purpose**: Processes direct sound spatialization and environmental effects

**Parameters:**
- Source ID (integer): Links to Unity Steam Audio source
- Apply HRTF to Direct (boolean): Enable/disable HRTF for direct sound
- Direct Mix Level (float 0.0-1.0): Volume level for direct sound

**Features:**
- Distance attenuation modeling
- Air absorption simulation
- Source directivity patterns
- Occlusion and transmission effects
- Binaural HRTF processing

### 2. Steam Audio Reverb (`steamaudio_fmodcore_reverb`)

**Purpose**: Processes reflection simulation results and applies spatial reverb

**Parameters:**
- Source ID (integer): Links to Unity Steam Audio source
- Apply HRTF to Reflections (boolean): Enable/disable HRTF for reflections
- Reflections Mix Level (float 0.0-1.0): Volume level for reflections

**Features:**
- Real-time reflection simulation
- Binaural reverb rendering
- Environmental acoustic modeling
- Dynamic reverb parameter adjustment

### 3. Steam Audio Mixer Return (`steamaudio_fmodcore_mixreturn`)

**Purpose**: Processes mixed reflection and pathing audio with final spatial processing

**Parameters:**
- Source ID (integer): Links to Unity Steam Audio source
- Apply HRTF to Pathing (boolean): Enable/disable HRTF for pathing
- Pathing Mix Level (float 0.0-1.0): Volume level for pathing

**Features:**
- Ambisonics decoding
- Multi-source mixing
- Final spatial processing stage
- Pathing and portal effects

## Unity Integration Setup

### Prerequisites

1. **Steam Audio Unity Package**: Install Steam Audio for Unity
2. **FMOD Core Unity Integration**: Set up FMOD Core in your Unity project
3. **Native Plugins**: Copy the appropriate DLL files to your Unity project

### Installation Steps

#### 1. Copy Native Plugins

Copy the plugin files to your Unity project's plugin directory:

```
YourUnityProject/
├── Assets/
│   └── Plugins/
│       └── SteamAudio/
│           └── Binaries/
│               └── Windows/
│                   └── x86_64/
│                       ├── phonon_fmodcore.dll          # Original version
│                       ├── phonon_fmodcore_enhanced.dll # Enhanced version
│                       └── phonon.dll                   # Steam Audio core
```

#### 2. Configure Plugin Settings

In Unity's Plugin Inspector for each DLL:

**Platform Settings:**
- ✅ Windows x86_64
- ❌ Other platforms (unless you have builds for them)

**Settings:**
- SDK: Any SDK
- CPU: x86_64
- Placeholder: `Assets/Plugins/SteamAudio/Binaries/Windows/x86_64/`

#### 3. Add Unity Scripts

Copy the Unity integration scripts to your project:

```
YourUnityProject/
├── Assets/
│   └── Scripts/
│       └── SteamAudio/
│           ├── Runtime/
│           │   ├── SteamAudioFMODCore.cs
│           │   ├── SteamAudioFMODCoreSource.cs
│           │   └── SteamAudioFMODCoreManager.cs
│           └── Editor/
│               └── SteamAudioFMODCoreEditor.cs
```

### Unity Editor Setup

#### 1. Create FMOD Core Manager

1. In your scene, create an empty GameObject
2. Add the `SteamAudioFMODCoreManager` component
3. Configure settings:
   ```csharp
   Auto Initialize: true
   Auto Update Settings: true
   Update Interval: 1.0f
   Enable Debug Logging: false (enable for debugging)
   ```

#### 2. Configure Audio Sources

For each audio source that should use Steam Audio:

1. Ensure the GameObject has a `SteamAudioSource` component
2. Add the `SteamAudioFMODCoreSource` component
3. Configure FMOD Core integration settings:
   ```csharp
   Enable Spatialization: true
   Enable Reverb: true
   Enable Mixer Return: false
   
   Apply HRTF to Direct: true
   Apply HRTF to Reflections: true
   Apply HRTF to Pathing: true
   
   Direct Mix Level: 1.0
   Reflections Mix Level: 1.0
   Pathing Mix Level: 1.0
   ```

#### 3. FMOD Core DSP Chain Setup

In your FMOD Core project, set up the DSP chain for spatialized audio:

```
Audio Source
    ↓
Steam Audio Spatializer DSP
    ↓
Steam Audio Reverb DSP
    ↓
Steam Audio Mixer Return DSP (optional)
    ↓
Output
```

**DSP Configuration:**
1. Add `steamaudio_fmodcore_spatialize` to your event
2. Add `steamaudio_fmodcore_reverb` after the spatializer
3. Optionally add `steamaudio_fmodcore_mixreturn` for complex scenarios
4. Set the Source ID parameter to link with Unity sources

### Runtime Usage

#### Basic Integration

```csharp
// Initialize FMOD Core integration
SteamAudioFMODCore.Initialize();

// Sources with SteamAudioFMODCoreSource components
// will automatically register themselves

// Check status
if (SteamAudioFMODCore.IsInitialized)
{
    Debug.Log("FMOD Core integration ready");
}
```

#### Advanced Usage

```csharp
// Get the manager
var manager = SteamAudioFMODCoreManager.Instance;

// Monitor source count
int sourceCount = manager.GetRegisteredSourceCount();
Debug.Log($"Active sources: {sourceCount}");

// Force refresh all sources
manager.RefreshAllSources();

// Enable debug logging at runtime
manager.enableDebugLogging = true;
```

## Choosing Between Plugin Versions

### Use Original Plugin When:
- ✅ Maximum performance is critical
- ✅ Memory usage must be minimized
- ✅ Simple, stable integration is sufficient
- ✅ Legacy compatibility is required
- ✅ Production environment with minimal debugging needs

### Use Enhanced Plugin When:
- ✅ Development and debugging phases
- ✅ Complex integration scenarios
- ✅ Production systems requiring robust error handling
- ✅ Performance monitoring and optimization needed
- ✅ Detailed logging and diagnostics required
- ✅ Multi-threaded audio processing environments

### Switching Between Versions

You can switch between plugin versions by:

1. **Unity Plugin Settings**: Enable/disable the appropriate DLL in Unity's Plugin Inspector
2. **FMOD Core Project**: Update DSP effect names if they differ between versions
3. **Runtime Detection**: The Unity integration automatically detects which version is loaded

## Troubleshooting

### Common Issues

1. **Plugin Not Loading**
   - Verify DLL is in correct Unity plugin directory
   - Check platform settings in Unity Plugin Inspector
   - Ensure all dependencies (phonon.dll) are present

2. **No Audio Output**
   - Verify FMOD Core DSP chain is correctly configured
   - Check Source ID parameters are being set
   - Ensure Steam Audio Manager is initialized before FMOD Core integration

3. **Performance Issues**
   - Consider switching to original plugin for better performance
   - Disable debug logging in production
   - Adjust update interval in manager settings

### Debug Tools

**Enhanced Plugin Debug Features:**
- Real-time performance monitoring
- Detailed error logging
- Resource usage tracking
- Parameter validation reports

**Unity Integration Debug:**
- Enable debug logging in `SteamAudioFMODCoreManager`
- Use Inspector status monitoring
- Check source registration status

### Performance Optimization

**For Original Plugin:**
- Minimal configuration required
- Focus on efficient DSP chain setup

**For Enhanced Plugin:**
- Disable unused monitoring features
- Adjust logging levels appropriately
- Use performance monitoring to identify bottlenecks

## Version Information

- **Steam Audio Version**: 4.6.1
- **Build Configuration**: Release
- **Platform**: Windows x64
- **Toolchain**: Visual Studio 2022
- **Enhanced Features**: Error Recovery, Thread Safety, Parameter Validation

## License and Third-Party

See `root/THIRDPARTY.md` for complete license information and third-party acknowledgments.

---

For detailed Unity integration documentation, see [`unity/README.md`](unity/README.md).

For implementation guides and technical details, see the [`docs/fmod-core/`](docs/fmod-core/) directory.