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

#include "reflection_baker.h"

#include "baked_reflection_data.h"
#include "energy_field_factory.h"
#include "opencl_energy_field.h"
#include "thread_pool.h"
#include "profiler.h"

#include <fstream>
#include <vector>

namespace ipl {

// Helper function to write a .wav file
void exportImpulseResponseAsWav(const std::vector<float>& impulseResponse, int sampleRate, const std::string& filePath) {
    std::ofstream outFile(filePath, std::ios::binary);

    // WAV file header
    int dataSize = impulseResponse.size() * sizeof(float);
    int fileSize = 36 + dataSize;

    outFile.write("RIFF", 4);
    outFile.write(reinterpret_cast<const char*>(&fileSize), 4);
    outFile.write("WAVE", 4);
    outFile.write("fmt ", 4);

    int fmtChunkSize = 16;
    short audioFormat = 3; // IEEE float
    short numChannels = 1; // Mono
    outFile.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    outFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
    outFile.write(reinterpret_cast<const char*>(&numChannels), 2);
    outFile.write(reinterpret_cast<const char*>(&sampleRate), 4);

    int byteRate = sampleRate * numChannels * sizeof(float);
    short blockAlign = numChannels * sizeof(float);
    short bitsPerSample = 32;
    outFile.write(reinterpret_cast<const char*>(&byteRate), 4);
    outFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
    outFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    outFile.write("data", 4);
    outFile.write(reinterpret_cast<const char*>(&dataSize), 4);

    // Write impulse response data
    outFile.write(reinterpret_cast<const char*>(impulseResponse.data()), dataSize);

    outFile.close();
}

// ---------------------------------------------------------------------------------------------------------------------
// ReflectionBaker
// ---------------------------------------------------------------------------------------------------------------------

std::atomic<bool> ReflectionBaker::sCancel(false);
std::atomic<bool> ReflectionBaker::sBakeInProgress(false);

void ReflectionBaker::bake(const IScene& scene,
                           IReflectionSimulator& simulator,
                           const BakedDataIdentifier& identifier,
                           bool bakeConvolution,
                           bool bakeParametric,
                           int numRays,
                           int numBounces,
                           float simDuration,
                           float bakeDuration,
                           int order,
                           float irradianceMinDistance,
                           int numThreads,
                           int bakeBatchSize,
                           SceneType sceneType,
                           shared_ptr<OpenCLDevice> openCL,
                           ProbeBatch& probeBatch,
                           ProgressCallback callback,
                           void* userData)
{
    PROFILE_FUNCTION();

    assert(bakeConvolution || bakeParametric);
    assert(identifier.type == BakedDataType::Reflections);
    assert(identifier.variation != BakedDataVariation::Dynamic);

    sBakeInProgress = true;

    if (sceneType != SceneType::RadeonRays && identifier.variation != BakedDataVariation::StaticListener)
    {
        bakeBatchSize = 1;
    }

    BakedReflectionsData* reflectionsData = nullptr;

    if (!probeBatch.hasData(identifier))
    {
        probeBatch.addData(identifier, make_unique<BakedReflectionsData>(identifier, probeBatch.numProbes(), bakeConvolution, bakeParametric));
    }

    reflectionsData = static_cast<BakedReflectionsData*>(&probeBatch[identifier]);

    reflectionsData->setHasConvolution(bakeConvolution);
    reflectionsData->setHasParametric(bakeParametric);

    JobGraph jobGraph;
    ThreadPool threadPool(numThreads);

    AirAbsorptionModel airAbsorption{};
    Array<CoordinateSpace3f> sources(bakeBatchSize);
    Array<CoordinateSpace3f> listeners(bakeBatchSize);
    Array<Directivity> directivities(bakeBatchSize);
    Array<unique_ptr<EnergyField>> energyFields(bakeBatchSize);
    Array<EnergyField*> energyFieldPtrs(bakeBatchSize);
    Array<int> indices(bakeBatchSize);
    auto numValidInBatch = 0;

    for (auto i = 0; i < probeBatch.numProbes(); ++i)
    {
        auto probeValid = false;

        if (identifier.variation == BakedDataVariation::Reverb)
        {
            probeValid = true;
            sources[numValidInBatch] = probeBatch[i].influence.center;
            listeners[numValidInBatch] = probeBatch[i].influence.center;
        }
        else if (identifier.variation == BakedDataVariation::StaticSource)
        {
            if (identifier.endpointInfluence.contains(probeBatch[i].influence.center))
            {
                probeValid = true;
                sources[numValidInBatch] = identifier.endpointInfluence.center;
                listeners[numValidInBatch] = probeBatch[i].influence.center;
            }
        }
        else if (identifier.variation == BakedDataVariation::StaticListener)
        {
            if (identifier.endpointInfluence.contains(probeBatch[i].influence.center))
            {
                probeValid = true;
                sources[numValidInBatch] = probeBatch[i].influence.center;
                listeners[numValidInBatch] = identifier.endpointInfluence.center;
            }
        }

        if (probeValid)
        {
            directivities[numValidInBatch] = Directivity{};
            energyFields[numValidInBatch] = EnergyFieldFactory::create(sceneType, simDuration, order, openCL);
            energyFieldPtrs[numValidInBatch] = energyFields[numValidInBatch].get();
            indices[numValidInBatch] = i;
            ++numValidInBatch;
        }

        if (numValidInBatch == bakeBatchSize || i == probeBatch.numProbes() - 1)
        {
            auto numSources = numValidInBatch;
            auto numListeners = 1;

            if (sceneType == SceneType::RadeonRays)
            {
                switch (identifier.variation)
                {
                case BakedDataVariation::StaticSource:
                    numSources = 1;
                    numListeners = numValidInBatch;
                    break;
                case BakedDataVariation::StaticListener:
                    numSources = numValidInBatch;
                    numListeners = 1;
                    break;
                case BakedDataVariation::Reverb:
                    numSources = numValidInBatch;
                    numListeners = numValidInBatch;
                    break;
                default:
                    break;
                }
            }

            jobGraph.reset();
            simulator.simulate(scene, numSources, sources.data(), numListeners, listeners.data(),
                               directivities.data(), numRays, numBounces, simDuration, order,
                               irradianceMinDistance, energyFieldPtrs.data(), jobGraph);

            threadPool.process(jobGraph);

#if defined(IPL_USES_OPENCL)
            if (sceneType == SceneType::RadeonRays)
            {
                for (auto j = 0; j < numValidInBatch; ++j)
                {
                    static_cast<OpenCLEnergyField&>(*energyFields[j]).copyDeviceToHost();
                }
            }
#endif

            if (bakeParametric)
            {
                for (auto j = 0; j < numValidInBatch; ++j)
                {
                    Reverb reverb;
                    ReverbEstimator::estimate(*energyFields[j], airAbsorption, reverb);

                    static_cast<BakedReflectionsData&>(probeBatch[identifier]).set(indices[j], reverb);
                }
            }

            if (bakeConvolution)
            {
                for (auto j = 0; j < numValidInBatch; ++j)
                {
                    unique_ptr<EnergyField> energyField;

                    if (simDuration == bakeDuration)
                    {
                        energyField = std::move(energyFields[j]);
                    }
                    else
                    {
                        energyField = make_unique<EnergyField>(bakeDuration, order);
                        energyField->copyFrom(*energyFields[j]);
                    }

                    static_cast<BakedReflectionsData&>(probeBatch[identifier]).set(indices[j], std::move(energyField));

                    std::vector<float> impulseResponse = energyFields[j]->getImpulseResponse();
                    int sampleRate = 44100;
                    std::string filePath = "output/impulse_response_" + std::to_string(indices[j]) + ".wav";
                    exportImpulseResponseAsWav(impulseResponse, sampleRate, filePath);
                }
            }

            numValidInBatch = 0;

            if (callback)
            {
                callback((i + 1.0f) / probeBatch.numProbes(), userData);
            }

            if (sCancel)
            {
                sCancel = false;
                break;
            }
        }
    }

    sBakeInProgress = false;
}

void ReflectionBaker::cancel()
{
    if (sBakeInProgress)
    {
        sCancel = true;
    }
}

}
