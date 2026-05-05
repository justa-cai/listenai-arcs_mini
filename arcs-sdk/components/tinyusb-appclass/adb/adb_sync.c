#define LOG_TAG "adb.sync"

#include "adb_services.h"
#include "adb.h"
#include "adb_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lsfs.h"

#include "tusb.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <strings.h>

#include "adb_sync_ext_disk.h"
#include "adb_sync_metadata.h"
#include "adb_sync_protocol.h"
#include "adb_sync_stage.h"

#define SYNC_ID(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define ADB_SYNC_ID_LIST SYNC_ID('L', 'I', 'S', 'T')
#define ADB_SYNC_ID_RECV SYNC_ID('R', 'E', 'C', 'V')
#define ADB_SYNC_ID_SEND SYNC_ID('S', 'E', 'N', 'D')
#define ADB_SYNC_ID_STAT SYNC_ID('S', 'T', 'A', 'T')
#define ADB_SYNC_ID_FAIL SYNC_ID('F', 'A', 'I', 'L')
#define ADB_SYNC_ID_DATA SYNC_ID('D', 'A', 'T', 'A')
#define ADB_SYNC_ID_DONE SYNC_ID('D', 'O', 'N', 'E')
#define ADB_SYNC_ID_OKAY SYNC_ID('O', 'K', 'A', 'Y')
#define ADB_SYNC_ID_QUIT SYNC_ID('Q', 'U', 'I', 'T')
#define ADB_SYNC_ID_DENT SYNC_ID('D', 'E', 'N', 'T')

#if defined(CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE)
#define ADB_SYNC_STAGE_ALIGNMENT CONFIG_LISA_SDMMC_ACCESS_BUFFER_ALIGN_SIZE
#else
#define ADB_SYNC_STAGE_ALIGNMENT 64U
#endif

#define ADB_SYNC_STAGE_BUFFER_SIZE 65536U

struct adb_sync_req {
	uint32_t id;
	uint32_t len;
};

struct adb_sync_rsp_data {
	uint32_t id;
	uint32_t len;
};

struct adb_sync_rsp_stat {
	uint32_t id;
	uint32_t mode;
	uint32_t size;
	uint32_t time;
};

struct adb_sync_send_data {
	uint32_t id;
	uint32_t chunk_size;
};

struct adb_sync_done_data {
	uint32_t id;
	uint32_t time;
};

struct adb_sync_dent_data {
	uint32_t id;
	uint32_t mode;
	uint32_t size;
	uint32_t time;
	uint32_t namelen;
};

struct adb_sync_ctx {
	QueueHandle_t rx_queue;
	struct adb_service *s;
	uint32_t rd_pos;
	adb_packet_t *curr_pkt;
};

struct adb_sync_raw_stage_ctx {
	struct adb_sync_ext_disk_ctx *disk_ctx;
};

struct adb_sync_file_stage_ctx {
	struct lsfs_file_t *fp;
	uint32_t *total_size;
};

static int adb_sync_stage_init_default(struct adb_sync_stage *stage)
{
	return adb_sync_stage_init(stage, ADB_SYNC_STAGE_BUFFER_SIZE, ADB_SYNC_STAGE_ALIGNMENT);
}

static bool adb_sync_stage_try_enable(struct adb_sync_stage *stage, const char *tag)
{
	if (adb_sync_stage_init_default(stage) == 0) {
		return true;
	}

	ADB_LOGW("do_send, %s staging buffer init failed, fallback to direct write\n", tag);
	return false;
}

static void adb_sync_stage_cleanup(struct adb_sync_stage *stage, bool *stage_ready)
{
	if (!*stage_ready) {
		return;
	}

	adb_sync_stage_deinit(stage);
	*stage_ready = false;
}

static int adb_sync_raw_stage_flush(void *user_data, const uint8_t *data, uint32_t len)
{
	struct adb_sync_raw_stage_ctx *stage_ctx = user_data;

	if (stage_ctx == NULL) {
		return -1;
	}

	return adb_sync_ext_disk_write(stage_ctx->disk_ctx, data, len);
}

static int adb_sync_file_write_exact(struct lsfs_file_t *fp, const uint8_t *data, uint32_t len, uint32_t *total_size)
{
	int written;

	written = lsfs_write(fp, data, len);
	if (written != (int)len) {
		if (written == 0) {
			return -1;
		}
		return written;
	}

	*total_size += (uint32_t)written;
	ADB_LOGD("do_send, file saved %d bytes", *total_size);
	return 0;
}

static int adb_sync_file_stage_flush(void *user_data, const uint8_t *data, uint32_t len)
{
	struct adb_sync_file_stage_ctx *stage_ctx = user_data;

	if (stage_ctx == NULL || stage_ctx->fp == NULL || stage_ctx->total_size == NULL) {
		return -1;
	}

	return adb_sync_file_write_exact(stage_ctx->fp, data, len, stage_ctx->total_size);
}

