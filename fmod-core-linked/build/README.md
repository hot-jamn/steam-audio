# Steam Audio FMOD Core Enhanced Build System

This directory contains the enhanced build system for Steam Audio FMOD Core integration, supporting both the original implementation and the new enhanced version with comprehensive error handling, performance monitoring, and robustness improvements.

## Quick Start 

### Basic Enhanced Build (Recommended)
```bash
python build.py --enhanced --configuration release
```

### Development Build with All Features
```bash
python build.py --enhanced --configuration debug --enable-profiling --enable-detailed-logging --enable-testing
```

### Production Build with Monitoring
```bash
python build.py --enhanced --configuration release --enable-profiling --enable-error-recovery
```

## Command Line Options

### Basic Build Options
- `-p, --platform`: Target operating system (windows, osx, linux, android, ios, wasm)
- `-t, --toolchain`: Compiler toolchain (Windows only: vs2013, vs2015, vs2017, vs2019, vs2022)
- `-a, --architecture`: CPU architecture (x86, x64, armv7, arm64)
- `-c, --configuration`: Build configuration (debug, release)
- `-o, --operation`: CMake operation (generate, build, install, package, default, ci_build, ci_package)

### Enhanced FMOD Core Options
- `--enhanced`: Build enhanced version with comprehensive error handling and performance monitoring (default: enabled)
- `--no-enhanced`: Disable enhanced version and build original implementation only
- `--original`: Build original version alongside enhanced version (default: enabled)
- `--no-original`: Disable original version and build enhanced implementation only

### Enhanced Features (Fine-grained Control)
- `--enable-profiling`: Enable performance profiling with runtime toggleable monitoring
- `--enable-error-recovery`: Enable automatic error recovery mechanisms (default: enabled)
- `--enable-resource-tracking`: Enable resource leak detection and tracking (debug builds only)
- `--enable-thread-safety`: Enable thread-safe operations with atomic state management (default: enabled)
- `--enable-detailed-logging`: Enable detailed logging for production debugging
- `--enable-memory-validation`: Enable memory validation and bounds checking (debug builds only)
- `--enable-parameter-validation`: Enable comprehensive parameter validation (default: enabled)

### Build Optimizations
- `--enable-fast-math`: Enable fast math optimizations for audio processing (default: enabled)
- `--enable-sanitizers`: Enable address sanitizer in debug builds
- `--enable-warnings-as-errors`: Treat compiler warnings as errors
- `--enable-multiprocessor-build`: Enable multi-processor compilation (MSVC only, default: enabled)

### Testing & Development
- `--enable-testing`: Enable testing framework and build test targets
- `--enable-benchmarks`: Enable performance benchmarking targets
- `--enable-examples`: Build example applications and demos

## Build Examples

### 1. Standard Release Build
```bash
# Enhanced version with error recovery and thread safety
python build.py --platform windows --architecture x64 --configuration release
```

### 2. Debug Build with Full Diagnostics
```bash
# All enhanced features enabled for debugging
python build.py --configuration debug --enable-profiling --enable-resource-tracking --enable-memory-validation --enable-detailed-logging --enable-testing
```

### 3. Production Build with Performance Monitoring
```bash
# Optimized for production with runtime monitoring
python build.py --configuration release --enable-profiling --enable-error-recovery --enable-fast-math
```

### 4. Cross-Platform Android Build
```bash
# Android ARM64 build with enhanced features
python build.py --platform android --architecture arm64 --configuration release --enable-profiling
```

### 5. Development Build with Testing
```bash
# Full development environment with testing and examples
python build.py --configuration debug --enable-testing --enable-benchmarks --enable-examples --enable-detailed-logging
```

### 6. Original Implementation Only
```bash
# Build only the original Steam Audio FMOD Core implementation
python build.py --no-enhanced --original --configuration release
```

### 7. CI/CD Pipeline Build
```bash
# Continuous integration build with installation
python build.py --operation ci_build --configuration release --enable-profiling --enable-error-recovery
```

## Enhanced Features Overview

### Error Handling & Recovery
- **Comprehensive Error Handling**: Multi-level error management with graceful degradation
- **Automatic Recovery**: Intelligent error recovery mechanisms that maintain audio continuity
- **Parameter Validation**: Extensive input validation with detailed error reporting
- **Resource Management**: Automatic tracking and cleanup of audio resources

