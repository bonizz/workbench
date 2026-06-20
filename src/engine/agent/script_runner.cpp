#include "agent/script_runner.h"

#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string& s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

} // namespace

ScriptResult runScript(AgentCommandContext& ctx,
                       const std::filesystem::path& path,
                       const std::string& filename)
{
    ScriptResult result;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.error = "Could not open script: " + path.string();
        return result;
    }

    std::ostringstream transcript;
    std::string line;
    size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        std::string command = trim(line);
        if (command.empty() || command.front() == '#') {
            continue;
        }

        AgentCommandResult commandResult = executeCommand(command, ctx);
        transcript << command << "\n-> " << commandResult.output << "\n\n";

        if (!commandResult.success) {
            result.output = transcript.str();
            result.error = "Error on line " + std::to_string(lineNumber)
                         + " (" + command + "): " + commandResult.output;
            return result;
        }

        ++result.executed;
    }

    ctx.lastScriptPath = filename;

    result.success = true;
    result.output = transcript.str();
    return result;
}