static inline uint32_t adb_sync_buffer_claim(struct adb_sync_ctx *ctx, uint8_t **data, uint32_t size)
{
	uint32_t len;

	if (ctx->curr_pkt == NULL) {
		adb_packet_t *p = NULL;
		if (xQueueReceive(ctx->rx_queue, &p, portMAX_DELAY) != pdTRUE) {
			ADB_LOGE("sync buffer read timeout\n");
			return -1;
		}
		ctx->rd_pos = 0;
		ctx->curr_pkt = p;
	}

    if ((ctx->rd_pos + size) > ctx->curr_pkt->msg.data_length) {
        len = ctx->curr_pkt->msg.data_length - ctx->rd_pos;
    } else {
        len = size;
    }

    *data = ctx->curr_pkt->data + ctx->rd_pos;

	return len;
}

static inline void adb_sync_buffer_commit(struct adb_sync_ctx *ctx, uint32_t size)
{
    ctx->rd_pos += size;

    if (ctx->rd_pos >= ctx->curr_pkt->msg.data_length) {
        adb_packet_free(ctx->curr_pkt);
        ctx->curr_pkt = NULL;
        ctx->rd_pos = 0;
    }
}

static inline int adb_sync_buffer_read(struct adb_sync_ctx *ctx, uint8_t *data, uint32_t size)
{
    int len;

    while (size) {
        uint8_t *p = NULL;
        len = adb_sync_buffer_claim(ctx, &p, size);
        memcpy(data, p, len);
		adb_sync_buffer_commit(ctx, len);
        data += len;
        size -= len;
    }

    return size;
}

static uint8_t *adb_get_full_path(const char *file_name)
{
	char *full_path = NULL;
	int status;

	status = adb_sync_metadata_build_full_path(file_name, &full_path);
	if (status != ADB_SYNC_METADATA_OK) {
		ADB_LOGE("resolve sync path failed, name:%s, status:%s\n", file_name,
			 adb_sync_metadata_status_string(status));
		return NULL;
	}

	return (uint8_t *)full_path;
}

static uint8_t *adb_get_dir_path(const char *file_name)
{
    int idx = (int)strlen(file_name) - 1;

    while (idx >= 0) {
        if (file_name[idx] == '/') {
            break;
        }
        idx--;
    }

    if (idx < 0) {
        return NULL;
    }

    uint32_t len = (uint32_t)idx + 1u;

    uint8_t *dir_path = ADB_MALLOC(len + 1u);
    if (dir_path == NULL) {
        return NULL;
    }

    memcpy(dir_path, file_name, len);
    dir_path[len] = '\0';

	return dir_path;
}

static const char *adb_sync_get_basename(const char *path)
{
	const char *name = strrchr(path, '/');

	return (name == NULL) ? path : (name + 1);
}

static size_t adb_sync_get_mount_root_len(const char *path)
{
	const char *sep = strchr(path + 1, '/');

	return (sep == NULL) ? strlen(path) : (size_t)(sep - path);
}

static void adb_sync_split_name_upper(const char *name, char *base, size_t base_size,
					 char *ext, size_t ext_size)
{
	const char *dot = strrchr(name, '.');
	size_t base_len;
	size_t ext_len;
	size_t i;

	if (dot == NULL || dot == name) {
		dot = NULL;
	}

	base_len = (dot == NULL) ? strlen(name) : (size_t)(dot - name);
	if (base_len >= base_size) {
		base_len = base_size - 1U;
	}

	for (i = 0; i < base_len; i++) {
		base[i] = (char)toupper((unsigned char)name[i]);
	}
	base[base_len] = '\0';

	ext[0] = '\0';
	if (dot == NULL || ext_size == 0U) {
		return;
	}

	ext_len = strlen(dot + 1);
	if (ext_len >= ext_size) {
		ext_len = ext_size - 1U;
	}

	for (i = 0; i < ext_len; i++) {
		ext[i] = (char)toupper((unsigned char)dot[1 + i]);
	}
	ext[ext_len] = '\0';
}

static bool adb_sync_is_fat_alias_match(const char *target, const char *entry)
{
	char target_base[64];
	char target_ext[16];
	char entry_base[64];
	char entry_ext[16];
	const char *tilde = NULL;
	size_t prefix_len;
	size_t entry_ext_len;

	if (strcasecmp(target, entry) == 0) {
		return true;
	}

	adb_sync_split_name_upper(target, target_base, sizeof(target_base), target_ext, sizeof(target_ext));
	adb_sync_split_name_upper(entry, entry_base, sizeof(entry_base), entry_ext, sizeof(entry_ext));

	tilde = strchr(entry_base, '~');
	if (tilde == NULL) {
		return false;
	}

	prefix_len = (size_t)(tilde - entry_base);
	if (prefix_len == 0U || prefix_len > 6U) {
		return false;
	}

	if (strncmp(entry_base, target_base, prefix_len) != 0) {
		return false;
	}

	for (tilde++; *tilde != '\0'; tilde++) {
		if (!isdigit((unsigned char)*tilde)) {
			return false;
		}
	}

	entry_ext_len = strlen(entry_ext);
	if (entry_ext_len == 0U) {
		return target_ext[0] == '\0';
	}

	return strncmp(entry_ext, target_ext, entry_ext_len) == 0;
}

