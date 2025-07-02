# Steam Audio FMOD Plugin - Debug Build

## Build Information

**Build Date:** June 26, 2025  
**Build Configuration:** Debug  
**Platform:** Windows x64  
**Compiler:** MSVC 19.44.35209.0 (Visual Studio 2022)  
**CMake Version:** 4.0.3  

## Build Process

1. **Repository:** Hot-jamn fork of Steam Audio (https://github.com/hot-jamn/steam-audio.git)
2. **Dependencies Downloaded:** Steam Audio API 4.6.0 libraries via `python setup.py`
3. **Build System Generated:** Visual Studio 2022 solution using CMake
4. **Build Command:** `cmake --build . --config Debug`

## Output Files

### Debug Plugin
- **Location:** `/f/fmod/steam-audio/fmod/bin/debug/`
- **Main DLL:** `phonon_fmod.dll` (1.91 MB)
- **Debug Symbols:** `phonon_fmod.pdb` (10.5 MB)

### Build Artifacts
- **Solution File:** `build/windows-vs2022-x64-debug/Phonon.sln`
- **Project File:** `build/windows-vs2022-x64-debug/src/phonon_fmod.vcxproj`

## Debug Features

This debug build includes:
- **Runtime Checks** (`/RTC1`)
- **Debug Symbols** (Full PDB file generated)
- **Debug Runtime Library** (`/MTd`)
- **No Optimizations** (Debug configuration)
- **Debug Preprocessor Definitions** (`_DEBUG`)

## Usage

To use this debug plugin in FMOD Studio:
1. Copy `phonon_fmod.dll` to your FMOD Studio plugins directory
2. Ensure `phonon_fmod.pdb` is in the same directory for debugging
3. The plugin will provide enhanced error information and debugging capabilities

## Steam Audio Version

- **Version:** 4.6.0
- **Core Library:** `phonon.dll` (29.4 MB) located in `lib/windows-x64/`

## Troubleshooting

The debug build provides enhanced error reporting compared to release builds:
- More detailed error messages
- Runtime assertion checks
- Better stack traces when using debuggers
- Unoptimized code for easier debugging

## Notes

- This build uses the Hot-jamn fork which includes additional features like WAV baking
- The build was successful with no errors or warnings
- All dependencies (FMOD and Steam Audio APIs) were found correctly
- Debug symbols are available for Visual Studio debugging
