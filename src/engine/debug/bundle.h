#pragma once

#include <string>

struct AgentCommandContext;

namespace Bundle {

// Returns the directory path for a bundle name: "bundles/<name>/".
std::string makeBundlePath(const std::string& name);

// Rejects empty names, path separators, and traversal components.
bool isValidBundleName(const std::string& name);

// Creates bundles/<name>/, writes state.txt synchronously, and queues
// screenshot.png for asynchronous capture via the renderer.
// Returns true if the directory and state file were created. The screenshot
// is only queued if a renderer is available.
bool createBundle(const std::string& name, AgentCommandContext& ctx, std::string& outPath);

} // namespace Bundle
