#include "debug/bundle.h"

#include "agent/command.h"
#include "debug/debug_state.h"
#include "renderer/metal_renderer.h"

#include <filesystem>

namespace Bundle {

std::string makeBundlePath(const std::string& name)
{
    return "bundles/" + name + "/";
}

bool isValidBundleName(const std::string& name)
{
    if (name.empty()) {
        return false;
    }

    for (char c : name) {
        if (c == '/' || c == '\\') {
            return false;
        }
    }

    std::filesystem::path p(name);
    for (const auto& part : p) {
        std::string s = part.string();
        if (s == "." || s == "..") {
            return false;
        }
    }

    return true;
}

bool createBundle(const std::string& name, AgentCommandContext& ctx, std::string& outPath)
{
    outPath = makeBundlePath(name);
    std::filesystem::create_directories(outPath);

    std::string stateText = DebugState::build(
        ctx.frame, ctx.fps, ctx.frameTimeMs, ctx.renderCommandCount,
        ctx.scene, ctx.selected, ctx.lastScriptPath, ctx.lastCapturePath);

    DebugState::writeToFile(stateText, outPath + "state.txt");

    if (ctx.renderer) {
        std::string screenshotPath = outPath + "screenshot.png";
        ctx.renderer->requestScreenshot(screenshotPath);
        ctx.lastCapturePath = screenshotPath;
    }

    return true;
}

} // namespace Bundle
