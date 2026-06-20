#pragma once

#include "agent/command.h"

#include <filesystem>
#include <string>

struct ScriptResult {
    size_t executed = 0;
    bool success = false;
    std::string output;
    std::string error;
};

// Executes commands from a text file, one per line.
// Blank lines and lines beginning with '#' are ignored.
// Execution stops on the first failing command.
// ctx.lastScriptPath is updated to the script filename on a successful run.
ScriptResult runScript(AgentCommandContext& ctx,
                       const std::filesystem::path& path,
                       const std::string& filename);
