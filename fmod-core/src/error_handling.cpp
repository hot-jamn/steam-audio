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
#include "error_handling.h"
#include <iostream>

namespace SteamAudioFMODCore {

// --------------------------------------------------------------------------------------------------------------------
// ParameterValidator Implementation
// --------------------------------------------------------------------------------------------------------------------

/* static */ void ParameterValidator::logValidationError(const std::string& message)
{
    // Report parameter validation error through the error handler
    ErrorContext context(
        "ParameterValidator",
        __FILE__,
        __LINE__,
        message,
        ErrorCode::ParameterValidationFailed,
        ErrorSeverity::Error
    );
    
    ErrorHandler::getInstance().reportError(context);
    
    // Also output to debug console for immediate visibility
#ifdef _DEBUG
    std::cerr << "[STEAMAUDIO_FMODCORE] Parameter Validation Error: " << message << std::endl;
#endif
}

// --------------------------------------------------------------------------------------------------------------------
// ResourceTracker Implementation
// --------------------------------------------------------------------------------------------------------------------

void ResourceTracker::logResourceError(const std::string& message)
{
    // Report resource error through the error handler
    ErrorContext context(
        "ResourceTracker",
        __FILE__,
        __LINE__,
        message,
        ErrorCode::ResourceLeakDetected,
        ErrorSeverity::Warning
    );
    
    ErrorHandler::getInstance().reportError(context);
    
    // Also output to debug console for immediate visibility
#ifdef _DEBUG
    std::cerr << "[STEAMAUDIO_FMODCORE] Resource Error: " << message << std::endl;
#endif
}

} // namespace SteamAudioFMODCore