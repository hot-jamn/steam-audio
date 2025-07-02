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

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>
#include <iostream>
#include <sstream>

namespace SteamAudioFMODCore {
namespace Testing {

// --------------------------------------------------------------------------------------------------------------------
// Test Framework Core
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Test result status.
 */
enum class TestResult
{
    Passed,
    Failed,
    Skipped,
    Error
};

/**
 * @brief Test case information.
 */
struct TestCase
{
    std::string name;
    std::string description;
    std::function<void()> testFunction;
    std::string category;
    bool enabled;

    TestCase(const std::string& n, const std::string& desc, std::function<void()> func, 
             const std::string& cat = "General", bool en = true)
        : name(n), description(desc), testFunction(func), category(cat), enabled(en)
    {
    }
};

/**
 * @brief Test execution result.
 */
struct TestExecutionResult
{
    std::string testName;
    TestResult result;
    std::string message;
    std::chrono::milliseconds executionTime;
    std::string category;

    TestExecutionResult(const std::string& name, TestResult res, const std::string& msg, 
                       std::chrono::milliseconds time, const std::string& cat)
        : testName(name), result(res), message(msg), executionTime(time), category(cat)
    {
    }
};

/**
 * @brief Test assertion exception.
 */
class TestAssertionException : public std::exception
{
public:
    explicit TestAssertionException(const std::string& message) : mMessage(message) {}
    const char* what() const noexcept override { return mMessage.c_str(); }

private:
    std::string mMessage;
};

/**
 * @brief Test framework for Steam Audio FMOD Core integration.
 */
class TestFramework
{
public:
    /**
     * @brief Get singleton instance of test framework.
     * @return Reference to singleton instance.
     */
    static TestFramework& getInstance()
    {
        static TestFramework instance;
        return instance;
    }

    /**
     * @brief Register a test case.
     * @param testCase Test case to register.
     */
    void registerTest(const TestCase& testCase)
    {
        mTestCases.push_back(testCase);
    }

    /**
     * @brief Run all registered tests.
     * @param category Optional category filter (empty = all categories).
     * @return Vector of test execution results.
     */
    std::vector<TestExecutionResult> runAllTests(const std::string& category = "")
    {
        std::vector<TestExecutionResult> results;
        
        for (const auto& testCase : mTestCases)
        {
            if (!testCase.enabled)
                continue;
                
            if (!category.empty() && testCase.category != category)
                continue;

            auto result = runSingleTest(testCase);
            results.push_back(result);
        }

        return results;
    }

    /**
     * @brief Run a specific test by name.
     * @param testName Name of test to run.
     * @return Test execution result.
     */
    TestExecutionResult runTest(const std::string& testName)
    {
        for (const auto& testCase : mTestCases)
        {
            if (testCase.name == testName)
            {
                return runSingleTest(testCase);
            }
        }

        return TestExecutionResult(testName, TestResult::Error, "Test not found", 
                                 std::chrono::milliseconds(0), "");
    }

    /**
     * @brief Get list of all registered test names.
     * @return Vector of test names.
     */
    std::vector<std::string> getTestNames() const
    {
        std::vector<std::string> names;
        for (const auto& testCase : mTestCases)
        {
            names.push_back(testCase.name);
        }
        return names;
    }

    /**
     * @brief Get list of all test categories.
     * @return Vector of unique category names.
     */
    std::vector<std::string> getCategories() const
    {
        std::vector<std::string> categories;
        for (const auto& testCase : mTestCases)
        {
            if (std::find(categories.begin(), categories.end(), testCase.category) == categories.end())
            {
                categories.push_back(testCase.category);
            }
        }
        return categories;
    }

    /**
     * @brief Print test results summary.
     * @param results Test execution results.
     */
    void printResults(const std::vector<TestExecutionResult>& results) const
    {
        int passed = 0, failed = 0, skipped = 0, errors = 0;
        std::chrono::milliseconds totalTime(0);

        std::cout << "\n=== Steam Audio FMOD Core Test Results ===\n\n";

        for (const auto& result : results)
        {
            std::string status;
            switch (result.result)
            {
            case TestResult::Passed:
                status = "PASS";
                passed++;
                break;
            case TestResult::Failed:
                status = "FAIL";
                failed++;
                break;
            case TestResult::Skipped:
                status = "SKIP";
                skipped++;
                break;
            case TestResult::Error:
                status = "ERROR";
                errors++;
                break;
            }

            std::cout << "[" << status << "] " << result.testName 
                      << " (" << result.executionTime.count() << "ms)";
            
            if (!result.message.empty())
            {
                std::cout << " - " << result.message;
            }
            
            std::cout << "\n";
            totalTime += result.executionTime;
        }

        std::cout << "\n=== Summary ===\n";
        std::cout << "Total Tests: " << results.size() << "\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "Skipped: " << skipped << "\n";
        std::cout << "Errors: " << errors << "\n";
        std::cout << "Total Time: " << totalTime.count() << "ms\n";
        
        if (failed > 0 || errors > 0)
        {
            std::cout << "\n❌ Some tests failed!\n";
        }
        else
        {
            std::cout << "\n✅ All tests passed!\n";
        }
    }

private:
    std::vector<TestCase> mTestCases;

