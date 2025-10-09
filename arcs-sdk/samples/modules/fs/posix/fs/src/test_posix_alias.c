#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "lsfs.h"
#include "lvfs.h"
#include "disk/disk_access.h"
#include "lisa_log.h"

static const char test_str[] = "hello world!";
static FILE *file = NULL;

#define RAMDISK_DEVICE      "RAM:"
#define RAMDISK_MOUNT_POINT "/"RAMDISK_DEVICE
#define TEST_FILE RAMDISK_MOUNT_POINT"/test_fopen.txt"

#define TC_FAIL   (-1)
#define TC_PASS    (0)

static int test_fopen(void)
{
    file = fopen(TEST_FILE, "w+");
    if (file == NULL) {
        LOGE("Failed opening file: errno=%d\n", errno);
        return TC_FAIL;
    }

    LOGI("[%s] pass", __FUNCTION__);
    return TC_PASS;
}

static int test_fwrite(void)
{
    size_t brw;
    int res;

    res = fseek(file, 0, SEEK_SET);
    if (res != 0) {
        LOGE("fseek failed [%d]\n", res);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    brw = fwrite(test_str, 1, strlen(test_str), file);
    if (brw < strlen(test_str)) {
        LOGE("Failed writing to file or volume full [%d]\n", (int)brw);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    if (fflush(file) != 0) {
        LOGE("Failed to flush file\n");
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    LOGI("[%s] pass", __FUNCTION__);
    return TC_PASS;
}

static int test_fread(void)
{
    size_t brw;
    int res;
    char read_buff[80];
    size_t sz = strlen(test_str);

    res = fseek(file, 0, SEEK_SET);
    if (res != 0) {
        LOGE("fseek failed [%d]\n", res);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    brw = fread(read_buff, 1, sz, file);
    if (brw < sz) {
        LOGE("Failed reading file [%d]\n", (int)brw);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    read_buff[brw] = 0;

    if (strcmp(test_str, read_buff)) {
        LOGE("Error - Data read does not match data written\n");
        LOGE("Data read:\"%s\"\n\n", read_buff);
        return TC_FAIL;
    }

    /* Now test after non-zero fseek. */
    res = fseek(file, 2, SEEK_SET);
    if (res != 0) {
        LOGE("fseek failed [%d]\n", res);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    brw = fread(read_buff, 1, sizeof(read_buff) - 1, file);
    if (brw == 0) {
        LOGE("Failed reading file [%d]\n", (int)brw);
        fclose(file);
        file = NULL;
        return TC_FAIL;
    }

    read_buff[brw] = 0;

    if (strcmp(test_str + 2, read_buff)) {
        LOGE("Error - Data read does not match data written\n");
        LOGE("Data read:\"%s\"\n\n", read_buff);
        return TC_FAIL;
    }
    LOGI("[%s] pass", __FUNCTION__);
    return TC_PASS;
}

static int test_fclose(void)
{
    int res = 0;

    if (file != NULL) {
        res = fclose(file);
        if (res != 0) {
            LOGE("Failed closing file: %d, errno=%d\n", res, errno);
        }
        file = NULL;
    }
    if (res != 0) {
        return TC_FAIL;
    }
    LOGI("[%s] pass", __FUNCTION__);
    return TC_PASS;
}

int test_posix_alias_operations(void)
{
    int res;

    LOGI("#### Test stdio operations ####\n");

    res = test_fopen();
    if (res != TC_PASS) {
        return res;
    }

    res = test_fwrite();
    if (res != TC_PASS) {
        return res;
    }

    res = test_fread();
    if (res != TC_PASS) {
        return res;
    }

    res = test_fclose();
    if (res != TC_PASS) {
        return res;
    }

    LOGI("#### Test stdio operations complete ####\n");
    return TC_PASS;
}