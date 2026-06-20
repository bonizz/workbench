#include "core/application.h"
#include "core/cli_options.h"

int main(int argc, const char* argv[])
{
    @autoreleasepool {
        Application app;
        app.setCliOptions(parseCliOptions(argc, argv));
        if (app.init()) {
            app.run();
        }
        app.shutdown();
    }

    return 0;
}