static int adb_sync_find_alias_in_dir(const char *dir_path, const char *target_name,
					 char *resolved_name, size_t resolved_name_size)
{
	struct lsfs_dir_t dir;
	struct lsfs_dirent entry = {0};
	char alias_name[MAX_FILE_NAME + 1];
	int r;

	alias_name[0] = '\0';
	lsfs_dir_t_init(&dir);
	r = lsfs_opendir(&dir, dir_path);
	if (r != 0) {
		return r;
	}

	while (1) {
		r = lsfs_readdir(&dir, &entry);
		if (r != 0 || entry.name[0] == '\0') {
			break;
		}

		if (strcasecmp(entry.name, target_name) == 0) {
			snprintf(resolved_name, resolved_name_size, "%s", entry.name);
			lsfs_closedir(&dir);
			return 0;
		}

		if (alias_name[0] == '\0' && adb_sync_is_fat_alias_match(target_name, entry.name)) {
			snprintf(alias_name, sizeof(alias_name), "%s", entry.name);
		}
	}

	lsfs_closedir(&dir);

	if (alias_name[0] != '\0') {
		snprintf(resolved_name, resolved_name_size, "%s", alias_name);
		return 0;
	}

	return -ENOENT;
}

static int adb_sync_resolve_existing_path(const char *path, bool allow_missing_tail, uint8_t **resolved_out)
{
	char component[MAX_FILE_NAME + 1];
	char resolved_name[MAX_FILE_NAME + 1];
	struct lsfs_dirent entry = {0};
	uint8_t *resolved = NULL;
	uint8_t *trial = NULL;
	const char *cursor = NULL;
	const char *next = NULL;
	size_t root_len;
	size_t component_len;
	int r;

	if (resolved_out == NULL || path == NULL || path[0] != '/') {
		return -EINVAL;
	}

	root_len = adb_sync_get_mount_root_len(path);
	resolved = ADB_MALLOC(strlen(path) + 1U);
	trial = ADB_MALLOC(strlen(path) + 1U);
	if (resolved == NULL || trial == NULL) {
		r = -ENOMEM;
		goto failed;
	}

	memcpy(resolved, path, root_len);
	resolved[root_len] = '\0';

	cursor = path + root_len;
	while (*cursor == '/') {
		cursor++;
	}

	while (*cursor != '\0') {
		next = strchr(cursor, '/');
		component_len = (next == NULL) ? strlen(cursor) : (size_t)(next - cursor);
		if (component_len == 0U || component_len > MAX_FILE_NAME) {
			r = -EINVAL;
			goto failed;
		}

		memcpy(component, cursor, component_len);
		component[component_len] = '\0';

		snprintf((char *)trial, strlen(path) + 1U, "%s/%s", resolved, component);
		r = lsfs_stat((const char *)trial, &entry);
		if (r == 0) {
			snprintf(resolved_name, sizeof(resolved_name), "%s", component);
		} else if (next == NULL && allow_missing_tail) {
			snprintf(resolved_name, sizeof(resolved_name), "%s", component);
		} else {
			r = adb_sync_find_alias_in_dir((const char *)resolved, component, resolved_name,
							 sizeof(resolved_name));
			if (r != 0) {
				goto failed;
			}
		}

		snprintf((char *)trial, strlen(path) + 1U, "%s/%s", resolved, resolved_name);
		snprintf((char *)resolved, strlen(path) + 1U, "%s", trial);
		cursor = (next == NULL) ? (cursor + component_len) : (next + 1);
	}

	*resolved_out = resolved;
	ADB_FREE(trial);
	return 0;

failed:
	ADB_FREE(trial);
	ADB_FREE(resolved);
	return r;
}

static void adb_sync_replace_path(uint8_t **path, uint8_t *resolved_path)
{
	if (path == NULL || *path == NULL || resolved_path == NULL) {
		return;
	}

	ADB_FREE(*path);
	*path = resolved_path;
}

static void adb_sync_try_resolve_existing_path(uint8_t **path, bool allow_missing_tail)
{
	uint8_t *resolved_path = NULL;

	if (path == NULL || *path == NULL) {
		return;
	}

	if (adb_sync_resolve_existing_path((const char *)*path, allow_missing_tail, &resolved_path) == 0) {
		if (strcmp((const char *)*path, (const char *)resolved_path) != 0) {
			ADB_LOGD("sync path resolved: %s -> %s\n", *path, resolved_path);
		}
		adb_sync_replace_path(path, resolved_path);
	}
}

