#include <string.h>
#include "lsfs.h"
#include "sqlite3.h"
#include "disk/disk_access.h"
#include "arcs_ap.h"
#include "FreeRTOS.h"
#include "task.h"
#include "log_print.h"

#define FLASHDISK_DEVICE "SD:"
#define FLASHDISK_MOUNT_POINT "/" FLASHDISK_DEVICE
#define SQLITE_EXEC_ENC_DB  (1)

#define CPU_FREQ_HZ (300 * 1000000)

static sqlite3 *db_vt = NULL;
#define ENCRYPT_KEY "123456"
#if (CONFIG_CSK_SQLITE)
#define SQLITE_ENC_DB_FILE FLASHDISK_MOUNT_POINT "/test_csk_enc.db"
#else
#define SQLITE_ENC_DB_FILE FLASHDISK_MOUNT_POINT "/test_wx_enc.db"
#endif
#define SQLITE_DEC_DB_FILE FLASHDISK_MOUNT_POINT "/test.db"

#if(SQLITE_EXEC_ENC_DB)
#define SQLITE_EXEC_DB_FILE SQLITE_ENC_DB_FILE
#else
#define SQLITE_EXEC_DB_FILE SQLITE_DEC_DB_FILE
#endif
// #define COMMAND "SELECT c.text as word_text,a.id,a.word_id,a.dynasty,a.poem,a.tts_text,a.translation,a.explain,a.grade,b.value,b.memo FROM dictionary_word c LEFT JOIN `dictionary_zh_poem` a on c.id = a.word_id LEFT JOIN dictionary_meta b on a.author = b.id WHERE c.id = 100667 limit 1;"
#define COMMAND "select docid,type,audio_type,word_text from v_word WHERE word_text MATCH '小*时*不*识*月*，*呼*作*白*玉*盘*。'"
static const char *data_base = "Callback function called";
#define TICKS_TO_MS(ticks) ((ticks) * portTICK_PERIOD_MS)

static struct lsfs_mount_t flash_lsfs_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = FLASHDISK_MOUNT_POINT,
    .fs_data = NULL,
};

static int fs_mount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_mount(mp);
    if (ret != 0)
    {
        CLOG("Failed to mount filesystem: %d", ret);
        return ret;
    }
    CLOG("%s mounted successfully", mp->mnt_point);
}

static int fs_unmount(struct lsfs_mount_t *mp)
{
    int ret;

    ret = lsfs_unmount(mp);
    if (ret != 0)
    {
        CLOG("%s unmount failed!\n", mp->mnt_point);
    }
    CLOG("%s unmount success!\n", mp->mnt_point);
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
    CLOG("%s opendir: %d", path, ret);
    if (ret == 0)
    {

        while (1)
        {
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0)
                break; /* Error or end of dir */

            if (entry.type == LSFS_DIR_ENTRY_DIR)
            {
                /* Directory */
                CLOG("   <DIR>   %s", entry.name);
                ndir++;
            }
            else
            {
                /* File */
                CLOG("%10lu %s", entry.size, entry.name);
                nfile++;
            }
        }
        lsfs_closedir(&dir);
        CLOG("%d dirs, %d files.", ndir, nfile);
    }
    else
    {
        CLOG("Failed to open \"%s\". (%u)", path, ret);
    }
    return ret;
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    const char *userData = (const char *)data;

    CLOG("%s", userData); // Print the user data passed to callback

    // for (i = 0; i < argc; i++)
    // {
    //     CLOG("%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
    // }
    // CLOG("-------------------"); // Separator between rows

    return 0;
}

static int db_exec(void)
{
    char *zErrMsg;
    uint32_t open_time = 0, key_time = 0, query_time = 0;
    sqlite3_initialize();
    TickType_t cycles_prev = xTaskGetTickCount();


    int rc = sqlite3_open(SQLITE_EXEC_DB_FILE, &db_vt);
    if (rc) {
        CLOG("Can't open database: %s\n", SQLITE_EXEC_DB_FILE);
        return rc;
    }
    open_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);


#if(SQLITE_EXEC_ENC_DB)
    cycles_prev = xTaskGetTickCount();
    rc = sqlite3_key(db_vt, ENCRYPT_KEY, strlen(ENCRYPT_KEY));
    if (rc != SQLITE_OK) {
        CLOG("sqlite3_key  fail rc: %d", rc);
        return -1;
    }
    key_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);
#endif

    cycles_prev = xTaskGetTickCount();
    rc = sqlite3_exec(db_vt, COMMAND, callback, (void *)data_base, &zErrMsg);
    if (rc) {
        CLOG("db_exec_base command : %s failed,rc:%d,err:%s", COMMAND, rc, zErrMsg);
        sqlite3_free(zErrMsg);
        return rc;
    }

    query_time = TICKS_TO_MS(xTaskGetTickCount() - cycles_prev);

    CLOG("\nSDK: %s\nDB: %s\nSQL: %s\n"
         "+------------+-------+\n"
         "| Operation  | Time  |\n"
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
         SQLITE_EXEC_DB_FILE,
         COMMAND,
         open_time, key_time, query_time);

    sqlite3_close(db_vt);
}

extern int sdmmc_hard_init(void);
int main(int argc, char **argv)
{
    logInit(0, 115200); 
    CLOG("SQLITE sample");

    sdmmc_hard_init();
    disk_init(NULL);
    lsfs_init();

    fs_mount(&flash_lsfs_mnt);
    list_dir(FLASHDISK_MOUNT_POINT "/");

    db_exec();
    fs_unmount(&flash_lsfs_mnt);
    while (1)
    {
        ;
    }
    return 0;
}
