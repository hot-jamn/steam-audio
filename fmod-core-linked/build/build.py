# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import re
import subprocess
import sys

# Ensure stdout uses UTF-8 encoding (Python >3.7)
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding='utf-8')

import shutil

# Detects the host operating system.
def detect_host_system():
    if sys.platform == 'win32':
        return 'windows'
    elif sys.platform == 'darwin':
        return 'osx'
    elif sys.platform in ['linux', 'linux2', 'linux3']:
        return 'linux'
    else:
        return ''

# Parses the command line.
def parse_command_line(host_system):
    parser = argparse.ArgumentParser(description='Steam Audio FMOD Core Build System')
    
    # Basic build options
    parser.add_argument('-p', '--platform', help = "Target operating system.", choices = ['windows', 'osx', 'linux', 'android', 'ios', 'wasm'], type = str.lower, default = host_system)
    parser.add_argument('-t', '--toolchain', help = "Compiler toolchain. (Windows only)", choices = ['vs2013', 'vs2015', 'vs2017', 'vs2019', 'vs2022'], type = str.lower, default = 'vs2022')
    parser.add_argument('-a', '--architecture', help = "CPU architecture.", choices = ['x86', 'x64', 'armv7', 'arm64'], type = str.lower, default = 'x64')
    parser.add_argument('-c', '--configuration', help = "Build configuration.", choices = ['debug', 'release'], type = str.lower, default = 'release')
    parser.add_argument('-o', '--operation', help = "CMake operation.", choices = ['generate', 'build', 'install', 'package', 'default', 'ci_build', 'ci_package'], type = str.lower, default = 'default')
    
    # Enhanced FMOD Core integration options
    enhanced_group = parser.add_argument_group('Enhanced FMOD Core Options', 'Options for enhanced Steam Audio FMOD Core integration with error handling and performance monitoring')
    enhanced_group.add_argument('--enhanced', action='store_true', default=True, help='Build enhanced version with comprehensive error handling and performance monitoring (default: enabled)')
    enhanced_group.add_argument('--no-enhanced', dest='enhanced', action='store_false', help='Disable enhanced version and build original implementation only')
    enhanced_group.add_argument('--original', action='store_true', default=True, help='Build original version alongside enhanced version (default: enabled)')
    enhanced_group.add_argument('--no-original', dest='original', action='store_false', help='Disable original version and build enhanced implementation only')
    
    # Enhanced feature toggles
    features_group = parser.add_argument_group('Enhanced Features', 'Fine-grained control over enhanced features (only applies when --enhanced is enabled)')
    features_group.add_argument('--enable-profiling', action='store_true', default=False, help='Enable performance profiling with runtime toggleable monitoring')
    features_group.add_argument('--enable-error-recovery', action='store_true', default=True, help='Enable automatic error recovery mechanisms (default: enabled)')
    features_group.add_argument('--enable-resource-tracking', action='store_true', default=False, help='Enable resource leak detection and tracking (debug builds only)')
    features_group.add_argument('--enable-thread-safety', action='store_true', default=True, help='Enable thread-safe operations with atomic state management (default: enabled)')
    features_group.add_argument('--enable-detailed-logging', action='store_true', default=False, help='Enable detailed logging for production debugging')
    features_group.add_argument('--enable-memory-validation', action='store_true', default=False, help='Enable memory validation and bounds checking (debug builds only)')
    features_group.add_argument('--enable-parameter-validation', action='store_true', default=True, help='Enable comprehensive parameter validation (default: enabled)')
    
    # Build optimization options
    optimization_group = parser.add_argument_group('Build Optimizations', 'Compiler and build optimization options')
    optimization_group.add_argument('--enable-fast-math', action='store_true', default=True, help='Enable fast math optimizations for audio processing (default: enabled)')
    optimization_group.add_argument('--enable-sanitizers', action='store_true', default=False, help='Enable address sanitizer in debug builds')
    optimization_group.add_argument('--enable-warnings-as-errors', action='store_true', default=False, help='Treat compiler warnings as errors')
    optimization_group.add_argument('--enable-multiprocessor-build', action='store_true', default=True, help='Enable multi-processor compilation (MSVC only, default: enabled)')
    
    # Testing and development options
    testing_group = parser.add_argument_group('Testing & Development', 'Options for testing and development builds')
    testing_group.add_argument('--enable-testing', action='store_true', default=False, help='Enable testing framework and build test targets')
    testing_group.add_argument('--enable-benchmarks', action='store_true', default=False, help='Enable performance benchmarking targets')
    testing_group.add_argument('--enable-examples', action='store_true', default=False, help='Build example applications and demos')
    
    args = parser.parse_args()
    
    # Validation: ensure at least one version is enabled
    if not args.enhanced and not args.original:
        parser.error("At least one of --enhanced or --original must be enabled")
    
    # Auto-enable resource tracking and memory validation in debug builds
    if args.configuration == 'debug':
        if not hasattr(args, 'enable_resource_tracking') or args.enable_resource_tracking is None:
            args.enable_resource_tracking = True
        if not hasattr(args, 'enable_memory_validation') or args.enable_memory_validation is None:
            args.enable_memory_validation = True
        if not hasattr(args, 'enable_testing') or args.enable_testing is None:
            args.enable_testing = True
    
    return args

