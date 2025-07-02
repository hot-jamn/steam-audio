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
#include "library.h"

namespace SteamAudioFMODCore {

namespace Library
{

void getLoadingBinaryPath(char* loadingBinaryPath, int maxPathLength)
{
#if defined(IPL_OS_WINDOWS)
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&getLoadingBinaryPath), &module);
    wchar_t widePathBuffer[MAX_PATH];
    GetModuleFileNameW(module, widePathBuffer, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, widePathBuffer, -1, loadingBinaryPath, maxPathLength, nullptr, nullptr);
#elif defined(IPL_OS_MACOSX)
    Dl_info info;
    dladdr(reinterpret_cast<void*>(&getLoadingBinaryPath), &info);
    strncpy(loadingBinaryPath, info.dli_fname, maxPathLength);
#else
    Dl_info info;
    dladdr(reinterpret_cast<void*>(&getLoadingBinaryPath), &info);
    strncpy(loadingBinaryPath, info.dli_fname, maxPathLength);
#endif
}

void getLoadedBinaryPath(const char* name, char* loadedBinaryPath, int maxPathLength)
{
#if defined(IPL_OS_WINDOWS)
    // Convert name to wide string
    wchar_t wideName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, MAX_PATH);
    
    HMODULE module = GetModuleHandleW(wideName);
    if (module)
    {
        wchar_t widePathBuffer[MAX_PATH];
        GetModuleFileNameW(module, widePathBuffer, MAX_PATH);
        WideCharToMultiByte(CP_UTF8, 0, widePathBuffer, -1, loadedBinaryPath, maxPathLength, nullptr, nullptr);
    }
    else
    {
        loadedBinaryPath[0] = '\0';
    }
#elif defined(IPL_OS_MACOSX)
    // On macOS, we need to iterate through loaded images
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        const char* imageName = _dyld_get_image_name(i);
        if (imageName && strstr(imageName, name))
        {
            strncpy(loadedBinaryPath, imageName, maxPathLength);
            return;
        }
    }
    loadedBinaryPath[0] = '\0';
#else
    // On Linux, we can use dladdr with a known symbol
    void* handle = dlopen(name, RTLD_NOLOAD);
    if (handle)
    {
        Dl_info info;
        if (dladdr(handle, &info))
        {
            strncpy(loadedBinaryPath, info.dli_fname, maxPathLength);
        }
        else
        {
            loadedBinaryPath[0] = '\0';
        }
        dlclose(handle);
    }
    else
    {
        loadedBinaryPath[0] = '\0';
    }
#endif
}

#if defined(IPL_OS_WINDOWS)

HMODULE load(const char* name)
{
    // Convert name to wide string
    wchar_t wideName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, MAX_PATH);
    return LoadLibraryW(wideName);
}

void unload(HMODULE library)
{
    if (library)
    {
        FreeLibrary(library);
    }
}

void* getFunction(HMODULE library, const char* name)
{
    if (!library)
        return nullptr;
    
    return reinterpret_cast<void*>(GetProcAddress(library, name));
}

#else

void* load(const char* name)
{
    return dlopen(name, RTLD_LAZY);
}

void unload(void* library)
{
    if (library)
    {
        dlclose(library);
    }
}

void* getFunction(void* library, const char* name)
{
    if (!library)
        return nullptr;
    
    return dlsym(library, name);
}

#endif

}

// --------------------------------------------------------------------------------------------------------------------
// API Implementation
// --------------------------------------------------------------------------------------------------------------------

API::API()
    : library(nullptr)
{
    // Try to load the Steam Audio library
    const char* libraryNames[] = {
#if defined(IPL_OS_WINDOWS)
        "phonon.dll",
        "libphonon.dll"
#elif defined(IPL_OS_MACOSX)
        "libphonon.dylib",
        "phonon.dylib"
#else
        "libphonon.so",
        "phonon.so"
#endif
    };

    for (const auto& libraryName : libraryNames)
    {
        library = Library::load(libraryName);
        if (library)
            break;
    }

    if (library)
    {
        // Dynamically link all required Steam Audio functions
        DYNAMIC_LINK_LIBRARY_FUNCTION(iplContextCreate);
        
        // Add more function linkings as needed
        // DYNAMIC_LINK_LIBRARY_FUNCTION(iplContextRelease);
        // DYNAMIC_LINK_LIBRARY_FUNCTION(iplHRTFCreate);
        // etc.
    }
}

API::~API()
{
    if (library)
    {
        Library::unload(library);
        library = nullptr;
    }
}

const API& gAPI()
{
    static API api;
    return api;
}

}