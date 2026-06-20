#pragma once

#include "agent/command.h"
#include "scene/scene.h"

#include <filesystem>
#include <string>
#include <vector>

namespace TestSuite {

struct Result {
    std::string name;
    bool passed = false;
    std::string failure;
    std::string bundlePath;
};

struct Summary {
    std::vector<Result> results;

    size_t passedCount() const;
    size_t failedCount() const;
    bool allPassed() const { return failedCount() == 0; }
};

// Returns all .wbs files in dir, sorted by filename.
std::vector<std::filesystem::path> discoverTests(const std::filesystem::path& dir);

// Runs a single test script against the provided scene and context.
// The caller is responsible for resetting scene state between tests.
Result runTestFile(Scene& scene, AgentCommandContext& ctx, const std::filesystem::path& path);

// Creates bundles/test_failures/<testName>/ containing state.txt.
// Does not queue a screenshot.
bool createFailureBundle(const std::string& testName, AgentCommandContext& ctx, std::string& outPath);

} // namespace TestSuite
