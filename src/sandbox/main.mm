#include "core/application.h"

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    @autoreleasepool {
        Application app;
        if (app.init()) {
            app.run();
        }
        app.shutdown();
    }

    return 0;
}
