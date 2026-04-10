#include <string.h>

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "disk/disk_access.h"
#include "lsfs.h"
#include "lisa_sdmmc.h"
#include "sqlite3.h"

#define TAG "sqlite3_sample"
#include <lisa_log.h>

#define FLASHDISK_DEVICE      "SD:"
#define FLASHDISK_MOUNT_POINT "/" FLASHDISK_DEVICE
#define SQLITE_EXEC_ENC_DB    (1)

static sqlite3 *db_vt = NULL;
#define ENCRYPT_KEY "123456"
#if (CONFIG_CSK_SQLITE)
#define SQLITE_ENC_DB_FILE FLASHDISK_MOUNT_POINT "/test_csk_enc.db"
#else
#define SQLITE_ENC_DB_FILE FLASHDISK_MOUNT_POINT "/test_wx_enc.db"
#endif
#define SQLITE_DEC_DB_FILE FLASHDISK_MOUNT_POINT "/test.db"

#if (SQLITE_EXEC_ENC_DB)
#define SQLITE_EXEC_DB_FILE SQLITE_ENC_DB_FILE
#else
#define SQLITE_EXEC_DB_FILE SQLITE_DEC_DB_FILE
#endif
// #define COMMAND "SELECT c.text as
// word_text,a.id,a.word_id,a.dynasty,a.poem,a.tts_text,a.translation,a.explain,a.grade,b.value,b.memo FROM
// dictionary_word c LEFT JOIN `dictionary_zh_poem` a on c.id = a.word_id LEFT JOIN dictionary_meta b on a.author = b.id
// WHERE c.id = 100667 limit 1;"
#define COMMAND                                                                                                        \
    "select docid,type,audio_type,word_text from v_word WHERE word_text MATCH '小*时*不*识*月*，*呼*作*白*玉*盘*。'"

#define COMMAND_CREATE                                                                                                 \
    "CREATE TABLE IF NOT EXISTS v_word (docid INTEGER PRIMARY KEY, type INTEGER, audio_type INTEGER, word_text TEXT);"
#define COMMAND_INSERT                                                                                                 \
    "INSERT INTO v_word (docid,type,audio_type,word_text) VALUES (1001,1,1,'测试数据1'),(1002,1,1,'测试数据2');"
#define COMMAND_QUERY  "SELECT * FROM v_word WHERE word_text LIKE '测试%';"
#define COMMAND_DELETE "DELETE FROM v_word WHERE docid>=1001 AND docid<=1002;"

/* Run a simple CRUD flow (create -> delete cleanup -> insert -> query -> delete cleanup) */
#define SQLITE_RUN_CRUD_TEST (0)

static const char *data_base = "Callback function called";
#define TICKS_TO_MS(ticks) ((ticks) * portTICK_PERIOD_MS)

static int exec_sql_step(sqlite3 *db, const char *sql, int (*cb)(void *, int, char **, char **), void *cb_data,
                         const char *step, uint32_t *elapsed_ms, bool ignore_no_such_module, bool *module_missing)
{
    char *err_msg = NULL;
    TickType_t t0 = xTaskGetTickCount();
    int rc = sqlite3_exec(db, sql, cb, cb_data, &err_msg);
    if (elapsed_ms != NULL) {
        *elapsed_ms = TICKS_TO_MS(xTaskGetTickCount() - t0);
    }
    if (rc != SQLITE_OK) {
        if (ignore_no_such_module && err_msg != NULL && strstr(err_msg, "no such module") != NULL) {
            if (module_missing != NULL) {
                *module_missing = true;
            }
            LISA_LOGW(TAG, "%s skipped: %s", step, err_msg);
            sqlite3_free(err_msg);
            return SQLITE_OK;
        }
        LISA_LOGE(TAG, "%s failed, rc:%d, err:%s, sql:%s", step, rc, err_msg ? err_msg : "(null)", sql);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

static struct lsfs_mount_t flash_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = FLASHDISK_MOUNT_POINT,
    .fs_data = NULL,
};

static int fs_mount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_mount(mp);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to mount filesystem: %d", ret);
        return ret;
    }
    LISA_LOGI(TAG, "%s mounted successfully", mp->mnt_point);
    return 0;
}

static int fs_unmount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_unmount(mp);
    if (ret != 0) {
        LISA_LOGE(TAG, "%s unmount failed!\n", mp->mnt_point);
    }
    LISA_LOGI(TAG, "%s unmount success!\n", mp->mnt_point);
    return ret;
}

