#include "core/cli_options.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

bool isFlag(const char* arg, const char* flag)
{
    return std::strcmp(arg, flag) == 0;
}

bool isOption(const char* arg)
{
    return std::strlen(arg) >= 2 && arg[0] == '-' && arg[1] == '-';
}

bool readNext(int argc, const char* argv[], int& i, std::string& out)
{
    if (i + 1 < argc && !isOption(argv[i + 1])) {
        out = argv[++i];
        return true;
    }
    std::fprintf(stderr, "Missing argument for %s\n", argv[i]);
    return false;
}

bool readNextInt(int argc, const char* argv[], int& i, int& out)
{
    if (i + 1 >= argc || isOption(argv[i + 1])) {
        std::fprintf(stderr, "Missing argument for %s\n", argv[i]);
        return false;
    }

    const char* value = argv[++i];
    try {
        size_t idx = 0;
        out = std::stoi(value, &idx);
        return idx == std::strlen(value);
    } catch (...) {
        std::fprintf(stderr, "Invalid integer for %s: %s\n", argv[i - 1], value);
        return false;
    }
}

} // namespace

CliOptions parseCliOptions(int argc, const char* argv[])
{
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (isFlag(arg, "--run-script")) {
            readNext(argc, argv, i, options.runScript);
        } else if (isFlag(arg, "--bundle")) {
            readNext(argc, argv, i, options.bundleName);
        } else if (isFlag(arg, "--run-tests")) {
            options.runTests = true;
        } else if (isFlag(arg, "--exit")) {
            options.autoExit = true;
        } else if (isFlag(arg, "--frames")) {
            readNextInt(argc, argv, i, options.extraFrames);
        }
    }

    return options;
}
