#include <cstdio>

void runTestCore();
void runTestScene();
void runTestAgent();
void runTestRenderer();
void runTestSerialization();
void runTestPicking();
void runTestDebug();
void runTestTestSuite();
void runTestUndo();

int main()
{
    runTestCore();
    runTestScene();
    runTestAgent();
    runTestRenderer();
    runTestSerialization();
    runTestPicking();
    runTestDebug();
    runTestTestSuite();
    runTestUndo();

    std::printf("All tests passed.\n");
    return 0;
}