static int list_dir(const char *path)
{
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;
    int nfile = 0, ndir = 0;
    /* 初始化目录对象 */
    lsfs_dir_t_init(&dir);

    ret = lsfs_opendir(&dir, path);
    LISA_LOGI(TAG, "%s opendir: %d", path, ret);
    if (ret == 0) {

        while (1) {
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0) {
                break; /* Error or end of dir */
            }

            if (entry.type == LSFS_DIR_ENTRY_DIR) {
                /* Directory */
                LISA_LOGI(TAG, "   <DIR>   %s", entry.name);
                ndir++;
            } else {
                /* File */
                LISA_LOGI(TAG, "%10lu %s", entry.size, entry.name);
                nfile++;
            }
        }
        lsfs_closedir(&dir);
        LISA_LOGI(TAG, "%d dirs, %d files.", ndir, nfile);
    } else {
        LISA_LOGI(TAG, "Failed to open \"%s\". (%u)", path, ret);
    }
    return ret;
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    const char *userData = (const char *)data;

    LISA_LOGI(TAG, "%s", userData); // Print the user data passed to callback

    for (i = 0; i < argc; i++) {
        LISA_LOGI(TAG, "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    LISA_LOGI(TAG, "-------------------"); // Separator between rows

    return 0;
}

static int db_exec(void)
{
    uint32_t open_time = 0, key_time = 0, query_time = 0;
#if (SQLITE_RUN_CRUD_TEST)
    uint32_t create_time = 0, cleanup1_time = 0, insert_time = 0, crud_query_time = 0, cleanup2_time = 0;
#endif
    sqlite3_initialize();
    TickType_t cycles_prev = xTaskGetTickCount();

    LISA_LOGI(TAG, "Opening database: %s", SQLITE_EXEC_DB_FILE);

    int rc = sqlite3_open_v2(SQLITE_EXEC_DB_FILE, &db_vt, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc) {
        LISA_LOGE(TAG, "Can't open database: %s\n", SQLITE_EXEC_DB_FILE);
        return rc;
    }
    open_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);

#if (SQLITE_EXEC_ENC_DB)
    cycles_prev = xTaskGetTickCount();
    rc = sqlite3_key(db_vt, ENCRYPT_KEY, strlen(ENCRYPT_KEY));
    if (rc != SQLITE_OK) {
        LISA_LOGE(TAG, "sqlite3_key  fail rc: %d", rc);
        return -1;
    }
    key_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);
#endif

#if (SQLITE_RUN_CRUD_TEST)
    rc = exec_sql_step(db_vt, COMMAND_CREATE, NULL, NULL, "CRUD CREATE", &create_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    /* Cleanup first to make repeated runs deterministic */
    rc = exec_sql_step(db_vt, COMMAND_DELETE, NULL, NULL, "CRUD DELETE(cleanup)", &cleanup1_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    rc = exec_sql_step(db_vt, COMMAND_INSERT, NULL, NULL, "CRUD INSERT", &insert_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    rc = exec_sql_step(db_vt, COMMAND_QUERY, callback, (void *)data_base, "CRUD QUERY", &crud_query_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    rc = exec_sql_step(db_vt, COMMAND_DELETE, NULL, NULL, "CRUD DELETE(cleanup)", &cleanup2_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    LISA_LOGI(TAG,
              "\nCRUD(ms):\n"
              "  CREATE : %u\n"
              "  CLEAN1 : %u\n"
              "  INSERT : %u\n"
              "  QUERY  : %u\n"
              "  CLEAN2 : %u",
              create_time, cleanup1_time, insert_time, crud_query_time, cleanup2_time);
#endif

    cycles_prev = xTaskGetTickCount();
    rc = exec_sql_step(db_vt, COMMAND, callback, (void *)data_base, "QUERY", &query_time, false, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db_vt);
        return rc;
    }

    query_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);

    LISA_LOGI(TAG,
              "\nSDK: %s\nDB: %s\nSQL: %s\n"
              "+------------+-------+\n"
              "| Operation  | Time(ms)  |\n"
              "+------------+-------+\n"
              "| Open      | %4u  |\n"
              "| Key       | %4u  |\n"
              "| Query     | %4u  |\n"
              "+------------+-------+",
#if (CONFIG_WX_SQLITE3)
              "WX_SQLITE3",
#else
              "CSK_SQLITE3",
#endif
              SQLITE_EXEC_DB_FILE, COMMAND, open_time, key_time, query_time);

    sqlite3_close(db_vt);
    return 0;
}

extern int sdmmc_hard_init(void);
int main(int argc, char **argv)
{
    LISA_LOGI(TAG, "SQLITE3 sample");

    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
    lsfs_init();

    fs_mount(&flash_lsfs_mnt);
    list_dir(FLASHDISK_MOUNT_POINT "/");

    vTaskDelay(pdMS_TO_TICKS(1000));
    db_exec();
    fs_unmount(&flash_lsfs_mnt);
    while (1) {
        ;
    }
    return 0;
}