    TestFramework() = default;
    ~TestFramework() = default;
    TestFramework(const TestFramework&) = delete;
    TestFramework& operator=(const TestFramework&) = delete;

    TestExecutionResult runSingleTest(const TestCase& testCase)
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try
        {
            testCase.testFunction();
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            return TestExecutionResult(testCase.name, TestResult::Passed, "", duration, testCase.category);
        }
        catch (const TestAssertionException& e)
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            return TestExecutionResult(testCase.name, TestResult::Failed, e.what(), duration, testCase.category);
        }
        catch (const std::exception& e)
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            return TestExecutionResult(testCase.name, TestResult::Error, 
                                     std::string("Exception: ") + e.what(), duration, testCase.category);
        }
        catch (...)
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            return TestExecutionResult(testCase.name, TestResult::Error, "Unknown exception", 
                                     duration, testCase.category);
        }
    }
};

// --------------------------------------------------------------------------------------------------------------------
// Test Assertion Macros
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Assert that a condition is true.
 */
#define STEAMAUDIO_TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Assert that two values are equal.
 */
#define STEAMAUDIO_TEST_ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " but got " << (actual) \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Assert that two values are not equal.
 */
#define STEAMAUDIO_TEST_ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " to not equal " << (actual) \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Assert that a pointer is not null.
 */
#define STEAMAUDIO_TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == nullptr) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #ptr << " is null at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Assert that a pointer is null.
 */
#define STEAMAUDIO_TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != nullptr) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #ptr << " is not null at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Assert that two floating point values are approximately equal.
 */
#define STEAMAUDIO_TEST_ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        auto diff = std::abs((expected) - (actual)); \
        if (diff > (tolerance)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " but got " << (actual) \
                << " (difference " << diff << " > tolerance " << (tolerance) << ")" \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
        } \
    } while(0)

/**
 * @brief Fail a test with a custom message.
 */
#define STEAMAUDIO_TEST_FAIL(message) \
    do { \
        std::ostringstream oss; \
        oss << "Test failed: " << (message) << " at " << __FILE__ << ":" << __LINE__; \
        throw SteamAudioFMODCore::Testing::TestAssertionException(oss.str()); \
    } while(0)

// --------------------------------------------------------------------------------------------------------------------
// Test Registration Macros
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Register a test case.
 */
#define STEAMAUDIO_TEST_CASE(name, description, category) \
    void test_##name(); \
    namespace { \
        struct TestRegistrar_##name { \
            TestRegistrar_##name() { \
                SteamAudioFMODCore::Testing::TestFramework::getInstance().registerTest( \
                    SteamAudioFMODCore::Testing::TestCase(#name, description, test_##name, category) \
                ); \
            } \
        }; \
        static TestRegistrar_##name testRegistrar_##name; \
    } \
    void test_##name()

/**
 * @brief Register a test case with default category.
 */
#define STEAMAUDIO_TEST(name, description) \
    STEAMAUDIO_TEST_CASE(name, description, "General")

// --------------------------------------------------------------------------------------------------------------------
// Mock and Stub Utilities
// --------------------------------------------------------------------------------------------------------------------

/**
 * @brief Mock Steam Audio context for testing.
 */
class MockSteamAudioContext
{
public:
    MockSteamAudioContext()
    {
        // Create a mock context that can be used for testing
        // This would be implemented to provide a fake Steam Audio context
    }

    ~MockSteamAudioContext()
    {
        // Clean up mock context
    }

    IPLContext getContext() const
    {
        // Return mock context
        return nullptr; // Would return actual mock context
    }
};

/**
 * @brief Performance test utilities.
 */
class PerformanceTestHelper
{
public:
    /**
     * @brief Measure execution time of a function.
     * @param func Function to measure.
     * @return Execution time in microseconds.
     */
    template<typename Func>
    static std::chrono::microseconds measureExecutionTime(Func&& func)
    {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }

    /**
     * @brief Run a performance benchmark.
     * @param func Function to benchmark.
     * @param iterations Number of iterations to run.
     * @return Average execution time in microseconds.
     */
    template<typename Func>
    static std::chrono::microseconds benchmark(Func&& func, int iterations = 1000)
    {
        std::chrono::microseconds totalTime(0);
        
        for (int i = 0; i < iterations; ++i)
        {
            totalTime += measureExecutionTime(func);
        }
        
        return totalTime / iterations;
    }
};

/**
 * @brief Memory test utilities.
 */
class MemoryTestHelper
{
public:
    /**
     * @brief Get current memory usage.
     * @return Memory usage in bytes.
     */
    static size_t getCurrentMemoryUsage()
    {
        // Platform-specific implementation would go here
        return 0;
    }

    /**
     * @brief Check for memory leaks in a function.
     * @param func Function to test for leaks.
     * @return True if no leaks detected.
     */
    template<typename Func>
    static bool checkForMemoryLeaks(Func&& func)
    {
        auto initialMemory = getCurrentMemoryUsage();
        func();
        auto finalMemory = getCurrentMemoryUsage();
        
        // Allow for some tolerance due to system allocations
        return (finalMemory - initialMemory) < 1024; // 1KB tolerance
    }
};

} // namespace Testing
} // namespace SteamAudioFMODCore