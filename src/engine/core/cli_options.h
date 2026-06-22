#pragma once

#include <string>
#include <vector>

struct CliOptions
{
    std::string runScript;
    std::string bundleName;
    bool runTests = false;
    bool autoExit = false;
    int extraFrames = 3;

    // Diagnostic messages for malformed input (e.g. a flag missing its value).
    // parseCliOptions is a pure function: it never writes to stderr itself. The
    // caller decides whether and how to surface these. Empty on a clean parse.
    std::vector<std::string> errors;
};

// Parses command-line options used for agent automation.
// Unknown flags are ignored. Parse errors (missing/invalid arguments) are
// collected in CliOptions::errors rather than printed; fields that failed to
// parse keep their default values.
CliOptions parseCliOptions(int argc, const char* argv[]);
