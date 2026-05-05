#include "unity.h"

#ifdef RUN_TEST
#undef RUN_TEST
#endif
#define RUN_TEST(TestFunc, TestLineNum) UnityDefaultTestRun(TestFunc, #TestFunc, TestLineNum)

#include <stdbool.h>

#include "boot_adb_path_policy.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_paths_are_rejected_without_tf_filesystem_support(void)
{
    TEST_ASSERT_FALSE(boot_adb_path_policy_is_allowed("update.bin", false));
    TEST_ASSERT_FALSE(boot_adb_path_policy_is_allowed("/RAM:/adb/update.bin", false));
    TEST_ASSERT_FALSE(boot_adb_path_policy_is_allowed("/SD:/adb/update.bin", false));
    TEST_ASSERT_TRUE(boot_adb_path_policy_is_allowed("/RAW/FLASH/0x30000/0x1000", false));
}

void test_relative_paths_only_prefix_default_root_when_tf_filesystem_is_enabled(void)
{
    TEST_ASSERT_FALSE(boot_adb_path_policy_should_prefix_default_root("update.bin", false));
    TEST_ASSERT_TRUE(boot_adb_path_policy_should_prefix_default_root("update.bin", true));
    TEST_ASSERT_FALSE(boot_adb_path_policy_should_prefix_default_root("/RAW/FLASH/0x30000/0x1000", true));
}

void test_filesystem_paths_are_allowed_when_tf_filesystem_is_enabled(void)
{
    TEST_ASSERT_TRUE(boot_adb_path_policy_is_allowed("update.bin", true));
    TEST_ASSERT_TRUE(boot_adb_path_policy_is_allowed("/RAM:/adb/update.bin", true));
    TEST_ASSERT_TRUE(boot_adb_path_policy_is_allowed("/SD:/adb/update.bin", true));
    TEST_ASSERT_TRUE(boot_adb_path_policy_is_allowed("/RAW/FLASH/0x30000/0x1000", true));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_paths_are_rejected_without_tf_filesystem_support, __LINE__);
    RUN_TEST(test_relative_paths_only_prefix_default_root_when_tf_filesystem_is_enabled, __LINE__);
    RUN_TEST(test_filesystem_paths_are_allowed_when_tf_filesystem_is_enabled, __LINE__);

    return UNITY_END();
}
