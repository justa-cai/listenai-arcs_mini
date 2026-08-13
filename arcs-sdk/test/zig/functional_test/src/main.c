#define LOG_TAG "func_test"
#include <lisa_log.h>

extern int zig_functional_test(void);

int main(int argc, char **argv)
{
    LOGI("=== Zig Functional Test Suite ===");

    int fails = zig_functional_test();

    if (fails == 0) {
        LOGI("=== ALL FUNCTIONAL TESTS PASSED ===");
    } else {
        LOGE("=== %d FUNCTIONAL TESTS FAILED ===", fails);
    }

    return fails;
}
