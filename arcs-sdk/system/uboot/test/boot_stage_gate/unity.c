#include "unity.h"

#include <stdio.h>

UNITY_STORAGE_T Unity;

int UnityBegin(const char *filename)
{
    (void)filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.CurrentTestFailed = 0;
    return 0;
}

void UnityConcludeTest(void)
{
    if (Unity.CurrentTestFailed) {
        Unity.TestFailures++;
        printf("[FAIL] %s:%d\n", Unity.CurrentTestName, Unity.CurrentTestLineNumber);
    } else {
        printf("[PASS] %s\n", Unity.CurrentTestName);
    }

    Unity.CurrentTestFailed = 0;
}

int UnityEnd(void)
{
    printf("%u tests, %u failures\n", Unity.NumberOfTests, Unity.TestFailures);
    return Unity.TestFailures == 0 ? 0 : 1;
}
