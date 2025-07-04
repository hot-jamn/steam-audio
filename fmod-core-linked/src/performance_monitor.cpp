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
#include "performance_monitor.h"

#include <iostream>

namespace SteamAudioFMODCore {

PerformanceMonitor::PerformanceMonitor()
{
}

PerformanceMonitor::~PerformanceMonitor()
{
}

void PerformanceMonitor::start(const char* name)
{
    mStartTimes[name] = std::chrono::high_resolution_clock::now();
}

void PerformanceMonitor::end(const char* name)
{
    auto endTime = std::chrono::high_resolution_clock::now();
    auto startTime = mStartTimes[name];
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    std::cout << name << ": " << duration << " us" << std::endl;
}

}