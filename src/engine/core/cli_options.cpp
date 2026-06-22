#include "core/cli_options.h"

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

bool readNext(int argc, const char* argv[], int& i, std::string& out,
              std::vector<std::string>& errors)
{
    if (i + 1 < argc && !isOption(argv[i + 1])) {
        out = argv[++i];
        return true;
    }
    errors.emplace_back(std::string("Missing argument for ") + argv[i]);
    return false;
}

bool readNextInt(int argc, const char* argv[], int& i, int& out,
                 std::vector<std::string>& errors)
{
    if (i + 1 >= argc || isOption(argv[i + 1])) {
        errors.emplace_back(std::string("Missing argument for ") + argv[i]);
        return false;
    }

    const char* value = argv[++i];
    try {
        size_t idx = 0;
        out = std::stoi(value, &idx);
        return idx == std::strlen(value);
    } catch (...) {
        errors.emplace_back(std::string("Invalid integer for ") + argv[i - 1] + ": " + value);
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
            readNext(argc, argv, i, options.runScript, options.errors);
        } else if (isFlag(arg, "--bundle")) {
            readNext(argc, argv, i, options.bundleName, options.errors);
        } else if (isFlag(arg, "--run-tests")) {
            options.runTests = true;
        } else if (isFlag(arg, "--exit")) {
            options.autoExit = true;
        } else if (isFlag(arg, "--frames")) {
            readNextInt(argc, argv, i, options.extraFrames, options.errors);
        }
    }

    return options;
}
