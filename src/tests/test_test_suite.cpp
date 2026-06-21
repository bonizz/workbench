#include "agent/command.h"
#include "agent/test_suite.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

void runTestTestSuite()
{
    // Test suite discovery.
    {
        std::filesystem::create_directories("build/tests/discover");
        std::ofstream("build/tests/discover/beta.wbs").close();
        std::ofstream("build/tests/discover/alpha.wbs").close();
        std::ofstream("build/tests/discover/ignore.txt").close();

        auto tests = TestSuite::discoverTests("build/tests/discover");
        assert(tests.size() == 2);
        assert(tests[0].filename() == "alpha.wbs");
        assert(tests[1].filename() == "beta.wbs");

        std::filesystem::remove_all("build/tests/discover");
    }

    // Test suite pass/fail counting.
    {
        std::filesystem::create_directories("build/tests/suite");

        {
            std::ofstream pass("build/tests/suite/pass.wbs");
            pass << "scene.create_cube TestCube\n";
            pass << "assert.object_exists TestCube\n";
        }

        {
            std::ofstream fail("build/tests/suite/fail.wbs");
            fail << "assert.object_count 99\n";
        }

        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        auto passResult = TestSuite::runTestFile(scene, ctx, "build/tests/suite/pass.wbs");
        assert(passResult.passed);
        assert(passResult.name == "pass");

        auto failResult = TestSuite::runTestFile(scene, ctx, "build/tests/suite/fail.wbs");
        assert(!failResult.passed);
        assert(failResult.name == "fail");
        assert(failResult.failure.find("Expected object count: 99") != std::string::npos);

        TestSuite::Summary summary;
        summary.results.push_back(std::move(passResult));
        summary.results.push_back(std::move(failResult));
        assert(summary.passedCount() == 1);
        assert(summary.failedCount() == 1);
        assert(!summary.allPassed());

        std::filesystem::remove_all("build/tests/suite");
    }

    // Test suite failure bundle writes state.txt.
    {
        Scene scene;
        scene.createCamera({0.0f, 1.0f, 2.0f});
        GameObject* selected = nullptr;
        AgentCommandContext ctx{scene, selected};

        TestSuite::Result result = TestSuite::runTestFile(scene, ctx, "assets/scripts/assertion_fail.wbs");
        assert(!result.passed);

        std::string bundlePath;
        assert(TestSuite::createFailureBundle("test_state", ctx, bundlePath));
        assert(std::filesystem::exists(bundlePath + "state.txt"));

        std::ifstream in(bundlePath + "state.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        assert(content.find("Workbench State") != std::string::npos);

        std::filesystem::remove_all(bundlePath);
    }
}
