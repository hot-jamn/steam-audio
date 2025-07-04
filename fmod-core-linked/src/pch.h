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

#include <assert.h>
#include <math.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

#if defined(IPL_OS_WINDOWS)
#include <Windows.h>
#elif defined(IPL_OS_MACOSX)
#endif

#include <fmod.hpp>
#include <fmod_common.h>

#include <phonon.h>

#include "steamaudio_fmodcore_version.h"
#include "steamaudio_fmodcore.h"
