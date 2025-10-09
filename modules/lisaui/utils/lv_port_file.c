#include <stdio.h>
#include "lv_port_file.h"
#if CONFIG_LVFS_POSIX_API

#elif CONFIG_LSFS
#include "lsfs.h"
#include "esp_heap_caps.h"
#endif
#include "lisa_log.h"

#if CONFIG_LVFS_POSIX_API

/**
 * Open a file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable
 * @param path path to the file beginning with the driver letter (e.g. S:/folder/file.txt)
 * @param mode read: FS_MODE_RD, write: FS_MODE_WR, both: FS_MODE_RD | FS_MODE_WR
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    FILE *file_p;

    if (mode == LV_FS_MODE_WR)
        file_p = fopen(path, "wb+");
    else if (mode == LV_FS_MODE_RD)
        file_p = fopen(path, "rb");
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
        file_p = fopen(path, "wb+");

    // CLOG("Open file %s %p\n", path, file_p);

    return (void *)file_p;
}


/**
 * Close an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    fclose((file_p));

    // CLOG("fs_close file %p\n", file_p);
    return res;
}

/**
 * Read data from an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param buf pointer to a memory block where to store the read data
 * @param btr number of Bytes To Read
 * @param br the real number of read bytes (Byte Read)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    *br = fread((char *)buf, 1, btr, file_p);
    if (*br == 0)
        printf("ftell %ld %p %d\n", ftell(file_p), buf, btr);

    return res;
}

static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    *bw = fwrite((char *)buf, 1, btw, file_p);

    return res;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open )
 * @param pos the new position of read write pointer
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    fseek(file_p, pos, whence);

    return res;
}

static lv_fs_res_t fs_tell(struct _lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    *pos_p = ftell(file_p);

    return res;
}

void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv = {0};

    lv_fs_drv_init(&fs_drv);

    /*Set up fields...*/
    fs_drv.letter = 'A';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    lv_fs_drv_register(&fs_drv);
}


#elif CONFIG_LSFS

/**
 * Open a file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable
 * @param path path to the file beginning with the driver letter (e.g. S:/folder/file.txt)
 * @param mode read: FS_MODE_RD, write: FS_MODE_WR, both: FS_MODE_RD | FS_MODE_WR
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    struct lsfs_file_t* pfile;
    int ret;

    // LOGI("[%s %d]Open file %s\n",__FUNCTION__,__LINE__,path);
    
    pfile = heap_caps_malloc(sizeof(struct lsfs_file_t), MALLOC_CAP_SPIRAM);
    if(!pfile){
        LOGE("[%s %d]No memory\n",__FUNCTION__,__LINE__);
        return NULL;
    }
    lsfs_file_t_init(pfile);
    if (mode == LV_FS_MODE_WR)
        ret = lsfs_open(pfile,path, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_RDWR);
    else if (mode == LV_FS_MODE_RD)
        ret = lsfs_open(pfile,path, LSFS_O_READ);
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
        ret = lsfs_open(pfile,path, LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_RDWR);

    if(ret != 0){
        heap_caps_free(pfile);
        LOGE("[%s %d]lsfs_open failed,ret:%d\n",__FUNCTION__,__LINE__,ret);
        return NULL;
    }

    // CLOG("Open file %s %p\n", path, pfile);

    return (void *)pfile;
}


/**
 * Close an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    if(NULL == file_p){
        CLOG("[%s %d]file_p is NULL\n",__FUNCTION__,__LINE__);
        return LV_FS_RES_INV_PARAM;
    }
    /* Add your code here*/
    lsfs_close((struct lsfs_file_t* )file_p);
    heap_caps_free(file_p);
    // CLOG("fs_close file %p\n", file_p);
    return res;
}

/**
 * Read data from an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param buf pointer to a memory block where to store the read data
 * @param btr number of Bytes To Read
 * @param br the real number of read bytes (Byte Read)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    lv_fs_res_t res = LV_FS_RES_OK;
    // LOGI("[%s %d]Read file %p, btr:%d\n",__FUNCTION__,__LINE__,file_p,btr);

    /* Add your code here*/
    *br = lsfs_read(file_p, buf, btr);
    if (*br <= 0){
        CLOG("Failed to read from file: %d", *br);
    }

    return res;
}

static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    *bw = lsfs_write(file_p, buf, btw);
    if (*bw != btw)
    {
        CLOG("Failed to write to file: %d", btw);
    }
    return res;
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open )
 * @param pos the new position of read write pointer
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    /* Add your code here*/
    lsfs_seek(file_p, pos, whence);
    return res;
}

static lv_fs_res_t fs_tell(struct _lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    lv_fs_res_t res = LV_FS_RES_OK;

    *pos_p = lsfs_tell(file_p);

    return res;
}

void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv = {0};

    lv_fs_drv_init(&fs_drv);

    /*Set up fields...*/
    fs_drv.letter = 'A';
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    lv_fs_drv_register(&fs_drv);
}
#else
void lv_port_fs_init(void)
{
    CLOG("lv_port_fs_init not implemented\n");
}
#endif