static int adb_sync_discard_bytes(struct adb_sync_ctx *ctx, uint32_t size)
{
	while (size > 0U) {
		uint8_t *data = NULL;
		uint32_t len = adb_sync_buffer_claim(ctx, &data, size);

		if (len == 0U || len == (uint32_t)-1) {
			return -1;
		}

		adb_sync_buffer_commit(ctx, len);
		size -= len;
	}

	return 0;
}

static int adb_sync_discard_send_payload(struct adb_sync_ctx *ctx)
{
	while (1) {
		struct adb_sync_send_data req = {0};
		int r;

		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(req));
		if (req.id == ADB_SYNC_ID_DONE) {
			return 0;
		}

		if (req.id != ADB_SYNC_ID_DATA) {
			ADB_LOGE("do_send, discard unknown req id:%x\n", req.id);
			return -1;
		}

		r = adb_sync_discard_bytes(ctx, req.chunk_size);
		if (r != 0) {
			return r;
		}
	}
}

static int adb_sync_wait_dir_ready(const char *path)
{
	struct lsfs_dir_t dir;
	int r;
	int retries = 3;

	while (retries-- > 0) {
		lsfs_dir_t_init(&dir);
		r = lsfs_opendir(&dir, path);
		if (r == 0) {
			lsfs_closedir(&dir);
			return 0;
		}

		if (r != -ENOENT || retries == 0) {
			return r;
		}

		vTaskDelay(1);
	}

	return -ENOENT;
}

static int adb_sync_ensure_dir_path(uint8_t *path)
{
	struct lsfs_dir_t dir;
	uint8_t *p = NULL;
	int r = 0;

	for (p = path + 1; *p; p++) {
		if (*p != '/') {
			continue;
		}

		*p = '\0';
		ADB_LOGI("sync send, mkdir dir path: %s\n", path);
		lsfs_dir_t_init(&dir);
		r = lsfs_opendir(&dir, (const char *)path);
		if (r == 0) {
			lsfs_closedir(&dir);
			*p = '/';
			continue;
		}

		r = lsfs_mkdir((const char *)path);
		if (r != 0 && r != -EEXIST) {
			ADB_LOGE("lsfs_mkdir error: %d\n", r);
			*p = '/';
			return r;
		}

		r = adb_sync_wait_dir_ready((const char *)path);
		if (r != 0) {
			ADB_LOGW("sync send, dir verify after mkdir failed: %d, continue with alias fallback\n", r);
			r = 0;
		}

		*p = '/';
	}

	return r;
}

static int adb_sync_rebuild_full_path(uint8_t **full_path, const uint8_t *dir_path)
{
	const char *base_name = NULL;
	size_t dir_len;
	size_t base_len;
	size_t sep_len;
	uint8_t *rebuilt = NULL;

	if (full_path == NULL || *full_path == NULL || dir_path == NULL) {
		return -EINVAL;
	}

	base_name = adb_sync_get_basename((const char *)*full_path);
	dir_len = strlen((const char *)dir_path);
	base_len = strlen(base_name);
	sep_len = (dir_len > 0U && dir_path[dir_len - 1U] != '/') ? 1U : 0U;
	rebuilt = ADB_MALLOC(dir_len + sep_len + base_len + 1U);
	if (rebuilt == NULL) {
		return -ENOMEM;
	}

	memcpy(rebuilt, dir_path, dir_len);
	if (sep_len != 0U) {
		rebuilt[dir_len] = '/';
		dir_len += sep_len;
	}
	memcpy(rebuilt + dir_len, base_name, base_len + 1U);
	adb_sync_replace_path(full_path, rebuilt);
	return 0;
}

static void adb_sync_rsp_okay(struct adb_service *s)
{
	struct adb_sync_rsp_data okay_data = {
		.id = ADB_SYNC_ID_OKAY,
		.len = 0,
	};

	adb_service_write_remote(s, (uint8_t *)&okay_data, sizeof(struct adb_sync_rsp_data));
}

static void adb_sync_rsp_fail(struct adb_service *s)
{
	struct adb_sync_rsp_data fail_data = {
		.id = ADB_SYNC_ID_FAIL,
		.len = 0,
	};

	adb_service_write_remote(s, (uint8_t *)&fail_data, sizeof(fail_data));
}

static void adb_sync_rsp_stat(struct adb_service *s, struct adb_sync_rsp_stat *stat)
{
	adb_service_write_remote(s, (uint8_t *)stat, sizeof(struct adb_sync_rsp_stat));
}

static void adb_file_close(void *f)
{
	int r = lsfs_close((struct lsfs_file_t *)f);

	if (r != 0) {
		ADB_LOGE("adb file close failed, %d\n", r);
	}
}