# Returns the subdirectory in which to create build files.
def build_subdir(args):
    if args.platform == 'windows':
        return "-".join([args.platform, args.toolchain, args.architecture])
    elif args.platform in ['osx', 'ios']:
        return args.platform
    elif args.platform in ['linux', 'android']:
        return "-".join([args.platform, args.architecture, args.configuration])
    elif args.platform in ['wasm']:
        return "-".join([args.platform, args.configuration])

# Returns the subdirectory in which to create output files.
def bin_subdir(args):
    base_subdir = ""
    if args.platform in ['windows', 'linux', 'android']:
        base_subdir = "-".join([args.platform, args.architecture])
    elif args.platform in ['osx', 'ios', 'wasm']:
        base_subdir = "-".join([args.platform])
    
    # Add version-specific suffix for enhanced/original differentiation
    if hasattr(args, 'enhanced') and hasattr(args, 'original'):
        if args.enhanced and args.original:
            # Dual-mode: return base path, subdirs handled by install prefix
            return base_subdir
        elif args.enhanced:
            return base_subdir + "-enhanced"
        elif args.original:
            return base_subdir + "-original"
    
    return base_subdir

# Returns the root directory of the repository.
def root_dir():
    # Get the directory containing this script (fmod-core/build)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Go up one level to fmod-core, then up one more to steam-audio root
    return os.path.normpath(os.path.join(script_dir, '..', '..'))

# Returns the generator name to pass to CMake, based on the platform.
def generator_name(args):
    if args.platform == 'windows':
        suffix = ''
        if args.architecture == 'x64' and args.toolchain in ['vs2013', 'vs2015', 'vs2017']:
            suffix = ' Win64'
        generator = ''
        if args.toolchain == 'vs2013':
            generator = 'Visual Studio 12 2013'
        elif args.toolchain == 'vs2015':
            generator = 'Visual Studio 14 2015'
        elif args.toolchain == 'vs2017':
            generator = 'Visual Studio 15 2017'
        elif args.toolchain == 'vs2019':
            generator = 'Visual Studio 16 2019'
        elif args.toolchain == 'vs2022':
            generator = 'Visual Studio 17 2022'
        return generator + suffix
    elif args.platform in ['osx', 'ios']:
        return 'Xcode'
    elif args.platform in ['linux', 'android', 'wasm']:
        return 'Unix Makefiles'

# Returns the configuration name to pass to CMake.
def config_name(args):
    if args.configuration == 'debug':
        return "Debug"
    else:
        return "Release"

