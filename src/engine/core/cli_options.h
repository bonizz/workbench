#pragma once

#include <string>

struct CliOptions
{
    std::string runScript;
    std::string bundleName;
    bool runTests = false;
    bool autoExit = false;
    int extraFrames = 3;
};

// Parses command-line options used for agent automation.
// Unknown flags are ignored.
CliOptions parseCliOptions(int argc, const char* argv[]);