### Performance Monitoring
- **Real-time Metrics**: Zero-overhead performance monitoring (when disabled)
- **CPU Usage Tracking**: Per-component CPU usage measurement
- **Memory Monitoring**: Memory allocation tracking and leak detection
- **Audio Latency Metrics**: Real-time audio processing latency measurement

### Thread Safety
- **Atomic Operations**: Lock-free state management for critical audio paths
- **Thread-safe Initialization**: Safe multi-threaded plugin initialization
- **Concurrent Access Protection**: Safe concurrent access to shared audio resources

### Development & Debugging
- **Detailed Logging**: Comprehensive logging system for production debugging
- **Testing Framework**: Automated testing with validation targets
- **Benchmarking**: Performance benchmarking tools for optimization
- **Memory Validation**: Debug-time memory bounds checking and validation

## Build Configuration Summary

The build system automatically displays a comprehensive configuration summary:

```
============================================================
STEAM AUDIO FMOD CORE BUILD CONFIGURATION
============================================================
Platform: windows
Architecture: x64
Configuration: release
Operation: default
Toolchain: vs2022

Build Targets:
  Enhanced Version: YES
  Original Version: YES

Enhanced Features:
  Performance Profiling: YES
  Error Recovery: YES
  Resource Tracking: NO
  Thread Safety: YES
  Detailed Logging: NO
  Memory Validation: NO
  Parameter Validation: YES

Build Optimizations:
  Fast Math: YES
  Sanitizers: NO
  Warnings as Errors: NO
  Multi-processor Build: YES

Development Features:
  Testing Framework: NO
  Benchmarks: NO
  Examples: NO
============================================================
```

## Platform-Specific Notes

### Windows
- Supports Visual Studio 2013-2022
- Multi-processor compilation enabled by default
- Documentation generation on x64 builds
- Address sanitizer support in debug builds

### macOS/iOS
- Xcode project generation
- Universal binary support
- Code signing integration

### Linux
- GCC/Clang support
- 32-bit builds with explicit -m32 flags
- Package manager integration

### Android
- NDK toolchain integration
- Multiple architecture support (ARMv7, ARM64, x86, x64)
- Requires ANDROID_NDK environment variable

### WebAssembly
- Emscripten toolchain support
- Requires EMSDK environment variable
- Static library builds only

## Troubleshooting

### Common Issues

1. **Missing Dependencies**
   ```bash
   # Ensure Steam Audio core libraries are available
   # Check include/phonon and lib directories
   ```

2. **Android NDK Not Found**
   ```bash
   # Set ANDROID_NDK environment variable
   export ANDROID_NDK=/path/to/android-ndk
   ```

3. **WebAssembly Build Fails**
   ```bash
   # Ensure Emscripten SDK is installed and activated
   source /path/to/emsdk/emsdk_env.sh
   ```

4. **CMake Version Issues**
   ```bash
   # Build system requires CMake 3.17 or later
   # Automatic detection will find compatible version in tools directory
   ```

### Build Validation

The build system includes automatic validation:
- Checks for required environment variables
- Validates conflicting options
- Warns about suboptimal configurations
- Provides detailed error messages

### Getting Help

```bash
# Display all available options
python build.py --help

# View enhanced features help
python build.py --help | grep -A 20 "Enhanced Features"
```

## Integration with IDEs

### Visual Studio (Windows)
```bash
# Generate Visual Studio solution
python build.py --operation generate --platform windows --toolchain vs2022
# Open build/windows-vs2022-x64/SteamAudioFMODCore.sln
```

### Xcode (macOS)
```bash
# Generate Xcode project
python build.py --operation generate --platform osx
# Open build/osx/SteamAudioFMODCore.xcodeproj
```

### CLion/VS Code (Cross-platform)
```bash
# Generate compile_commands.json for IntelliSense
python build.py --operation generate
```

## Performance Considerations

### Release Builds
- Enhanced features add minimal overhead when disabled
- Performance monitoring can be toggled at runtime
- Fast math optimizations enabled by default
- Multi-processor compilation for faster builds

### Debug Builds
- Resource tracking and memory validation enabled automatically
- Address sanitizer support for memory error detection
- Detailed logging for comprehensive debugging
- Testing framework integration

## Contributing

When modifying the build system:
1. Test across multiple platforms
2. Ensure backward compatibility
3. Update documentation
4. Add appropriate validation
5. Test both enhanced and original builds

## License

Copyright 2017-2023 Valve Corporation. Licensed under the Apache License, Version 2.0.