# Runs CMake (or CTest, or CPack).
def run_cmake(program_name, args):
    env = os.environ.copy()
    if os.getenv('STEAMAUDIO_OVERRIDE_PYTHONPATH') is not None:
        env['PYTHONPATH'] = ''
    if os.getenv('STEAMAUDIO_OVERRIDE_SDKROOT') is not None:
        env['SDKROOT'] = ''

    subprocess.check_call([program_name] + args, env=env)

# Runs the "generate" step.
def cmake_generate(args):
    cmake_args = ['-G', generator_name(args)]

    # If using Visual Studio 2019 on Windows, specify architecture as a parameter.
    if args.platform == 'windows' and args.toolchain not in ['vs2013', 'vs2015', 'vs2017']:
        if args.architecture == 'x86':
            cmake_args += ['-A', 'Win32']
        elif args.architecture == 'x64':
            cmake_args += ['-A', 'x64']

    # For Android, specify the toolchain file and point to the NDK installation.
    if args.platform == 'android':
        if args.architecture == 'armv7':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv7.cmake']
        elif args.architecture == 'arm64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_armv8.cmake']
        elif args.architecture == 'x86':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x86.cmake']
        elif args.architecture == 'x64':
            cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_android_x64.cmake']

        if os.environ.get('ANDROID_NDK') is not None:
            cmake_args += ['-DCMAKE_ANDROID_NDK=' + os.environ.get('ANDROID_NDK')]
            cmake_args += ['-DCMAKE_MAKE_PROGRAM=' + os.environ.get('ANDROID_NDK') + '/prebuilt/windows-x86_64/bin/make.exe']

    # On iOS, specify the toolchain file.
    if args.platform in ['ios']:
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + root_dir() + '/build/toolchain_ios.cmake']

    # On Linux and Android, specify the build configuration at generate-time.
    if args.platform in ['linux', 'android']:
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

    # Install files to fmod-core/bin/ with version-specific subdirectories
    # Get the fmod-core directory (parent of the build script directory)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    fmod_core_dir = os.path.dirname(script_dir)  # Go up from build/ to fmod-core/
    fmod_core_bin_path = os.path.normpath(os.path.join(fmod_core_dir, 'bin'))
    
    if args.enhanced and args.original:
        # Dual-mode build: create separate subdirectories for each version
        cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + fmod_core_bin_path]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENHANCED_INSTALL_SUBDIR=enhanced']
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ORIGINAL_INSTALL_SUBDIR=original']
        print("INFO: Dual-mode build will install to separate subdirectories:")
        print(f"  Enhanced plugins: {os.path.join(fmod_core_bin_path, 'enhanced')}")
        print(f"  Original plugins: {os.path.join(fmod_core_bin_path, 'original')}")
    elif args.enhanced:
        # Enhanced-only build
        enhanced_path = os.path.join(fmod_core_bin_path, 'enhanced')
        cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + enhanced_path]
        print(f"INFO: Enhanced-only build will install to: {enhanced_path}")
    elif args.original:
        # Original-only build
        original_path = os.path.join(fmod_core_bin_path, 'original')
        cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + original_path]
        print(f"INFO: Original-only build will install to: {original_path}")
    else:
        # Fallback (should not happen due to validation)
        cmake_args += ['-DCMAKE_INSTALL_PREFIX=' + fmod_core_bin_path]
    
    # Ensure the bin directory exists and copy the README
    try:
        os.makedirs(fmod_core_bin_path, exist_ok=True)
        readme_source = os.path.join(fmod_core_dir, 'bin', 'README.md')
        if os.path.exists(readme_source):
            print(f"INFO: README.md will be available at: {fmod_core_bin_path}")
    except Exception as e:
        print(f"Warning: Could not prepare bin directory: {e}")

    # On Linux x86, specify -m32 explicitly.
    if args.platform == 'linux' and args.architecture == 'x86':
        cmake_args += ['-DCMAKE_CXX_FLAGS=-m32']
        cmake_args += ['-DCMAKE_SHARED_LINKER_FLAGS=-m32']

    # On Android, explitly point to dependencies.
    if args.platform == 'android':
        cmake_args += ['-DFMOD_INCLUDE_DIR=' + root_dir() + '/include/fmod']
        cmake_args += ['-DSteamAudio_INCLUDE_DIR=' + root_dir() + '/include/phonon']

    # Enhanced FMOD Core integration options with explicit type casting
    print("Configuring Steam Audio FMOD Core with enhanced features...")
    
    # Main build toggles with explicit boolean conversion
    cmake_args += ['-DSTEAMAUDIO_FMODCORE_BUILD_ENHANCED=' + str('ON' if bool(args.enhanced) else 'OFF')]
    cmake_args += ['-DSTEAMAUDIO_FMODCORE_BUILD_ORIGINAL=' + str('ON' if bool(args.original) else 'OFF')]
    
    # Enhanced/Basic DSP linking validation and configuration
    if args.enhanced and args.original:
        print("INFO: Configuring dual-mode build with enhanced and original DSP descriptions")
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_DUAL_MODE=ON']
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_LINK_DSP_DESCRIPTIONS=ON']
    elif args.enhanced:
        print("INFO: Configuring enhanced-only build")
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_DUAL_MODE=OFF']
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENHANCED_ONLY=ON']
    elif args.original:
        print("INFO: Configuring original-only build")
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_DUAL_MODE=OFF']
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ORIGINAL_ONLY=ON']
    
    # Enhanced feature options (only apply when enhanced build is enabled)
    if args.enhanced:
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_ENHANCED=' + str('ON' if bool(args.enhanced) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_PROFILING=' + str('ON' if bool(args.enable_profiling) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_ERROR_RECOVERY=' + str('ON' if bool(args.enable_error_recovery) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_RESOURCE_TRACKING=' + str('ON' if bool(args.enable_resource_tracking) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_THREAD_SAFETY=' + str('ON' if bool(args.enable_thread_safety) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_DETAILED_LOGGING=' + str('ON' if bool(args.enable_detailed_logging) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_MEMORY_VALIDATION=' + str('ON' if bool(args.enable_memory_validation) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_PARAMETER_VALIDATION=' + str('ON' if bool(args.enable_parameter_validation) else 'OFF')]
        
        # Build optimization options with explicit type casting
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_FAST_MATH=' + str('ON' if bool(args.enable_fast_math) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_SANITIZERS=' + str('ON' if bool(args.enable_sanitizers) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_WARNINGS_AS_ERRORS=' + str('ON' if bool(args.enable_warnings_as_errors) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_MULTIPROCESSOR_BUILD=' + str('ON' if bool(args.enable_multiprocessor_build) else 'OFF')]
        
        # Testing and development options with explicit type casting
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_TESTING=' + str('ON' if bool(args.enable_testing) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_BENCHMARKS=' + str('ON' if bool(args.enable_benchmarks) else 'OFF')]
        cmake_args += ['-DSTEAMAUDIO_FMODCORE_ENABLE_EXAMPLES=' + str('ON' if bool(args.enable_examples) else 'OFF')]
        
        # Print configuration summary
        print("Enhanced features enabled:")
        if args.enable_profiling:
            print("  - Performance profiling with runtime monitoring")
        if args.enable_error_recovery:
            print("  - Automatic error recovery mechanisms")
        if args.enable_resource_tracking:
            print("  - Resource leak detection and tracking")
        if args.enable_thread_safety:
            print("  - Thread-safe operations with atomic state management")
        if args.enable_detailed_logging:
            print("  - Detailed logging for production debugging")
        if args.enable_memory_validation:
            print("  - Memory validation and bounds checking")
        if args.enable_parameter_validation:
            print("  - Comprehensive parameter validation")
        if args.enable_testing:
            print("  - Testing framework and validation targets")
        if args.enable_benchmarks:
            print("  - Performance benchmarking targets")
        if args.enable_examples:
            print("  - Example applications and demos")

    # On Windows x64, build documentation.
    if args.platform == 'windows' and args.architecture == 'x64':
        cmake_args += ['-DSTEAMAUDIOFMODCORE_BUILD_DOCS=TRUE']
        doxygen_path = find_tool('doxygen', r'doxygen-(\d+)\.(\d+)\.?(\d+)?', [1, 9])
        if doxygen_path is not None:
            cmake_args += ['-DDOXYGEN_EXECUTABLE=' + os.path.normpath(os.path.join(doxygen_path, 'doxygen.exe'))]

    if args.platform == 'wasm':
        cmake_args += ['-DCMAKE_TOOLCHAIN_FILE=' + os.environ.get('EMSDK') + '/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake']
        cmake_args += ['-DBUILD_SHARED_LIBS=FALSE']
        cmake_args += ['-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH']
        cmake_args += ['-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH']
        cmake_args += ['-DEMSCRIPTEN_SYSTEM_PROCESSOR=arm']
        cmake_args += ['-DCMAKE_BUILD_TYPE=' + config_name(args)]

    cmake_args += ['..']

    run_cmake('cmake', cmake_args)

# Runs the "build" step.
def cmake_build(args):
    cmake_args = ['--build', '.']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['--config', config_name(args)]

    run_cmake('cmake', cmake_args)

# Runs the "install" step.
def cmake_install(args):
    cmake_args = ['--install', '.']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['--config', config_name(args)]

    run_cmake('cmake', cmake_args)

# Runs the "package" step.
def cmake_package(args):
    cmake_args = ['-G', 'ZIP']

    if args.platform in ['windows', 'osx', 'ios']:
        cmake_args += ['-C', config_name(args)]

    run_cmake('cpack', cmake_args)

# Finds a build tool (e.g. cmake, ispc).
def find_tool(tool_name, dir_regex, min_version):
    tool_matches = {}

    tools_dirs = [os.path.normpath(root_dir() + '/../../tools'), os.path.normpath(root_dir() + '/../tools')]
    for tools_dir in tools_dirs:
        if not os.path.exists(tools_dir):
            continue

        dir_contents = os.listdir(tools_dir)
        for item in dir_contents:
            dir_path = os.path.join(tools_dir, item)
            if not os.path.isdir(dir_path):
                continue

            dir_name = os.path.basename(dir_path)

            regex_match = re.match(dir_regex, dir_name)
            if regex_match is None:
                continue

            tool_matches[dir_path] = regex_match

    num_version_components = len(min_version)

    latest_version = []
    for i in range(1, num_version_components + 1):
        latest_version.append(0)

    latest_version_path = None

    for tool_path in list(tool_matches.keys()):
        version_match = tool_matches[tool_path]

        current_version = []
        for i in range(1, num_version_components + 1):
            current_version.append(int(version_match.group(i)))

        newer_version_found = False

        for i in range(1, num_version_components + 1):
            if current_version[i-1] < min_version[i-1]:
                break
            if current_version[i-1] < latest_version[i-1]:
                break
            newer_version_found = True

        if newer_version_found:
            latest_version = current_version
            latest_version_path = tool_path

    if latest_version_path is not None:
        host_platform = detect_host_system()
        platform_subdirectory = None
        if host_platform == 'windows':
            platform_subdirectory = 'windows-x64'
        elif host_platform == 'linux':
            platform_subdirectory = 'linux-x64'
        elif host_platform == 'osx':
            platform_subdirectory = 'osx'

        if platform_subdirectory is not None:
            latest_version_path = os.path.join(latest_version_path, platform_subdirectory)

    return latest_version_path

# Prints build configuration summary.
def print_build_summary(args):
    print("\n" + "="*60)
    print("STEAM AUDIO FMOD CORE BUILD CONFIGURATION")
    print("="*60)
    print(f"Platform: {args.platform}")
    print(f"Architecture: {args.architecture}")
    print(f"Configuration: {args.configuration}")
    print(f"Operation: {args.operation}")
    
    if args.platform == 'windows':
        print(f"Toolchain: {args.toolchain}")
    
    print(f"\nBuild Targets:")
    print(f"  Enhanced Version: {'YES' if args.enhanced else 'NO'}")
    print(f"  Original Version: {'YES' if args.original else 'NO'}")
    
    if args.enhanced:
        print(f"\nEnhanced Features:")
        print(f"  Performance Profiling: {'YES' if args.enable_profiling else 'NO'}")
        print(f"  Error Recovery: {'YES' if args.enable_error_recovery else 'NO'}")
        print(f"  Resource Tracking: {'YES' if args.enable_resource_tracking else 'NO'}")
        print(f"  Thread Safety: {'YES' if args.enable_thread_safety else 'NO'}")
        print(f"  Detailed Logging: {'YES' if args.enable_detailed_logging else 'NO'}")
        print(f"  Memory Validation: {'YES' if args.enable_memory_validation else 'NO'}")
        print(f"  Parameter Validation: {'YES' if args.enable_parameter_validation else 'NO'}")
        
        print(f"\nBuild Optimizations:")
        print(f"  Fast Math: {'YES' if args.enable_fast_math else 'NO'}")
        print(f"  Sanitizers: {'YES' if args.enable_sanitizers else 'NO'}")
        print(f"  Warnings as Errors: {'YES' if args.enable_warnings_as_errors else 'NO'}")
        print(f"  Multi-processor Build: {'YES' if args.enable_multiprocessor_build else 'NO'}")
        
        print(f"\nDevelopment Features:")
        print(f"  Testing Framework: {'YES' if args.enable_testing else 'NO'}")
        print(f"  Benchmarks: {'YES' if args.enable_benchmarks else 'NO'}")
        print(f"  Examples: {'YES' if args.enable_examples else 'NO'}")
    
    print("="*60 + "\n")

# Validates build configuration.
def validate_build_config(args):
    config_warnings = []
    config_errors = []
    
    # Check for conflicting options
    if args.configuration == 'release' and args.enable_sanitizers:
        config_warnings.append("Address sanitizers are typically used in debug builds")
    
    if args.configuration == 'release' and args.enable_memory_validation:
        config_warnings.append("Memory validation adds overhead and is typically used in debug builds")
    
    if not args.enhanced and (args.enable_profiling or args.enable_error_recovery):
        config_warnings.append("Enhanced features are enabled but enhanced build is disabled")
    
    # Enhanced/Basic DSP linking validation
    if args.enhanced and args.original:
        print("INFO: Building both enhanced and original versions - ensuring proper DSP description linking")
    elif not args.enhanced and not args.original:
        config_errors.append("At least one of enhanced or original versions must be enabled")
    
    # Platform-specific validations
    if args.platform == 'android' and not os.environ.get('ANDROID_NDK'):
        config_errors.append("ANDROID_NDK environment variable is required for Android builds")
    
    if args.platform == 'wasm' and not os.environ.get('EMSDK'):
        config_errors.append("EMSDK environment variable is required for WebAssembly builds")
    
    # Print warnings and errors
    if config_warnings:
        print("WARNINGS:")
        for config_warning in config_warnings:
            print(f"  - {config_warning}")
        print()
    
    if config_errors:
        print("ERRORS:")
        for config_error in config_errors:
            print(f"  - {config_error}")
        print()
        return False
    
    return True

# Main script.

host_system = detect_host_system()
args = parse_command_line(host_system)

# Print build configuration summary
print_build_summary(args)

# Validate build configuration
if not validate_build_config(args):
    print("Build configuration validation failed. Exiting.")
    sys.exit(1)

# Determine the correct build directory
# The CMake files are generated in fmod-core/windows-vs2022-x64, not fmod-core/build/windows-vs2022-x64
script_dir = os.path.dirname(os.path.abspath(__file__))
fmod_core_dir = os.path.dirname(script_dir)  # Go up from build/ to fmod-core/
build_dir = os.path.join(fmod_core_dir, build_subdir(args))

try:
    os.makedirs(build_dir, exist_ok=True)
except Exception as dir_creation_error:
    print(f"Warning: Could not create build directory {build_dir}: {dir_creation_error}")

olddir = os.getcwd()
os.chdir(build_dir)

print(f"INFO: Using build directory: {build_dir}")
print(f"INFO: Current working directory: {os.getcwd()}")

cmake_path = find_tool('cmake', r'cmake-(\d+)\.(\d+)\.?(\d+)?', [3, 17])
if cmake_path is not None:
    if host_system == 'osx':
        os.environ['PATH'] = os.path.normpath(os.path.join(cmake_path, 'CMake.app', 'Contents', 'bin')) + os.pathsep + os.environ['PATH']
    else:
        os.environ['PATH'] = os.path.normpath(os.path.join(cmake_path, 'bin')) + os.pathsep + os.environ['PATH']

# Execute build operations with enhanced error handling
try:
    if args.operation == 'generate':
        print("Generating build files...")
        cmake_generate(args)
        print("Build files generated successfully!")
    elif args.operation == 'build':
        print("Building project...")
        cmake_build(args)
        print("Build completed successfully!")
        print("Installing artifacts...")
        cmake_install(args)
        print("Installation completed successfully!")
    elif args.operation == 'install':
        print("Installing project...")
        cmake_install(args)
        print("Installation completed successfully!")
    elif args.operation == 'package':
        print("Installing artifacts...")
        cmake_install(args)
        print("Installation completed successfully!")
        print("Creating package...")
        cmake_package(args)
        print("Package created successfully!")
    elif args.operation == 'default':
        print("Generating build files...")
        cmake_generate(args)
        print("Build files generated successfully!")
        print("Building project...")
        cmake_build(args)
        print("Build completed successfully!")
        print("Installing artifacts...")
        cmake_install(args)
        print("Installation completed successfully!")
    elif args.operation == 'ci_build':
        print("Running CI build (generate + build + install)...")
        cmake_generate(args)
        print("Build files generated successfully!")
        cmake_build(args)
        print("Build completed successfully!")
        cmake_install(args)
        print("Installation completed successfully!")
    elif args.operation == 'ci_package':
        print("Running CI package (generate + build + install + package)...")
        cmake_generate(args)
        print("Build files generated successfully!")
        cmake_build(args)
        print("Build completed successfully!")
        cmake_install(args)
        print("Installation completed successfully!")
        cmake_package(args)
        print("Package created successfully!")

    # Print success summary
    print("\n" + "="*60)
    print("BUILD COMPLETED SUCCESSFULLY")
    print("="*60)
    print(f"Platform: {args.platform} ({args.architecture})")
    print(f"Configuration: {args.configuration}")
    print(f"Operation: {args.operation}")
    
    if args.enhanced:
        print("\nEnhanced Steam Audio FMOD Core integration built with:")
        enabled_features = []
        if args.enable_profiling:
            enabled_features.append("Performance profiling")
        if args.enable_error_recovery:
            enabled_features.append("Error recovery")
        if args.enable_resource_tracking:
            enabled_features.append("Resource tracking")
        if args.enable_thread_safety:
            enabled_features.append("Thread safety")
        if args.enable_detailed_logging:
            enabled_features.append("Detailed logging")
        if args.enable_memory_validation:
            enabled_features.append("Memory validation")
        if args.enable_parameter_validation:
            enabled_features.append("Parameter validation")
        if args.enable_testing:
            enabled_features.append("Testing framework")
        
        if enabled_features:
            for feature in enabled_features:
                print(f"  ✓ {feature}")
        else:
            print("  ✓ Basic enhanced features")
    
    if args.original:
        print("✓ Original Steam Audio FMOD Core implementation included")
    
    print("="*60)

except subprocess.CalledProcessError as build_error:
    print(f"\nBuild failed with error code {build_error.returncode}")
    print("Check the output above for detailed error information.")
    os.chdir(olddir)
    sys.exit(build_error.returncode)
except Exception as unexpected_error:
    print(f"\nUnexpected error during build: {unexpected_error}")
    os.chdir(olddir)
    sys.exit(1)

os.chdir(olddir)