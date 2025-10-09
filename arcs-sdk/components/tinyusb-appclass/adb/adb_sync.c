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

#include "adb_sync_ext_disk.h"

#ifndef CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT
#define CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT "/RAM:/adb/"
#endif

#define ADB_SYNC_FS_ROOT CONFIG_ADB_PUSH_PULL_DEFAULT_ROOT

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
	uint8_t *full_path = NULL;
	uint32_t len;
	if (file_name[0] != '/') {
		len = strlen(ADB_SYNC_FS_ROOT) + strlen(file_name) + 1;
	} else {
		len = strlen(file_name) + 1;
	}

	full_path = ADB_MALLOC(len);

	if (full_path == NULL) {
		return NULL;
	}

	memset(full_path, 0, len);
	if (file_name[0] != '/') {
		strcat(full_path, ADB_SYNC_FS_ROOT);
	}

	strcat(full_path, file_name);

	return full_path;
}

static uint8_t *adb_get_dir_path(const char *file_name)
{
    uint32_t idx = strlen(file_name) - 1;

    while (idx >= 0) {
        if (file_name[idx] == '/') {
            break;
        }
        idx--;
    }

    if (idx < 0) {
        return NULL;
    }

    uint32_t len = idx + 1;

    uint8_t *dir_path = (uint8_t *)malloc(len + 1);
    if (dir_path == NULL) {
        return NULL;
    }

    memcpy(dir_path, file_name, len);
    dir_path[len] = '\0';

    return dir_path;
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

static void *adb_file_open_create(const char *name)
{
	struct lsfs_file_t *f = ADB_MALLOC(sizeof(struct lsfs_file_t));
	uint8_t *file_path = NULL;

	if (f == NULL) {
		return NULL;
	}

	if (name[0] != '/') {
		file_path = ADB_MALLOC(strlen(ADB_SYNC_FS_ROOT) + strlen(name) + 1);
		if (file_path == NULL) {
			ADB_LOGE("adb file open failed, malloc error");
			ADB_FREE(f);
			return NULL;
		}
		strcat(file_path, ADB_SYNC_FS_ROOT);
	} else {
		file_path = ADB_MALLOC(strlen(name) + 1);
		if (file_path == NULL) {
			ADB_LOGE("adb file open failed, malloc error");
			ADB_FREE(f);
			return NULL;
		}
	}

	memset(f, 0, sizeof(struct lsfs_file_t));
	strcat(file_path, name);

	int r = lsfs_open(f, file_path, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
	if (r != 0) {
		ADB_FREE(f);
		ADB_FREE(file_path);
		ADB_LOGE("adb file open failed, %d\n", r);
		return NULL;
	}

	return f;
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
	ADB_LOGI("sync stat, stat file name: %s, full path: %s\n", file_name, full_path);
	memset(ls_stat, 0, sizeof(struct lsfs_dirent));
	struct adb_sync_rsp_stat stat = {0};
	stat.id = ADB_SYNC_ID_STAT;

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint32_t start_addr = 0;
		uint32_t size = 0;
		r = adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		if (r == 0) {
			stat.mode = 33188;
			stat.size = size;
			stat.time = 0;
		}
	} else {
		r = lsfs_stat(full_path, ls_stat);
		if (r == 0) {
			/* normal file, 0o100644 */
			stat.mode = ls_stat->type == 0 ? 33188 : 16877;
			stat.size = ls_stat->size;

			/* TOTO: time */
			stat.time = 0;
			ADB_LOGI("file details: name=%s, size=%d, type=%d\n", ls_stat->name, ls_stat->size,
				ls_stat->type);
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

	uint8_t *file_name = ADB_MALLOC(req->len + 1);
	uint8_t *buff = NULL;
	const int buff_size = MAX_PAYLOAD;
	int r;

	if (file_name == NULL) {
		ADB_LOGE("do_send, file_name ADB_MALLOC error\n");
		goto failed;
	}
	file_name[req->len] = '\0';
	buff = ADB_MALLOC(buff_size);
	if (buff == NULL) {
		ADB_LOGE("do_send, buff ADB_MALLOC error\n");
		goto failed;
	}

	adb_sync_buffer_read(ctx, file_name, req->len);

	/* format: filename,mode */
	for (int i = 0; i < req->len; i++) {
		if (file_name[i] == ',') {
			file_name[i] = '\0';
			break;
		}
	}

	uint8_t *full_path = adb_get_full_path(file_name);
	if (full_path == NULL) {
		ADB_LOGE("do_send, adb_get_full_path error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	ADB_LOGI("sync send, file name: %s, full path: %s\n", file_name, full_path);

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint32_t start_addr = 0;
		uint32_t size = 0;
		adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		ADB_LOGI("do_send, ext disk access, disk_name:%s, start_addr:0x%x, size:0x%x\n", disk_name, start_addr, size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr);
		if (disk_ctx == NULL) {
			ADB_LOGE("do send, ext disk init error\n");
			goto failed;
		}
		while (1) {
			struct adb_sync_send_data req;
			adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));

			if (req.id != ADB_SYNC_ID_DATA) {
				if (req.id == ADB_SYNC_ID_DONE) {
					ADB_LOGI("do_send, done, timestamp:%d\n", req.chunk_size);
					adb_sync_rsp_okay(s);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto done;
				} else {
					ADB_LOGE("do_send, unknown req id:%x\n", req.id);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
			if (req.chunk_size > 65536) {
				ADB_LOGE("do_send, invalid chunksize: %d\n", req.chunk_size);
				adb_sync_ext_disk_ctx_free(disk_ctx);
				goto failed;
			}
			while (req.chunk_size) {
				uint8_t *p = NULL;
				int n = 0;
				n = adb_sync_buffer_claim(ctx, &p, req.chunk_size);
				req.chunk_size -= n;
				r = adb_sync_ext_disk_write(disk_ctx, p, n);
				adb_sync_buffer_commit(ctx, n);
				if (r) {
					ADB_LOGE("do_send, ext disk write error: %d\n", r);
					adb_sync_ext_disk_ctx_free(disk_ctx);
					goto failed;
				}
			}
		}
	}

    struct lsfs_file_t fp = {0};

	uint8_t *dir_path = adb_get_dir_path(full_path);
	ADB_LOGI("sync send, dir path: %s\n", dir_path);
	if (dir_path == NULL) {
		ADB_LOGE("do_send, adb_get_dir_path error\n");
		adb_sync_rsp_fail(s);
		goto failed;
	}

	struct lsfs_dir_t dir;
	uint8_t *p = dir_path + 1;
	for (; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			ADB_LOGI("sync send, mkdir dir path: %s\n", dir_path);
			lsfs_dir_t_init(&dir);
			r = lsfs_opendir(&dir, dir_path);
			if (r) {
				r = lsfs_mkdir(dir_path);
				if (r) {
					ADB_LOGE("lsfs_mkdir error: %d\n", r);
					lsfs_closedir(&dir);
					adb_sync_rsp_fail(s);
					ADB_FREE(dir_path);
					goto failed;
				}
			} else {
				lsfs_closedir(&dir);
			}
			*p = '/';
		}
	}
	ADB_FREE(dir_path);

	r = lsfs_open(&fp, full_path, LSFS_O_WRITE | LSFS_O_CREATE | LSFS_O_TRUNC);
	if (r != 0) {
		ADB_LOGE("open file error: %d\n", r);
		adb_sync_rsp_fail(s);
		goto failed;
	}

	/**
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * DATA(4) CHUNK_SIZE(4) DATAS(CHUNK_SIZE)
	 * ....
	 * DONE(4) TIME(4)
	 */
	uint32_t total_recv_size = 0;
	while (1) {
		struct adb_sync_send_data req;
		adb_sync_buffer_read(ctx, (uint8_t *)&req, sizeof(struct adb_sync_send_data));
		uint8_t *cmd = (uint8_t *)&req.id;

		if (req.id != ADB_SYNC_ID_DATA) {
			if (req.id == ADB_SYNC_ID_DONE) {
				ADB_LOGI("do_send, done, timestamp:%d\n", req.chunk_size);
				adb_sync_rsp_okay(s);
				goto done;
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
			r = lsfs_write(&fp, p, n);
			adb_sync_buffer_commit(ctx, n);
			total_size += r;
			ADB_LOGD("do_send, file saved %d bytes", total_size);
			if (r != n) {
				ADB_LOGE("lsfs write error: %d\n", r);
				goto failed;
			}
		}
	}

failed:
	adb_sync_rsp_fail(s);

done:
	ADB_FREE(file_name);
	ADB_FREE(buff);
	ADB_FREE(full_path);

	adb_file_close(&fp);
}

static void do_recv(struct adb_service *s, struct adb_sync_req *req)
{
	uint8_t *full_path = NULL;
	uint8_t *path = NULL;
	uint8_t *read_buf = NULL;
	int r;
	const uint32_t chunk_size = MAX_PAYLOAD;
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

	ADB_LOGI("sync recv, path: %s, full path: %s\n", path, full_path);
	struct adb_sync_send_data sd = {0};
	sd.id = ADB_SYNC_ID_DATA;

	if (adb_sync_is_ext_disk_access(full_path)) {
		char *disk_name = NULL;
		uint32_t start_addr = 0;
		uint32_t size = 0;

		adb_sync_ext_disk_get_info_by_path(full_path, &disk_name, &start_addr, &size);
		ADB_LOGI("do_recv, ext disk access, disk_name:%s, start_addr:0x%x, size:0x%x\n", disk_name, start_addr, size);
		struct adb_sync_ext_disk_ctx *disk_ctx = adb_sync_ext_disk_ctx_init(disk_name, start_addr);
		if (disk_ctx == NULL) {
			ADB_LOGE("do recv, ext disk init error\n");
			adb_sync_rsp_fail(s);
			goto failed;
		}
		uint32_t read_size = chunk_size / disk_ctx->sec_size * disk_ctx->sec_size;

		while (1) {
			r = adb_sync_ext_disk_read(disk_ctx, read_buf, size > read_size ? read_size : size);
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
