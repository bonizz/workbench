#include "agent/test_suite.h"

#include "agent/script_runner.h"
#include "debug/bundle.h"
#include "debug/debug_state.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace TestSuite {

size_t Summary::passedCount() const
{
    size_t count = 0;
    for (const auto& r : results) {
        if (r.passed) ++count;
    }
    return count;
}

size_t Summary::failedCount() const
{
    size_t count = 0;
    for (const auto& r : results) {
        if (!r.passed) ++count;
    }
    return count;
}

std::vector<std::filesystem::path> discoverTests(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> tests;

    if (!std::filesystem::exists(dir)) {
        return tests;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wbs") {
            tests.push_back(entry.path());
        }
    }

    std::sort(tests.begin(), tests.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  return a.filename() < b.filename();
              });

    return tests;
}

Result runTestFile(Scene& scene, AgentCommandContext& ctx, const std::filesystem::path& path)
{
    Result result;
    result.name = path.stem().string();

    ScriptResult scriptResult = runScript(ctx, path, path.filename().string());

    result.passed = scriptResult.success;
    if (!result.passed) {
        result.failure = scriptResult.error;
    }

    return result;
}

bool createFailureBundle(const std::string& testName, AgentCommandContext& ctx, std::string& outPath)
{
    if (!Bundle::isValidBundleName(testName)) {
        std::fprintf(stderr, "Invalid test failure bundle name: %s\n", testName.c_str());
        return false;
    }

    outPath = "bundles/test_failures/" + testName + "/";
    std::filesystem::create_directories(outPath);

    std::string stateText = DebugState::build(
        ctx.frame, ctx.fps, ctx.frameTimeMs, ctx.renderCommandCount,
        ctx.scene, ctx.selected, ctx.lastScriptPath, ctx.lastCapturePath,
        ctx.lastBundlePath, ctx.lastAssertionFailure);

    DebugState::writeToFile(stateText, outPath + "state.txt");
    return true;
}

} // namespace TestSuite
