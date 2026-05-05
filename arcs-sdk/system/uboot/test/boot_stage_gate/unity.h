#ifndef UNITY_H
#define UNITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *CurrentTestName;
    int CurrentTestLineNumber;
    unsigned int NumberOfTests;
    unsigned int TestFailures;
    int CurrentTestFailed;
} UNITY_STORAGE_T;

extern UNITY_STORAGE_T Unity;

int UnityBegin(const char *filename);
int UnityEnd(void);
void UnityConcludeTest(void);

#define TEST_PROTECT() 1

#define TEST_ASSERT_TRUE(condition)                                                                                   \
    do {                                                                                                             \
        if (!(condition)) {                                                                                          \
            Unity.CurrentTestFailed = 1;                                                                             \
            Unity.CurrentTestLineNumber = __LINE__;                                                                  \
            return;                                                                                                  \
        }                                                                                                            \
    } while (0)

#define TEST_ASSERT_FALSE(condition) TEST_ASSERT_TRUE(!(condition))

#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                                                                   \
    do {                                                                                                             \
        if ((uint32_t)(expected) != (uint32_t)(actual)) {                                                            \
            Unity.CurrentTestFailed = 1;                                                                             \
            Unity.CurrentTestLineNumber = __LINE__;                                                                  \
            return;                                                                                                  \
        }                                                                                                            \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
