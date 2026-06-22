#include "core/application.h"
#include "core/cli_options.h"

#include <cstdio>

int main(int argc, const char* argv[])
{
    @autoreleasepool {
        Application app;
        CliOptions options = parseCliOptions(argc, argv);
        for (const std::string& error : options.errors) {
            std::fprintf(stderr, "%s\n", error.c_str());
        }
        app.setCliOptions(std::move(options));
        if (!app.init()) {
            return 1;
        }
        return app.run();
    }
}
