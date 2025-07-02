# Setup Visual Studio environment and run CMake
$ErrorActionPreference = "Stop"

# Setup VS environment
$vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vsPath) {
    Write-Host "Setting up Visual Studio environment..."
    cmd /c "`"$vsPath`" && cmake -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`"../bin`" .."
} else {
    Write-Error "Visual Studio Build Tools not found at $vsPath"
}