static void do_stat(struct adb_service *s, struct adb_sync_req *req)
{
	struct adb_sync_ctx *ctx = s->data;
	uint8_t *file_name = ADB_MALLOC(req->len + 1);
	int r;
	int metadata_status;
	uint8_t *full_path = NULL;

	if (file_name == NULL) {
		ADB_LOGE("do stat,file_name ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		return;
	}
	file_name[req->len] = '\0';
	adb_sync_buffer_read(ctx, file_name, req->len);

	struct lsfs_dirent *ls_stat = ADB_MALLOC(sizeof(struct lsfs_dirent));
	if (ls_stat == NULL) {
		ADB_LOGE("do stat, ls_stat ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	full_path = adb_get_full_path(file_name);
	if (full_path == NULL) {
		ADB_LOGE("do stat, adb_get_full_path error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}
	adb_sync_try_resolve_existing_path(&full_path, false);
	ADB_LOGI("sync stat, stat file name: %s, full path: %s\n", file_name, full_path);
	memset(ls_stat, 0, sizeof(struct lsfs_dirent));
	struct adb_sync_rsp_stat stat = {0};
	stat.id = ADB_SYNC_ID_STAT;

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;
		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r == 0) {
			stat.mode = 33188;
			stat.size = size > UINT32_MAX ? UINT32_MAX : (uint32_t)size;
			stat.time = 0;
		}
	} else {
		r = lsfs_stat(full_path, ls_stat);
		if (r == 0) {
			/* normal file, 0o100644 */
			stat.mode = ls_stat->type == 0 ? 33188 : 16877;
			stat.size = ls_stat->size;
			metadata_status = adb_sync_metadata_get_timestamp((const char *)file_name, &stat.time);
			if (metadata_status == ADB_SYNC_METADATA_NOT_FOUND || metadata_status == ADB_SYNC_METADATA_BYPASS) {
				stat.time = 0;
			} else if (metadata_status != ADB_SYNC_METADATA_OK) {
				ADB_LOGE("sync stat metadata failed, name:%s, status:%s\n", file_name,
					 adb_sync_metadata_status_string(metadata_status));
				adb_sync_rsp_fail(s);
				goto failed;
			}
			ADB_LOGI("file details: name=%s, size=%d, type=%d, time=%u\n", ls_stat->name, ls_stat->size,
				ls_stat->type, stat.time);
		} else {
			ADB_LOGE("Failed to get file stat, name:%s, error:%d\n", file_name, r);
		}
	}

	adb_sync_rsp_stat(s, &stat);
failed:
	ADB_FREE(file_name);
	ADB_FREE(ls_stat);
	ADB_FREE(full_path);
}

static void do_send(struct adb_service *s, struct adb_sync_req *req)
{
	struct adb_sync_ctx *ctx = s->data;
	uint32_t total_size = 0;
	uint32_t done_timestamp = 0;
	uint8_t *file_name = ADB_MALLOC(req->len + 1);
	uint8_t *full_path = NULL;
	uint8_t *dir_path = NULL;
	struct lsfs_file_t fp = {0};
	struct adb_sync_stage file_stage = {0};
	struct adb_sync_file_stage_ctx file_stage_ctx = {0};
	bool file_opened = false;
	bool file_stage_ready = false;
	bool need_discard = true;
	int r;

	if (file_name == NULL) {
		ADB_LOGE("do_send, file_name ADB_MALLOC error\n");
		goto failed;
	}
	file_name[req->len] = '\0';
	adb_sync_buffer_read(ctx, file_name, req->len);

	/* format: filename,mode */
	for (int i = 0; i < req->len; i++) {
		if (file_name[i] == ',') {
			file_name[i] = '\0';
			break;
		}
	}

	full_path = adb_get_full_path(file_name);
	if (full_path == NULL) {
		ADB_LOGE("do_send, adb_get_full_path error\n");
		goto failed;
	}

	ADB_LOGI("sync send, file name: %s, full path: %s\n", file_name, full_path);

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;
		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r != 0) {
			ADB_LOGE("do_send, invalid raw path: %s\n", file_name);
			goto failed;
		}
		ADB_LOGI("do_send, ext disk access, disk_name:%s, start_addr:0x%llx, size:0x%llx\n", disk_name,
			 (unsigned long long)start_addr, (unsigned long long)size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr, size, true);
		struct adb_sync_stage stage = {0};
		struct adb_sync_raw_stage_ctx stage_ctx = {
			.disk_ctx = disk_ctx,
		};
		bool stage_ready = false;

		if (disk_ctx == NULL) {
			ADB_LOGE("do send, ext disk init error\n");
			goto failed;
		}
		stage_ready = adb_sync_stage_try_enable(&stage, "raw");
		while (1) {
			struct adb_sync_send_data req;
			adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

			if (req.id != ADB_SYNC_ID_DATA) {
				if (req.id == ADB_SYNC_ID_DONE) {
					if (stage_ready) {
						r = adb_sync_stage_finish(&stage, adb_sync_raw_stage_flush, &stage_ctx);
						if (r != 0) {
							ADB_LOGE("do_send, raw staging flush error: %d\n", r);
							adb_sync_stage_cleanup(&stage, &stage_ready);
							adb_sync_ext_disk_ctx_free(disk_ctx);
							goto failed;
						}
						adb_sync_stage_cleanup(&stage, &stage_ready);
					}
					ADB_LOGI("do_send, done, timestamp:%d\n", req.chunk_size);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					need_discard = false;
					adb_sync_rsp_okay(s);
					goto done;
				} else {
					ADB_LOGE("do_send, unknown req id:%x\n", req.id);
					adb_sync_stage_cleanup(&stage, &stage_ready);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
			if (req.chunk_size > 65536) {
				ADB_LOGE("do_send, invalid chunksize: %d\n", req.chunk_size);
				adb_sync_stage_cleanup(&stage, &stage_ready);
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto failed;
			}
			while (req.chunk_size) {
				uint8_t *p = NULL;
				int n = 0;
				n = adb_sync_buffer_claim(ctx, &p, req.chunk_size);
				req.chunk_size -= n;
				if (stage_ready) {
					r = adb_sync_stage_write(&stage, p, n, adb_sync_raw_stage_flush, &stage_ctx);
				} else {
					r = adb_sync_ext_disk_write(disk_ctx, p, n);
				}
				adb_sync_buffer_commit(ctx, n);
				if (r) {
					ADB_LOGE("do_send, ext disk write error: %d\n", r);
					adb_sync_stage_cleanup(&stage, &stage_ready);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
		}
	}

	dir_path = adb_get_dir_path(full_path);
	ADB_LOGI("sync send, dir path: %s\n", dir_path);
	if (dir_path == NULL) {
		ADB_LOGE("do_send, adb_get_dir_path error\n");
		goto failed;
	}

	r = adb_sync_ensure_dir_path(dir_path);
	if (r != 0) {
		goto failed;
	}
	adb_sync_try_resolve_existing_path(&dir_path, false);
	r = adb_sync_rebuild_full_path(&full_path, dir_path);
	if (r != 0) {
		goto failed;
	}
	adb_sync_try_resolve_existing_path(&full_path, true);

	r = lsfs_open(&fp, full_path, LSFS_O_WRITE | LSFS_O_CREATE | LSFS_O_TRUNC);
	if (r != 0) {
		ADB_LOGE("open file error: %d\n", r);
		goto failed;
	}
	file_opened = true;
	file_stage_ctx.fp = &fp;
	file_stage_ctx.total_size = &total_size;
	file_stage_ready = adb_sync_stage_try_enable(&file_stage, "file");

	/**
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * ....
	 * DONE(4) TIME(4)
	 */
	while (1) {
		struct adb_sync_send_data req;
		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

		if (req.id != ADB_SYNC_ID_DATA) {
			if (req.id == ADB_SYNC_ID_DONE) {
				done_timestamp = req.chunk_size;
				ADB_LOGI("do_send, done, timestamp:%u\n", done_timestamp);
				break;
			} else {
				ADB_LOGE("do_send, unknown req id:%x\n", req.id);
				goto failed;
			}
		}

		if (req.chunk_size > 65536) {
			ADB_LOGE("do_send, invalid chunksize: %d\n", req.chunk_size);
			goto failed;
		}

		while (req.chunk_size) {
			uint8_t *p = NULL;
			int n = 0;
			n = adb_sync_buffer_claim(ctx, &p, req.chunk_size);
			req.chunk_size -= n;
			if (file_stage_ready) {
				r = adb_sync_stage_write(&file_stage, p, n, adb_sync_file_stage_flush, &file_stage_ctx);
			} else {
				r = adb_sync_file_write_exact(&fp, p, n, &total_size);
			}
			adb_sync_buffer_commit(ctx, n);
			if (r != 0) {
				ADB_LOGE("lsfs write error: %d\n", r);
				goto failed;
			}
		}
	}

	if (file_stage_ready) {
		r = adb_sync_stage_finish(&file_stage, adb_sync_file_stage_flush, &file_stage_ctx);
		if (r != 0) {
			ADB_LOGE("lsfs staging flush error: %d\n", r);
			goto failed;
		}
		adb_sync_stage_cleanup(&file_stage, &file_stage_ready);
	}
	
	adb_file_close(&fp);
	file_opened = false;

	r = adb_sync_metadata_record_timestamp((const char *)file_name, done_timestamp);
	if (r != ADB_SYNC_METADATA_OK && r != ADB_SYNC_METADATA_BYPASS) {
		ADB_LOGE("sync metadata update failed, name:%s, status:%s\n", file_name,
			 adb_sync_metadata_status_string(r));
		goto failed;
	}

	adb_sync_rsp_okay(s);
	need_discard = false;
	goto done;

failed:
	if (need_discard) {
		r = adb_sync_discard_send_payload(ctx);
		if (r != 0) {
			ADB_LOGE("do_send, discard payload failed: %d\n", r);
		}
	}
	adb_sync_rsp_fail(s);

done:
	if (file_opened) {
		adb_file_close(&fp);
	}
	adb_sync_stage_cleanup(&file_stage, &file_stage_ready);
	ADB_FREE(dir_path);
	ADB_FREE(file_name);
	ADB_FREE(full_path);
}

static void do_recv(struct adb_service *s, struct adb_sync_req *req)
{
	uint8_t *full_path = NULL;
	uint8_t *path = NULL;
	uint8_t *read_buf = NULL;
	int r;
	const uint32_t chunk_size = adb_sync_data_chunk_limit(MAX_PAYLOAD);
	struct adb_sync_ctx *ctx = s->data;

	read_buf = ADB_MALLOC(chunk_size);
	if (read_buf == NULL) {
		ADB_LOGE("do recv, read buff malloc error\n");
		adb_sync_rsp_fail(s);
		return;
	}

	path = ADB_MALLOC(req->len + 1);
	if (path == NULL) {
		ADB_LOGE("do recv, path ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}
	path[req->len] = '\0';
	adb_sync_buffer_read(ctx, path, req->len);

	full_path = adb_get_full_path(path);
	if (full_path == NULL) {
		ADB_LOGE("do recv, get full path error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}
	adb_sync_try_resolve_existing_path(&full_path, false);

	ADB_LOGI("sync recv, path: %s, full path: %s\n", path, full_path);
	struct adb_sync_send_data sd = {0};
	sd.id = ADB_SYNC_ID_DATA;

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint64_t start_addr = 0;
		uint64_t size = 0;

		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r != 0 || size == 0U) {
			ADB_LOGE("do_recv, invalid raw path: %s\n", path);
			adb_sync_rsp_fail(s);
			goto failed;
		}
		ADB_LOGI("do_recv, ext disk access, disk_name:%s, start_addr:0x%llx, size:0x%llx\n", disk_name,
			 (unsigned long long)start_addr, (unsigned long long)size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr, size, false);
		if (disk_ctx == NULL) {
			ADB_LOGE("do recv, ext disk init error\n");
			adb_sync_rsp_fail(s);
			goto failed;
		}
		uint32_t read_size = chunk_size / disk_ctx->sec_size * disk_ctx->sec_size;

		while (1) {
			uint32_t request_size = size > read_size ? read_size : (uint32_t)size;
			r = adb_sync_ext_disk_read(disk_ctx, read_buf, request_size);
			if (r < 0) {
				ADB_LOGE("do recv, ext disk read error: %d\n", r);
				adb_sync_rsp_fail(s);
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto failed;
			}

			sd.id = ADB_SYNC_ID_DATA;
			sd.chunk_size = r;
			adb_service_write_remote(s, (uint8_t *)&sd,
						sizeof(struct adb_sync_send_data));
			adb_service_write_remote(s, (uint8_t *)read_buf, r);
			size -= r;

			if (size == 0) {
				ADB_LOGI("do recv, ext disk read done\n");
				sd.id = ADB_SYNC_ID_DONE;
				sd.chunk_size = 0;

				adb_service_write_remote(s, (uint8_t *)&sd,
							sizeof(struct adb_sync_send_data));
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto done;
			}
		}
	}

	struct lsfs_file_t fp = {0};
	r = lsfs_open(&fp, full_path, LSFS_O_READ | LSFS_O_RDWR);
	if (r) {
		adb_sync_rsp_fail(s);
		ADB_LOGE("do recv, lsfs open error: %d\n", r);
		goto failed;
	}

	while (1) {
		r = lsfs_read(&fp, read_buf, chunk_size);
		if (r < 0) {
			ADB_LOGE("do recv, lsfs read error: %d\n", r);
			adb_sync_rsp_fail(s);
			lsfs_close(&fp);
			goto failed;
		} else if (r == 0) {
			ADB_LOGI("do recv, lsfs read done\n");
			lsfs_close(&fp);
			sd.id = ADB_SYNC_ID_DONE;
			sd.chunk_size = 0;

			adb_service_write_remote(s, (uint8_t *)&sd,
						 sizeof(struct adb_sync_send_data));
			break;
		} else {
			sd.id = ADB_SYNC_ID_DATA;
			sd.chunk_size = r;
			adb_service_write_remote(s, (uint8_t *)&sd,
						 sizeof(struct adb_sync_send_data));
			adb_service_write_remote(s, (uint8_t *)read_buf, r);
		}
	}

failed:
done:
	ADB_FREE(full_path);
	ADB_FREE(path);
	ADB_FREE(read_buf);
}

static void do_list(struct adb_service *s, struct adb_sync_req *req)
{
	uint8_t *path = ADB_MALLOC(req->len + 1);
	struct lsfs_dir_t dir;
	struct lsfs_dirent entry;
	struct adb_sync_ctx *ctx = s->data;

	if (path == NULL) {
		ADB_LOGE("ADB_MALLOC error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	ADB_LOGI("sync list, req len: %d\n", req->len);
	adb_sync_buffer_read(ctx, path, req->len);
	path[req->len] = '\0';

	ADB_LOGI("sync list, path: %s\n", path);

	lsfs_dir_t_init(&dir);

	struct adb_sync_dent_data dent = {0};

	dent.id = ADB_SYNC_ID_DENT;

	int r = lsfs_opendir(&dir, path);
	if (r == 0) {
		while (1) {
			r = lsfs_readdir(&dir, &entry);
			if (r != 0 || entry.name[0] == 0) {
				break;
			}

			dent.namelen = strlen(entry.name);
			dent.size = entry.size;
			dent.mode = entry.type == 0 ? 33188 : 16877;
			ADB_LOGI("list, name:%s, size:%d, type:%d\n", entry.name, entry.size,
				entry.type);
			adb_service_write_remote(s, (uint8_t *)&dent,
						 sizeof(struct adb_sync_dent_data));
			adb_service_write_remote(s, (uint8_t *)entry.name, dent.namelen);
		}
	} else {
		ADB_LOGE("lsfs opendir error: %d\n", r);
	}
	lsfs_closedir(&dir);
failed:
	memset(&dent, 0, sizeof(struct adb_sync_dent_data));
	dent.id = ADB_SYNC_ID_DONE;
	adb_service_write_remote(s, (uint8_t *)&dent, sizeof(struct adb_sync_dent_data));

	if (path) {
		ADB_FREE(path);
	}
}

static void adb_sync_task(void *arg)
{
	struct adb_sync_ctx *ctx = arg;
	struct adb_service *s = ctx->s;

	assert(s);
	assert(ctx);

	uint8_t quit = 0;
	while (!quit) {
		struct adb_sync_req req;
		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_req));
		uint8_t *cmd = (uint8_t *)&req.id;
		ADB_LOGI("adb sync, cmd: %c%c%c%c\n", cmd[0], cmd[1], cmd[2], cmd[3]);
		switch (req.id) {
		case ADB_SYNC_ID_LIST:
			do_list(s, &req);
			break;
		case ADB_SYNC_ID_RECV:
			do_recv(s, &req);
			break;
		case ADB_SYNC_ID_SEND:
			do_send(s, &req);
			break;
		case ADB_SYNC_ID_STAT:
			do_stat(s, &req);
			break;
		case ADB_SYNC_ID_QUIT:
			quit = 1;
			break;
		default:
			ADB_LOGE("adb sync, unknown req id:%x\n", req.id);
			quit = 1;
			break;
		}
	}
	ADB_LOGI("adb sync, quit\n");
	adb_close(s->local_id, s->remote_id);
	vTaskDelete(NULL);
}

static int adb_sync_open(struct adb_service *s, const uint8_t *args)
{
	struct adb_sync_ctx *ctx = ADB_MALLOC(sizeof(struct adb_sync_ctx));

	if (ctx == NULL) {
		return -1;
	}

	memset(ctx, 0, sizeof(struct adb_sync_ctx));

	ADB_LOGI("adb sync open, local_id:%d, remote_id:%d\n", s->local_id, s->remote_id);

	ctx->rx_queue = xQueueCreate(20, sizeof(adb_packet_t *));

	if (ctx->rx_queue == NULL) {
		ADB_FREE(ctx);
		ADB_LOGE("sync queue create failed\n");
		return -1;
	}

	ctx->s = s;
	s->data = ctx;

	BaseType_t xReturn = xTaskCreate(adb_sync_task, "sync_task", 1024 * 2, ctx, CONFIG_ADB_TASK_PRIORITY - 1, NULL);
	if (xReturn != pdPASS) {
		vQueueDelete(ctx->rx_queue);
		ADB_FREE(ctx);
		ADB_LOGE("sync task create failed\n");
		return -1;
	}

	return 0;
}

int adb_write_file(const char *path, uint8_t *data, uint32_t len)
{
	return 0;
}

static int adb_sync_write(struct adb_service *s, adb_packet_t *p)
{
	struct adb_sync_ctx *ctx = s->data;

	if (ctx && ctx->rx_queue) {
		xQueueSend(ctx->rx_queue, &p, portMAX_DELAY);
	} else {
		ADB_LOGE("sync write failed\n");
	}

	return 0;
}

static int adb_sync_close(struct adb_service *s)
{
	if (s == NULL || s->data == NULL) {
		return -1;
	}

	struct adb_sync_ctx *ctx = s->data;
	if (ctx->curr_pkt) {
		adb_packet_free(ctx->curr_pkt);
		ctx->curr_pkt = NULL;
	}

	if (ctx->rx_queue) {
		vQueueDelete(ctx->rx_queue);
		ctx->rx_queue = NULL;
	}

	ADB_FREE(ctx);

	return 0;
}

static const struct adb_service_handle adb_sync_handle = {
	.name = "sync",
	.open = adb_sync_open,
	.close = adb_sync_close,
	.write = adb_sync_write,
};

int adb_sync_init(void)
{
	return adb_service_hd_register(&adb_sync_handle);
}

int adb_sync_prepare(void)
{
	return 0;
}
