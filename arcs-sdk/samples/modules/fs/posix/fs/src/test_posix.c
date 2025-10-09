#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "lsfs.h"
#include "lvfs.h"
#include "disk/disk_access.h"
#include "lisa_log.h"

const char test_str[] = "hello world!";
static int file = -1;

#define RAMDISK_DEVICE      "RAM:"
#define RAMDISK_MOUNT_POINT "/"RAMDISK_DEVICE
#define TEST_FILE RAMDISK_MOUNT_POINT"/testfile.txt"

#define TC_FAIL   (-1)
#define TC_PASS    (0)

static int test_open(void)
{
	int res;

	res = open(TEST_FILE, O_CREAT | O_RDWR, 0660);
	if (res < 0) {
		LOGE("Failed opening file: %d, errno=%d\n", res, errno);
        return TC_FAIL;
	}

	file = res;
    LOGI("[%s] pass", __FUNCTION__);
	return TC_PASS;
}

int test_write(void)
{
	ssize_t brw;
	off_t res;

	res = lseek(file, 0, SEEK_SET);
	if (res != 0) {
		LOGE("lseek failed [%d]\n", (int)res);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	brw = write(file, (char *)test_str, strlen(test_str));
	if (brw < 0) {
		LOGE("Failed writing to file [%d]\n", (int)brw);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	if (brw < strlen(test_str)) {
		LOGE("Unable to complete write. Volume full.\n");
		LOGE("Number of bytes written: [%d]\n", (int)brw);
		close(file);
		file = -1;
		return TC_FAIL;
	}
    LOGI("[%s] pass", __FUNCTION__);
	return res;
}

static int test_read(void)
{
	ssize_t brw;
	off_t res;
	char read_buff[80];
	size_t sz = strlen(test_str);

	res = lseek(file, 0, SEEK_SET);
	if (res != 0) {
		LOGE("lseek failed [%d]\n", (int)res);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	brw = read(file, read_buff, sz);
	if (brw < 0) {
		LOGE("Failed reading file [%d]\n", (int)brw);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	read_buff[brw] = 0;

	if (strcmp(test_str, read_buff)) {
		LOGE("Error - Data read does not match data written\n");
		LOGE("Data read:\"%s\"\n\n", read_buff);
		return TC_FAIL;
	}

	/* Now test after non-zero lseek. */

	res = lseek(file, 2, SEEK_SET);
	if (res != 2) {
		LOGE("lseek failed [%d]\n", (int)res);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	brw = read(file, read_buff, sizeof(read_buff));
	if (brw < 0) {
		LOGE("Failed reading file [%d]\n", (int)brw);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	/* Check for array overrun */
	brw = (brw < 80) ? brw : brw - 1;

	read_buff[brw] = 0;

	if (strcmp(test_str + 2, read_buff)) {
		LOGE("Error - Data read does not match data written\n");
		LOGE("Data read:\"%s\"\n\n", read_buff);
		return TC_FAIL;
	}
    LOGI("[%s] pass", __FUNCTION__);
	return TC_PASS;
}

static int test_close(void)
{
	int res = 0;

	if (file >= 0) {
		res = close(file);
		if (res < 0) {
			LOGE("Failed closing file: %d, errno=%d\n", res, errno);
		}

		file = -1;
	}
    if (res < 0) {
        return TC_FAIL;
    }
    LOGI("[%s] pass", __FUNCTION__);
	return res;
}

static int test_fsync(void)
{
	int res = 0;

	if (file < 0) {
		return res;
	}

	res = fsync(file);
	if (res < 0) {
		LOGE("Failed to sync file: %d, errno = %d\n", res, errno);
		return TC_FAIL;
	}

    LOGI("[%s] pass", __FUNCTION__);
	return res;
}

static int test_truncate(void)
{
	int res = 0;
    ssize_t brw;
    char read_buff[80];
	size_t truncate_size = sizeof(test_str) - 4;

	if (file < 0) {
		return res;
	}

	res = ftruncate(file, truncate_size);
	if (res) {
		LOGE("Error truncating file [%d]\n", res);
		return TC_FAIL;
	}

    res = lseek(file, 0, SEEK_SET);
	if (res != 0) {
		LOGE("lseek failed [%d]\n", (int)res);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	brw = read(file, read_buff, sizeof(read_buff));
	if (brw < 0) {
		LOGE("Failed reading file [%d]\n", (int)brw);
		close(file);
		file = -1;
		return TC_FAIL;
	}

	if ((brw != truncate_size) || (0 != memcmp(test_str, read_buff, brw))) {
		LOGE("Error - Data read does not match data written\n");
		LOGE("Data read:\"%s\"\n\n", read_buff);
		return TC_FAIL;
	}

    LOGI("[%s] pass", __FUNCTION__);
	return res;
}


int test_posix_operations(void){

    test_open();
    test_write();
    test_read();
    test_fsync();
    test_truncate();
    test_close();
    return 0;
}
