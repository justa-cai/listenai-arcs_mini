#include "wordseg_res.h"
#include "mpu_wrappers.h"
#include "wordseg_wrapper.h"
#include "lib_sdc.h"
#include "crc32.h"
#include "cache.h"

// #define SHAREMEM                   (0x280BC000)
#define SHAREMEM               (0)
#if SHAREMEM
static int32_t sharemem_size = 16*1024;
static int32_t sharemem_used_size = 0;
#endif
/*----------------------------以下信息和TF卡资源存放位置、csk的烧录位置强相关，请修改------------------------*/
#define CONFIG_EMMC_RES_OFFSET_ADDR (0x6000000UL)
#define COPY_RES_BY_FS              (0) //通过文件系统把资源拷贝到指定secotor

#define  HASHRES_SIZE               (3333960)
#define  DATARES_SIZE               (9740722)
#if COPY_RES_BY_FS
#define WS_HASHRES_SD_OFFSET          (RES_WORDSEG_START_ADDR)//目前SD卡lcthink 8G卡实际7616MB，预留116MB,使用磁盘重新分区1大小为7500MB
#define WS_DATARES_SD_OFFSET          (WS_HASHRES_SD_OFFSET + 50*1024*1024UL) //hash.bin预留50MB，data.bin预留66MB（当前50W词条hash.bin 3MB,data.bin 8MB）
#else
#define WS_HASHRES_SD_OFFSET         (0x5500000UL+CONFIG_EMMC_RES_OFFSET_ADDR) // hash.bin资源在TF卡的偏移地址
#define WS_DATARES_SD_OFFSET         (0x4B00000UL+CONFIG_EMMC_RES_OFFSET_ADDR) // data.bin资源在TF卡的偏移地址
#endif

#define SD_CARD_SECTOR_SIZE             (512)       // 等于low_sdcard.c里的s_card->csd.sector_size

/* ---------------------------离线测试相关项----------------------------- */
#define OFFLINE_TEST_WORDSEG            (1) //离线测试分词性能
#define READ_DATA_RES_LOG               (1) // 打印资源读取次数等信息
/* 备忘：查看psram和ram配置等看gcc_arm.ld 38行左右，分配内部ram用malloc，分配psram用heap_psram_malloc */

static int sdio_read(void *buf, int sct, int cnt, void *user);
static int wordseg_run_entry(char *text);
static int read_hash_res_func(void *p, int offset, int size, void **dst, int dst_size);
static int read_data_res_func(void *p, int offset, int size, void **dst, int dst_size);
static void free_callback(void *p);
static void * malloc_callback(unsigned int size, int type);
//把分词资源读到预留的sd卡位置，因为分词资源是裸读sd卡，要保障文件存储的连续性
// static int read_file_to_sd_sectors(char *file_path, unsigned long long sd_offset, int *file_size);

#if OFFLINE_TEST_WORDSEG
//用于离线测试和统计分词性能
static int wordseg_offline_test(void *engine, char *text);
static int offline_test_wordseg_tree(void *engine, friso_token_t result, int result_num, FILE *fp);
static void printf_wordseg_result(friso_token_t result, int num);
static void test_tf();
#endif

#define OS_MEM_IRAM    0   //芯片内部ram
#define OS_MEM_ERAM    1   //芯片外部ram 
#ifndef NULL
#define NULL 0
#endif

ws_engine_t *ws_engine = NULL;

//calc res crc
static void calc_res_crc(void)
{
    char *buf = os_mem_alloc_ext(4*1024, OS_MEM_ERAM);
    //hash res
    uint32_t crc = 0;
    uint32_t sct = WS_HASHRES_SD_OFFSET/SD_CARD_SECTOR_SIZE;
    int cnt = 8;
    int size = HASHRES_SIZE;
    while(1)
    {
        sdio_read(buf, sct, cnt, NULL);
        crc = crc32_calc(buf, size>=4*1024 ? 4*1024 : size, crc);
        size -= 4*1024;
        if(size<=0){
            break;
        }
        sct += 8;
    }
    LOGD("hash res crc:%x", crc);
    crc = 0;
    sct = WS_DATARES_SD_OFFSET/SD_CARD_SECTOR_SIZE;
    size = DATARES_SIZE;
    while(1)
    {
        sdio_read(buf, sct, cnt, NULL);
        crc = crc32_calc(buf, size>=4*1024 ? 4*1024 : size, crc);
        size -= 4*1024;
        if(size<=0){
            break;
        }
        sct += 8;
    }
    LOGD("data res crc:%x", crc);
    os_mem_free(buf);
    return;
}

// 分词引擎初始化相关
int wordseg_init_entry()
{
    int ret = 0;
    int32_t is_failed = 0;

    LOGD("[wordseg]version:%s\n", wordseg_getversion());    

    calc_res_crc();
#if COPY_RES_BY_FS
    //如果分词资源有更新，就要拷贝到预留的SD卡位置
    read_file_to_sd_sectors(WS_HASHRES_FILE_PATH, WS_HASHRES_SD_OFFSET, NULL);
    read_file_to_sd_sectors(WS_DATARES_FILE_PATH, WS_DATARES_SD_OFFSET, NULL);
#endif

    if (NULL == ws_engine)
    {
        ws_engine = os_mem_alloc_ext(sizeof(ws_engine_t), OS_MEM_ERAM);
        if (ws_engine == NULL)
        {
            LOGE("[wordseg]malloc PSRAM %d Bytes failed.\n", sizeof(ws_engine_t));
            is_failed = 1;
            goto wordseg_init_end;
        }
        LOGD("[wordseg]malloc PSRAM: sizeof(ws_engine_t) = %d, p=%p", sizeof(ws_engine_t), ws_engine);
    }

    int ticks = os_ticks_get();

    PHashResHdr hash_hdr = NULL;
    void *hash_res_buf = NULL; // hash.bin资源文件加载到内存buf指针

    // 读取data.bin资源
    int data_res_handle = 0; // data.bin资源句柄，如果是使用读文件方式就是文件句柄，如果是裸读sd卡，就是文件在sd卡的偏移地址
    data_res_handle = WS_DATARES_SD_OFFSET;

#if 0 // 看下SD卡里的资源对不对,调试阶段使用
    PDataResHdr data_hdr = (PDataResHdr)data_res_handle;  

    int tmp_buf_size = SD_CARD_SECTOR_SIZE;
    char *tmp_buf = (char *)os_mem_alloc_ext(tmp_buf_size, OS_MEM_ERAM);
    if (NULL == tmp_buf)
    {
        LOGE("[wordseg] malloc tmp_buf failed. iram size=%d\n", tmp_buf_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    memset(tmp_buf, 0, tmp_buf_size);

    read_data_res_func((void *)0x100, 0, tmp_buf_size, (void **)&tmp_buf, tmp_buf_size);
    data_hdr = (PDataResHdr)tmp_buf;    
    LOGD("data res hdr info: crc=%u, crc_size=%u, version=%s, checkstr=%u, matchcrc=%u\n",
                data_hdr->file_crc, data_hdr->file_crc_size, data_hdr->version, data_hdr->check_str, data_hdr->hash_data_match_crc);

    read_hash_res_func((void *)0x100, 0, tmp_buf_size, (void **)&tmp_buf, tmp_buf_size);
    hash_hdr = (PHashResHdr)tmp_buf;
    LOGD("hash res hdr info: crc=%u, crc_size=%u, version=%s, checkstr=%u, matchcrc=%u\n",
                hash_hdr->file_crc, hash_hdr->file_crc_size, hash_hdr->version, hash_hdr->check_str, hash_hdr->hash_data_match_crc);

    os_mem_free(tmp_buf);
#endif

    // 分配传递分词结果的结构体
    ws_engine->result_size = WS_TOKEN_MAX_NUM * (sizeof(wordseg_result_t) + 3) + WS_INPUT_TEXT_MAX_LEN * 3; // 第一个3表示word/word_delpunc/word_tts结尾的/0,第二个3上述三种文本总字符大小不会超过输入文本最大值WS_INPUT_TEXT_MAX_LEN
#if SHAREMEM
    if(ws_engine->result_size + sharemem_used_size > sharemem_size)
    {
        LOGE("[wordseg]malloc sharemem %d Bytes failed.\n", ws_engine->result_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    ws_engine->result = (wordseg_result_t *)(SHAREMEM + sharemem_used_size);
    sharemem_used_size += ws_engine->result_size;
    LOGD("[wordseg]use sharemem: ws_engine->result size=%d, p=%p", ws_engine->result_size, ws_engine->result);
#else
    ws_engine->result = os_mem_alloc_ext(ws_engine->result_size, OS_MEM_ERAM);
    if (ws_engine->result == NULL)
    {
        LOGE("[wordseg]malloc PSRAM %d Bytes failed.\n", ws_engine->result_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    LOGD("[wordseg]malloc PSRAM: ws_engine->result size=%d, p=%p", ws_engine->result_size, ws_engine->result);
#endif

    ws_engine->read_res_buf_size = 4 * 1024;
    ws_engine->read_res_buf = os_mem_alloc_ext(ws_engine->read_res_buf_size, OS_MEM_ERAM); //wordseg_init就会用到,先分配一次
    if(NULL == ws_engine->read_res_buf)
    {
        LOGE("[wordseg]malloc ws_engine->read_res_buf failed. iram size=%d\n", ws_engine->read_res_buf_size);
        is_failed = 1;
        goto wordseg_init_end;
    }

    // 获取分词引擎需要的内存
    char dump[4] = {0};
    hash_res_buf = dump; //其实没用，给个假的先
    ret = wordseg_init_ext(NULL, &ws_engine->resident_buf_size, NULL, &ws_engine->tmp_buf_size, hash_res_buf, read_hash_res_func, (void *)(data_res_handle), read_data_res_func);
    // ret = wordseg_init(NULL, &ws_engine->resident_buf_size, NULL, &ws_engine->tmp_buf_size, hash_res_buf, (void *)(data_res_handle), read_data_res_func);
    if (ret != TC_WSErrID_TellSize)
    {
        LOGE("[wordseg]wordseg_init_ext err:%d\n", ret);
        is_failed = 1;
        goto wordseg_init_end;
    }
   
    ws_engine->resident_buf = os_mem_alloc_ext(ws_engine->resident_buf_size, OS_MEM_ERAM);
    ws_engine->tmp_buf = os_mem_alloc_ext(ws_engine->tmp_buf_size, OS_MEM_ERAM);
    if (NULL == ws_engine->resident_buf || NULL == ws_engine->tmp_buf)
    {
        LOGE("[wordseg] malloc ws_engine->resident_buf/ws_engine->tmp_buf failed.\n");
        is_failed = 1;
        goto wordseg_init_end;
    }
    LOGD("[wordseg]malloc PSRAM: ws_engine->resident_buf size=%d, p=%p", ws_engine->resident_buf_size, ws_engine->resident_buf);
    LOGD("[wordseg]malloc PSRAM: ws_engine->tmp_buf size=%d, p=%p", ws_engine->tmp_buf_size, ws_engine->tmp_buf);

    // 分词引擎初始化    
    ret = wordseg_init_ext(ws_engine->resident_buf, &ws_engine->resident_buf_size, ws_engine->tmp_buf, &ws_engine->tmp_buf_size, hash_res_buf, read_hash_res_func, (void *)(data_res_handle), read_data_res_func);
    if (ret != 0)
    {
        LOGE("[wordseg]wordseg_init err:%d\n", ret);
        is_failed = 1;
        goto wordseg_init_end;
    }    

    wordseg_get_read_data_buf_size(ws_engine->resident_buf, (int *)&ws_engine->read_res_buf_size);
    if (ws_engine->read_res_buf != NULL)
    {
        os_mem_free(ws_engine->read_res_buf);
        ws_engine->read_res_buf = NULL;
    }
#if 0//SHAREMEM
    if(ws_engine->read_res_buf_size + sharemem_used_size > sharemem_size)
    {
        LOGE("[wordseg]malloc sharemem %d Bytes failed 2.\n", ws_engine->read_res_buf_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    ws_engine->read_res_buf = (void *)(SHAREMEM + sharemem_used_size);
    sharemem_used_size += ws_engine->read_res_buf_size;    
    LOGD("[wordseg]use sharemem: read_sd_res_buf size=%d, p=%p", ws_engine->read_res_buf_size, ws_engine->read_res_buf);
#else
    ws_engine->read_res_buf = os_mem_alloc_ext(ws_engine->read_res_buf_size, OS_MEM_ERAM);
    if (NULL == ws_engine->read_res_buf)
    {
        LOGE("[wordseg]malloc read_sd_res_buf size=%d, failed.\n", ws_engine->read_res_buf_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    LOGD("[wordseg]malloc PSRAM: read_sd_res_buf size=%d, p=%p", ws_engine->read_res_buf_size, ws_engine->read_res_buf);
#endif
    ret = wordseg_set_memcallback(ws_engine->resident_buf, malloc_callback, free_callback);

    LOGD("[wordseg] wordseg_init_entry exit. used time=%d ms", os_ticks_get() - ticks);    

    return ret;

wordseg_init_end:
    if (is_failed)
    {        
        if(NULL != ws_engine && NULL != ws_engine->read_res_buf)
        {
            os_mem_free(ws_engine->read_res_buf);
            ws_engine->read_res_buf = NULL;
        }
        if (NULL != ws_engine && NULL != hash_res_buf)
        {
            os_mem_free(hash_res_buf);
            hash_res_buf = NULL;
        }
        if (NULL != ws_engine && NULL != ws_engine->resident_buf)
        {
            os_mem_free(ws_engine->resident_buf);
            ws_engine->resident_buf = NULL;
        }
        if (NULL != ws_engine && NULL != ws_engine->tmp_buf)
        {
            os_mem_free(ws_engine->tmp_buf);
            ws_engine->tmp_buf = NULL;
        }     
#if SHAREMEM
        sharemem_used_size = 0;
#else
        if (NULL != ws_engine->result)
        {
            os_mem_free(ws_engine->result);
            ws_engine->result = NULL;
        }       
#endif   
        if(NULL != ws_engine)
        {
            os_mem_free(ws_engine);
            ws_engine = NULL;
        }
        
        LOGE("[wordseg] wordseg_init_entry exit. failed. used time=%d ms", os_ticks_get() - ticks);
    }

    return -1;
}

int wordseg_uninit_entry()
{
#if SHAREMEM
    sharemem_used_size = 0;
#endif

    if (NULL == ws_engine)
        return -1;

    if (NULL != ws_engine->read_res_buf)
    {
        os_mem_free(ws_engine->read_res_buf);
        ws_engine->read_res_buf = NULL;
    }
    if (NULL != ws_engine->resident_buf)
    {
        os_mem_free(ws_engine->resident_buf);
        ws_engine->resident_buf = NULL;
    }
    if (NULL != ws_engine->tmp_buf)
    {
        os_mem_free(ws_engine->tmp_buf);
        ws_engine->tmp_buf = NULL;
    }
#if !SHAREMEM    
    if (NULL != ws_engine->result)
    {
        os_mem_free(ws_engine->result);
        ws_engine->result = NULL;
    }   
#endif
    if (NULL != ws_engine)
    {
        os_mem_free(ws_engine);
        ws_engine = NULL;
    }

    return 0;
}

static int sdio_read(void *buf, int sct, int cnt, void *user){
    int ret;

    ret = gm_sdc_api_sdcard_sector_read(SD_0, sct, cnt, buf);
    HAL_InvalidateDCache_by_Addr(buf, cnt*SD_CARD_SECTOR_SIZE);
    if (ret != 0)
    {
        return -1;
    }
    return 0;
}

static int my_read_sd_sectors(void *p, int offset, int size, void **dst, int dst_size, unsigned long long file_offset, void *buffer, int32_t buffer_size)
{
    int ret = 0;
    unsigned long long sd_offset = file_offset + offset;                               // 由于data.bin在SD卡的偏移超过的32bit范围，先直接用宏定义，后续该void*p类型
    unsigned int read_size = ivGridSize(size + offset % SD_CARD_SECTOR_SIZE, SD_CARD_SECTOR_SIZE); // 需要是block的倍数
    unsigned long sector = sd_offset / SD_CARD_SECTOR_SIZE;
    unsigned int count = read_size / SD_CARD_SECTOR_SIZE;

    if (NULL == *dst)
    {
        if (count * SD_CARD_SECTOR_SIZE > buffer_size)
        {
            LOGE("[wordseg]error out of memory %d, %d\n", read_size, ws_engine->read_res_buf_size);
            return TC_WSErrID_ReadDataResErr;            
        }
        // 这种情况一般是wordseg_do调用,直接用外部申请的IRAM指针read_data_buf进行low_sd_read_sectors加速
        //LOGE("[%d]sct %x, %d", __LINE__, sector, count);
        ret = sdio_read(buffer, sector, count, NULL);
        if (ret != 0)
        {
            LOGE("[wordseg]read_data sdio_read 2 failed. ret=%d\n", ret);
            return TC_WSErrID_ReadDataResErr;            
        }
        *dst = buffer + offset % SD_CARD_SECTOR_SIZE;
    }
    else
    {
        unsigned char *buf = os_mem_alloc_ext(count * SD_CARD_SECTOR_SIZE, OS_MEM_ERAM);        
        if (NULL == buf)
        {
            LOGE("[wordseg]my_read_sd_sectors malloc PSRAM %d failed \n", count * SD_CARD_SECTOR_SIZE);
            return TC_WSErrID_ReadDataResErr;
        }                        
        //LOGE("[%d]sct %x, %d", __LINE__, sector, count);
        ret = sdio_read(buf, sector, count, NULL);
        if (ret != 0)
        {
            LOGE("[wordseg]read_data sdio_read 1 failed. ret=%d\n", ret);
            return TC_WSErrID_ReadDataResErr;
        }
        memcpy(*dst, buf + offset % SD_CARD_SECTOR_SIZE, size);        
        os_mem_free(buf);
    }

    return 0;
}

#if READ_DATA_RES_LOG
typedef struct stat_wordseg_info{
    int call_num;   //调用wordseg_do次数
    int cost_time; //ms  分词总耗时
    int token_num; //分词结果token次数累加

    //读data资源耗时
    int read_data_res_time; 
    int read_data_res_num;
    int read_data_res_size;

    //读hash资源耗时
    int read_hash_res_time; 
    int read_hash_res_num;
    int read_hash_res_size;

}t_wordseg_info, p_wordseg_info;
static t_wordseg_info g_info[1] ={0}; 
#endif

static int read_hash_res_func(void *p, int offset, int size, void **dst, int dst_size)
{   
    uint32_t t1 = os_ticks_get();

    // 参数p没用

    // if (NULL == p || NULL == dst || dst_size < size) {
    //	return TC_WSErrID_InvCal;
    // }    
    int ret = my_read_sd_sectors(p, offset, size, dst, dst_size, WS_HASHRES_SD_OFFSET, ws_engine->read_res_buf, ws_engine->read_res_buf_size);
    //printk("read_hash_res_func: p=%p, offset=%d, size=%d, *dst=%p, dst_size=%d, dst_value=%d", p, offset, size, *dst, dst_size, ((int *)(*dst))[0]);

#if READ_DATA_RES_LOG
    uint32_t t2 = os_ticks_get();
    g_info->read_hash_res_num++;
    g_info->read_hash_res_size += size;
    g_info->read_hash_res_time += (t2 - t1);
#endif

    return ret;
}

static int read_data_res_func(void *p, int offset, int size, void **dst, int dst_size)
{   
    uint32_t t1 = os_ticks_get();

    // 参数p没用

    // if (NULL == p || NULL == dst || dst_size < size) {
    //	return TC_WSErrID_InvCal;
    // }
    // printk("read_data_res_func: p=%p, offset=%d, size=%d, *dst=%p, dst_size=%d", p, offset, size, *dst, dst_size);
    int ret = my_read_sd_sectors(p, offset, size, dst, dst_size, WS_DATARES_SD_OFFSET, ws_engine->read_res_buf, ws_engine->read_res_buf_size);
    //print_buf("data.bin", *dst, size);

#if READ_DATA_RES_LOG
    uint32_t t2 = os_ticks_get();
    g_info->read_data_res_num++;
    g_info->read_data_res_size += size;
    g_info->read_data_res_time += (t2 - t1);
#endif

read_data_res_func_end:

    return ret;
}

#define TEST_LAST_WRITE_CRC     (1)
//把分词资源读到预留的sd卡位置，因为分词资源是裸读sd卡，要保障文件存储的连续性
#if COPY_RES_BY_FS
static int read_file_to_sd_sectors(char *file_path, unsigned long long sd_offset, int *file_size)
{     
    int ret = 0;
    int size = 0;   
    int fd = 0;
    char *buf = NULL; 
    int32_t ticks = os_ticks_get();    
	
    // printk("[wordseg]read_file_to_sd_sectors enter. file_path=%s, sd_offset=0x%llx\n", file_path, sd_offset);
    size = low_fs_file_size(file_path);
    if(size <= 0){
		LOGE("[wordseg] read_file_to_sd_sectors get file size failed. file=%s\n", file_path);
        return -1;
    }    

    // printf("[wordseg]low_fs_file_size used time=%d ms, file_path=%s\n", os_ticks_get()-ticks, file_path);

    fd = low_fs_open(file_path, FS_O_READ);
	if (fd < 0) {
		LOGE("[wordseg] read_file_to_sd_sectors open file failed. file=%s\n", file_path);
		return -1;
	} 

    unsigned int buf_size = SD_CARD_SECTOR_SIZE * 16;
    buf = os_mem_alloc_ext(buf_size, OS_MEM_ERAM);
    if(NULL == buf){        
        LOGE("[wordseg]read_file_to_sd_sectors:malloc %d Bytes failed.\n", buf_size);
        ret = -1;
        goto read_file_to_sd_sectors_end;
    }

    /* 读file_path和sd_offset位置信息，如果头部crc值相同，说明文件相同，不需要拷贝
        只有firmware/engine/wordseg/data.bin和hash.bin有改变时才需要拷贝
    */
    char hdr1[24]={0}; //24请参考worseg_res.h中tagHashResHdr/tagDataResHdr结构体开头的file_crc/file_crc_size字段字节数
    low_fs_read(fd, hdr1, sizeof(hdr1));    
    sdio_read(buf, (unsigned long)(sd_offset/SD_CARD_SECTOR_SIZE), 1, NULL);
    if(0 == memcmp(hdr1, buf, sizeof(hdr1)))
    {
        if(NULL != file_size)
        {
            *file_size = size;
        }
        LOGI("[wordseg]read_file_to_sd_sectors no need! size=%d, used time=%d ms\n", size, os_ticks_get()-ticks);
        ret = 0;
        goto read_file_to_sd_sectors_end;
    }
    else
    {
        low_fs_seek(fd, 0, FS_SEEK_SET);
    }
        
    unsigned long sector = (unsigned long)(sd_offset/SD_CARD_SECTOR_SIZE);
    unsigned long sector_bak = sector;    
#if TEST_LAST_WRITE_CRC
    int read_size = SD_CARD_SECTOR_SIZE;
    low_fs_seek(fd, read_size, FS_SEEK_SET);    
    memset(buf, 0xFF, SD_CARD_SECTOR_SIZE);
    if(sector < SD_ALGO_RESOURCE_OFFSET_SECTOR) {
        LOGE("%s, %d, sector addr error, sect:%d",  __FUNCTION__, __LINE__, sector);
        goto read_file_to_sd_sectors_end;
    }
    ret = sdio_write(buf, sector, 1, NULL); //文件头部有crc，防止下面文件写出错，头部的信息先写点0XFF，最后再写真实值
    if(ret != 0)
    {
        LOGE("[wordseg]read_file_to_sd_sectors:sdio_write head_sector 0XFF failed. sector=%d, count=%d\n",sector, 1);
        goto read_file_to_sd_sectors_end;
    }
    sector += 1;
#else
    int read_size = 0;
#endif
    while(read_size < size)
    {
        int once_size = (size - read_size > buf_size) ? buf_size : (size - read_size);

        low_fs_read(fd, buf, once_size);
        if(sector < SD_ALGO_RESOURCE_OFFSET_SECTOR) {
            LOGE("%s, %d, sector addr error, sect:%d",  __FUNCTION__, __LINE__, sector);
            goto read_file_to_sd_sectors_end;
        }
        int ret = sdio_write(buf, sector, buf_size/SD_CARD_SECTOR_SIZE, NULL); //最后一次读的size可能小于buf_size,无影响
        if(ret != 0)
        {
            LOGE("[wordseg]read_file_to_sd_sectors:sdio_write failed. sector=%d, count=%d\n",sector, buf_size/SD_CARD_SECTOR_SIZE);
            goto read_file_to_sd_sectors_end;
        }

        sector += buf_size/SD_CARD_SECTOR_SIZE;
        read_size += once_size;
    }

#if TEST_LAST_WRITE_CRC    
    low_fs_seek(fd, 0, FS_SEEK_SET);    
    low_fs_read(fd, buf, SD_CARD_SECTOR_SIZE);
    if(sector_bak < SD_ALGO_RESOURCE_OFFSET_SECTOR) {
        LOGE("%s, %d, sector addr error, sect:%d",  __FUNCTION__, __LINE__, sector_bak);
        return -1;
    }
    ret = sdio_write(buf, sector_bak, 1, NULL); //文件头部有crc，防止下面文件写出错，头部的信息先写点0XFF，最后再写真实值
    if(ret != 0)
    {
        LOGE("[wordseg]read_file_to_sd_sectors:sdio_write head_secotr failed. sector=%d, count=%d\n",sector, 1);
        goto read_file_to_sd_sectors_end;
    }    
#endif

    if(file_size != NULL){
        *file_size = size;       
    }
    printk("[wordseg]read_file_to_sd_sectors exit. size=%d, write_sector=%u, used time=%d ms\n", size, sector_bak, os_ticks_get()-ticks);

    ret = 0;

read_file_to_sd_sectors_end:
    if(buf != NULL){
        os_mem_free(buf);
    }

    if(fd > 0){
        low_fs_close(fd);
    }

    return ret;
}
#endif

#if OFFLINE_TEST_WORDSEG

#include <time.h>
static int random_int(int min, int max) {
    srand(time(NULL)); // 设置种子
    return (rand() % (max - min + 1)) + min;
}

//简单测试裸读TF卡接口速度
static void test_tf()
{
    int read_size = 128*1024;
    // char *buf = os_mem_alloc_ext(256*SD_CARD_SECTOR_SIZE, OS_MEM_ERAM);    
    // int count_lst[6] ={1, 2, 3, 4, 64,128};
    char *buf = os_mem_alloc_ext(64*SD_CARD_SECTOR_SIZE, OS_MEM_ERAM);    
    if(buf == NULL){
        LOGD("test_tf failed. malloc %d IRAM failed./n", 32*SD_CARD_SECTOR_SIZE);
        return;
    }

    int count_lst[6] ={1, 4, 8, 16, 32, 64};
    int cost_time_lst[6] = {0};
    int read_cnt[6] = {0};

    srand((unsigned)time(NULL)); // 设置种子

    for(int i=0; i<sizeof(count_lst)/sizeof(count_lst[0]); i++)
    {
        int count = count_lst[i];
        int read_once_size = SD_CARD_SECTOR_SIZE * count;
        int cost_time = 0;
        int sector = rand() % 6200000;
        LOGD("rand_sector:%d\n", sector);
        int t1 = os_ticks_get();
        for(int j=0; j<read_size/read_once_size; j++)
        {
            //LOGE("[%d]sct %x, %d", __LINE__, sector+j+100, count);
             sdio_read(buf, sector+j+100, count, NULL);
             read_cnt[i]++;       
        }
        cost_time_lst[i] = os_ticks_get() - t1;;
    }
    LOGD("\nsector | count | read_cnt | size(KB) | rd_time(ms) | rd_speed(MB/s)\n");
    for(int i=0; i<sizeof(count_lst)/sizeof(count_lst[0]); i++)
    {        
        LOGD("%-6d | %-5d | %-8d | %-8d | %-11d | %.2f\n", SD_CARD_SECTOR_SIZE, count_lst[i], read_cnt[i], read_size/1024, cost_time_lst[i], 1000.0/cost_time_lst[i]*read_size/1024.0/1024.0);
    }
    LOGD("-----------------------------------------------------\n");    
    os_mem_free(buf);    
}

// 测试分词性能入口函数
int wordseg_offline_test_entry()
{
    int ret = 0;
    
    LOGD("---------------wordseg_offline_test_entry enter------------------\n");

    int32_t tick = os_ticks_get();    

    test_tf(); //测试TF卡读速度

    ret = wordseg_init_entry();
    if (0 != ret)
    {
        LOGE("wordseg_init_entry err. ret=%d\n", ret);
        return ret;
    }

#if 0 // 测试相似单词推荐接口
    char text[][40] = {"loook", "1st", "Additionathy", "Bridger", "good"};
    for (int ii = 0; ii < sizeof(text) / sizeof(text[0]); ii++)
    {
        int32_t t1 = os_ticks_get();
        PWordCorrRst word_rst = NULL;        
        int ret = wordseg_wordcorrection(ws_engine->resident_buf, text[ii], &word_rst);
        printk("[wordseg_wordcorrection] text=%s, rst=%s %s, use time=%d ms\n", text[ii], word_rst->ppWord[0], word_rst->ppWord[1], os_ticks_get() - t1);
    }

#endif

    /* 耗时测试结果备份 arcs(cp) 20250122
    [test wordseg_get_tts_text 001] text_len=15, cost_time=6 ms 
    [test wordseg_do 001] wordseg_do次数=1, 文本长度=15, 读data次数=5, 读hash次数=14, 读data字节数=104, 读hash字节数=56, 总耗时=4, 读data耗时=0, 读hash耗时=3, 运算耗时=1, 文本=客路青山外, token_num=3,
    [xxxtest wordseg_do 001]        4	0	3	1	6
    [test wordseg_tree 001] wordseg_do次数=8, 文本长度=15, 读data次数=14, 读hash次数=31, 读data字节数=544, 读hash字节数=124, 总耗时=10, 读data耗时=1, 读hash耗时=6, 运算耗时=3, 文本=客路青山外, node_num=8, rst_size=292
    [xxxtest wordseg_tree 001]        10	1	6	3
    [test wordseg_get_tts_text 002] text_len=36, cost_time=24 ms 
    [test wordseg_do 002] wordseg_do次数=1, 文本长度=36, 读data次数=7, 读hash次数=15, 读data字节数=168, 读hash字节数=60, 总耗时=4, 读data耗时=1, 读hash耗时=2, 运算耗时=1, 文本=客路青山外，行舟绿水前。, token_num=4,
    [xxxtest wordseg_do 002]        4	1	2	1	24
    [test wordseg_tree 002] wordseg_do次数=18, 文本长度=36, 读data次数=32, 读hash次数=73, 读data字节数=968, 读hash字节数=292, 总耗时=22, 读data耗时=5, 读hash耗时=15, 运算耗时=2, 文本=客路青山外，行舟绿水前。, node_num=18, rst_size=668
    [xxxtest wordseg_tree 002]        22	5	15	2
    [test wordseg_get_tts_text 003] text_len=51, cost_time=43 ms 
    [test wordseg_do 003] wordseg_do次数=1, 文本长度=51, 读data次数=10, 读hash次数=20, 读data字节数=272, 读hash字节数=80, 总耗时=5, 读data耗时=2, 读hash耗时=3, 运算耗时=0, 文本=客路青山外，行舟绿水前。潮平两岸阔, token_num=5,
    // [xxxtest wordseg_do 003]        5	2	3	0	43
    [test wordseg_tree 003] wordseg_do次数=26, 文本长度=51, 读data次数=43, 读hash次数=107, 读data字节数=1376, 读hash字节数=428, 总耗时=30, 读data耗时=6, 读hash耗时=19, 运算耗时=5, 文本=客路青山外，行舟绿水前。潮平两岸阔, node_num=26, rst_size=956
    [xxxtest wordseg_tree 003]        30	6	19	5
    [test wordseg_get_tts_text 004] text_len=108, cost_time=104 ms 
    [test wordseg_do 004] wordseg_do次数=1, 文本长度=108, 读data次数=21, 读hash次数=47, 读data字节数=568, 读hash字节数=188, 总耗时=12, 读data耗时=1, 读hash耗时=8, 运算耗时=3, 文本=客路青山外，行舟绿水前。潮平两岸阔，风正一帆悬。海日生残夜，江春入旧年。, token_num=12,
    [xxxtest wordseg_do 004]        12	1	8	3	104
    [test wordseg_tree 004] wordseg_do次数=49, 文本长度=108, 读data次数=86, 读hash次数=217, 读data字节数=2480, 读hash字节数=868, 总耗时=58, 读data耗时=11, 读hash耗时=33, 运算耗时=14, 文本=客路青山外，行舟绿水前。潮平两岸阔，风正一帆悬。海日生残夜，江春入旧年。, node_num=49, rst_size=1792
    [xxxtest wordseg_tree 004]        58	11	33	14
    [test wordseg_get_tts_text 005] text_len=183, cost_time=207 ms 
    [test wordseg_do 005] wordseg_do次数=1, 文本长度=183, 读data次数=149, 读hash次数=314, 读data字节数=3616, 读hash字节数=1256, 总耗时=81, 读data耗时=19, 读hash耗时=46, 运算耗时=16, 文本=阅读的时候，既读进去，又想开去，不仅可以深化对课文思想内容的理解，而且可以活跃思想，激发创造力。我们应该在这方面多下功夫。, token_num=38,
    [xxxtest wordseg_do 005]        81	19	46	16	207
    [test wordseg_tree 005] wordseg_do次数=85, 文本长度=183, 读data次数=217, 读hash次数=533, 读data字节数=6112, 读hash字节数=2132, 总耗时=140, 读data耗时=40, 读hash耗时=66, 运算耗时=34, 文本=阅读的时候，既读进去，又想开去，不仅可以深化对课文思想内容的理解，而且可以活跃思想，激发创造力。我们应该在这方面多下功夫。, node_num=85, rst_size=2916
    [xxxtest wordseg_tree 005]        140	40	66	34
    [test wordseg_get_tts_text 006] text_len=306, cost_time=379 ms 
    [test wordseg_do 006] wordseg_do次数=1, 文本长度=306, 读data次数=398, 读hash次数=768, 读data字节数=9912, 读hash字节数=3072, 总耗时=206, 读data耗时=51, 读hash耗时=109, 运算耗时=46, 文本=会场在天安门广场。广场成丁字形。丁字形一横的北面是一道河，河上并排架着五座白石桥；再北面是城墙，城墙中央高高耸起天安门的城楼。丁字形的一竖向南直伸到中华门。在一横一竖的交点的南面，场中挺立着一根电动旗杆。, token_num=72,
    [xxxtest wordseg_do 006]        206	51	109	46	379
    [test wordseg_tree 006] wordseg_do次数=131, 文本长度=306, 读data次数=509, 读hash次数=1066, 读data字节数=14208, 读hash字节数=4264, 总耗时=290, 读data耗时=75, 读hash耗时=146, 运算耗时=69, 文本=会场在天安门广场。广场成丁字形。丁字形一横的北面是一道河，河上并排架着五座白石桥；再北面是城墙，城墙中央高高耸起天安门的城楼。丁字形的一竖向南直伸到中华门。在一横一竖的交点的南面，场中挺立着一根电动旗杆。, node_num=131, rst_size=4508
    [xxxtest wordseg_tree 006]        290	75	146	69
    [test wordseg_get_tts_text 007] text_len=451, cost_time=430 ms 
    [test wordseg_do 007] wordseg_do次数=1, 文本长度=451, 读data次数=261, 读hash次数=533, 读data字节数=6776, 读hash字节数=2132, 总耗时=144, 读data耗时=37, 读hash耗时=76, 运算耗时=31, 文本=今天全省大部地区晴好依旧，上午10点气温来看，大部地区在20℃上下，较昨日同时又有2-4℃的升幅，而在下午13时10分左右，湖州最高温则达到了26.9 ℃。可惜这舒适的阳光将暂时退场，连晴了数日却依旧有所不舍。明天下午起，湖州多云转阴，还可能有阵雨的降临。春天天气向来如此多变，未来五天我省晴雨转换频繁，气温起伏变化较大。, token_num=105,
    [xxxtest wordseg_do 007]        144	37	76	31	430
    [test wordseg_tree 007] wordseg_do次数=200, 文本长度=451, 读data次数=447, 读hash次数=1024, 读data字节数=13112, 读hash字节数=4096, 总耗时=280, 读data耗时=70, 读hash耗时=143, 运算耗时=67, 文本=今天全省大部地区晴好依旧，上午10点气温来看，大部地区在20℃上下，较昨日同时又有2-4℃的升幅，而在下午13时10分左右，湖州最高温则达到了26.9 ℃。可惜这舒适的阳光将暂时退场，连晴了数日却依旧有所不舍。明天下午起，湖州多云转阴，还可能有阵雨的降临。春天天气向来如此多变，未来五天我省晴雨转换频繁，气温起伏变化较大。, node_num=200, rst_size=6792
    [xxxtest wordseg_tree 007]        280	70	143	67
    [test wordseg_get_tts_text 008] text_len=382, cost_time=527 ms 
    [test wordseg_do 008] wordseg_do次数=1, 文本长度=382, 读data次数=892, 读hash次数=1774, 读data字节数=21272, 读hash字节数=7096, 总耗时=470, 读data耗时=137, 读hash耗时=251, 运算耗时=82, 文本=闶竳憞逅頜苬夐擞苬襢苬讚髣絽屗憬剓产鎶檤磴予淥颍虄局烠泸旯涖鄇旮旯胓夤旮旯胓薈僶旮旯酽頕鸰頜鼂颍鸮旯膱犴鏹鎧驾郐旯屗~载媒旯君厮旯苬恿播爿襢姦弰旯襢峞鸮旯竳躈旯槠軵旯蹋軵襢胓竳睮頠軵局頠覎釲篣絨攪櫜劵讚釲旯釲旯辸旯頜諢莬烥頜灻芛舝窙炗邌筪譡鈛弨蒦舝丷潁涖廜, token_num=125,
    [xxxtest wordseg_do 008]        470	137	251	82	527
    [test wordseg_tree 008] wordseg_do次数=132, 文本长度=382, 读data次数=996, 读hash次数=1923, 读data字节数=24408, 读hash字节数=7692, 总耗时=524, 读data耗时=140, 读hash耗时=286, 运算耗时=98, 文本=闶竳憞逅頜苬夐擞苬襢苬讚髣絽屗憬剓产鎶檤磴予淥颍虄局烠泸旯涖鄇旮旯胓夤旮旯胓薈僶旮旯酽頕鸰頜鼂颍鸮旯膱犴鏹鎧驾郐旯屗~载媒旯君厮旯苬恿播爿襢姦弰旯襢峞鸮旯竳躈旯槠軵旯蹋軵襢胓竳睮頠軵局頠覎釲篣絨攪櫜劵讚釲旯釲旯辸旯頜諢莬烥頜灻芛舝窙炗邌筪譡鈛弨蒦舝丷潁涖廜, node_num=132, rst_size=4492
    [xxxtest wordseg_tree 008]        524	140	286	98
    [test wordseg_get_tts_text 009] text_len=68, cost_time=11 ms 
    [test wordseg_do 009] wordseg_do次数=1, 文本长度=68, 读data次数=42, 读hash次数=93, 读data字节数=904, 读hash字节数=372, 总耗时=25, 读data耗时=7, 读hash耗时=8, 运算耗时=10, 文本=Whether or not you can do this well depends on your learning habits., token_num=22,
    [xxxtest wordseg_do 009]        25	7	8	10	11
    [test wordseg_tree 009] wordseg_do次数=29, 文本长度=68, 读data次数=54, 读hash次数=132, 读data字节数=1184, 读hash字节数=528, 总耗时=38, 读data耗时=4, 读hash耗时=22, 运算耗时=12, 文本=Whether or not you can do this well depends on your learning habits., node_num=29, rst_size=968
    [xxxtest wordseg_tree 009]        38	4	22	12
    [test wordseg_get_tts_text 010] text_len=79, cost_time=29 ms 
    [test wordseg_do 010] wordseg_do次数=1, 文本长度=79, 读data次数=17, 读hash次数=33, 读data字节数=432, 读hash字节数=132, 总耗时=9, 读data耗时=3, 读hash耗时=2, 运算耗时=4, 文本=朝阳 凿壁偷光 登金陵凤凰台 摄提贞于孟陬兮,惟庚寅吾以降, token_num=9,
    [xxxtest wordseg_do 010]        9	3	2	4	29
    [test wordseg_tree 010] wordseg_do次数=40, 文本长度=79, 读data次数=77, 读hash次数=177, 读data字节数=2144, 读hash字节数=708, 总耗时=49, 读data耗时=9, 读hash耗时=21, 运算耗时=19, 文本=朝阳 凿壁偷光 登金陵凤凰台 摄提贞于孟陬兮,惟庚寅吾以降, node_num=40, rst_size=1436
    [xxxtest wordseg_tree 010]        49	9	21	19
    [test wordseg_get_tts_text 011] text_len=502, cost_time=99 ms 
    [test wordseg_do 011] wordseg_do次数=1, 文本长度=502, 读data次数=314, 读hash次数=672, 读data字节数=7784, 读hash字节数=2688, 总耗时=254, 读data耗时=51, 读hash耗时=79, 运算耗时=124, 文本=for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his not, token_num=163,
    [xxxtest wordseg_do 011]        254	51	79	124	99
    [test wordseg_tree 011] wordseg_do次数=197, 文本长度=502, 读data次数=369, 读hash次数=890, 读data字节数=9456, 读hash字节数=3560, 总耗时=322, 读data耗时=50, 读hash耗时=112, 运算耗时=160, 文本=for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his not, node_num=197, rst_size=6720
    [xxxtest wordseg_tree 011]        322	50	112	160
    [test wordseg_get_tts_text 012] text_len=324, cost_time=53 ms 
    [test wordseg_do 012] wordseg_do次数=1, 文本长度=324, 读data次数=169, 读hash次数=393, 读data字节数=3968, 读hash字节数=1572, 总耗时=140, 读data耗时=29, 读hash耗时=44, 运算耗时=67, 文本=New Zealand has its own government, but it is also part of the British Commonwealth, and therefore the official head of state is Elizabeth Ⅱ, the Queen of England, Scotland and Wales, New Zealand was the first country in the world to give the vote to women in 1893, to have old age pensions and the eight-hour working day., token_num=98,
    [xxxtest wordseg_do 012]        140	29	44	67	53
    [test wordseg_tree 012] wordseg_do次数=137, 文本长度=324, 读data次数=226, 读hash次数=593, 读data字节数=6008, 读hash字节数=2372, 总耗时=200, 读data耗时=35, 读hash耗时=76, 运算耗时=89, 文本=New Zealand has its own government, but it is also part of the British Commonwealth, and therefore the official head of state is Elizabeth Ⅱ, the Queen of England, Scotland and Wales, New Zealand was the first country in the world to give the vote to women in 1893, to have old age pensions and the eight-hour working day., node_num=137, rst_size=4600
    [xxxtest wordseg_tree 012]        200	35	76	89
    [test wordseg_get_tts_text 013] text_len=467, cost_time=85 ms 
    [test wordseg_do 013] wordseg_do次数=1, 文本长度=467, 读data次数=286, 读hash次数=615, 读data字节数=6944, 读hash字节数=2460, 总耗时=240, 读data耗时=40, 读hash耗时=82, 运算耗时=118, 文本=It is the fastest and the second cheapest, but you may have to wait for hours at the airport because of bad weather.By the time California elected to become the thirty-first federal state of the USA in 1850, it was already a multicultural society LATER ARRIVALS Although Chinese immigrants began to arrive during the Gold Rush Period, it was the building of the rail network from the west to the east coast that brought even larger numbers to California in the 1860s., token_num=135,
    [xxxtest wordseg_do 013]        240	40	82	118	85
    [test wordseg_tree 013] wordseg_do次数=183, 文本长度=467, 读data次数=355, 读hash次数=883, 读data字节数=8984, 读hash字节数=3532, 总耗时=316, 读data耗时=43, 读hash耗时=128, 运算耗时=145, 文本=It is the fastest and the second cheapest, but you may have to wait for hours at the airport because of bad weather.By the time California elected to become the thirty-first federal state of the USA in 1850, it was already a multicultural society LATER ARRIVALS Although Chinese immigrants began to arrive during the Gold Rush Period, it was the building of the rail network from the west to the east coast that brought even larger numbers to California in the 1860s., node_num=183, rst_size=6188
    [xxxtest wordseg_tree 013]        316	43	128	145
    [test wordseg_get_tts_text 014] text_len=259, cost_time=41 ms 
    [test wordseg_do 014] wordseg_do次数=1, 文本长度=259, 读data次数=156, 读hash次数=370, 读data字节数=3872, 读hash字节数=1480, 总耗时=115, 读data耗时=21, 读hash耗时=48, 运算耗时=46, 文本=Unfortunately for him, the water he sent over his balcony every day ended up on the McKay's, or too often, on the McKays themselves " For the last fortnight, since Smith moved into the flat above us, we have no hardly dared go onto our balcony, "said Laurene., token_num=93,
    [xxxtest wordseg_do 014]        115	21	48	46	41
    [test wordseg_tree 014] wordseg_do次数=109, 文本长度=259, 读data次数=193, 读hash次数=501, 读data字节数=5096, 读hash字节数=2004, 总耗时=155, 读data耗时=23, 读hash耗时=69, 运算耗时=63, 文本=Unfortunately for him, the water he sent over his balcony every day ended up on the McKay's, or too often, on the McKays themselves " For the last fortnight, since Smith moved into the flat above us, we have no hardly dared go onto our balcony, "said Laurene., node_num=109, rst_size=3548
    [xxxtest wordseg_tree 014]        155	23	69	63
    [test wordseg_get_tts_text 015] text_len=255, cost_time=43 ms 
    [test wordseg_do 015] wordseg_do次数=1, 文本长度=255, 读data次数=193, 读hash次数=444, 读data字节数=4512, 读hash字节数=1776, 总耗时=135, 读data耗时=29, 读hash耗时=60, 运算耗时=46, 文本=and on the slopes at the foot of the hills there were houses with rich gardens and an open parkland with groves of trees and the white gleam of a classical templ Just beside him was that bare patch in the air as hard Oxford,hi whatever this new world was,, token_num=88,
    [xxxtest wordseg_do 015]        135	29	60	46	43
    [test wordseg_tree 015] wordseg_do次数=108, 文本长度=255, 读data次数=230, 读hash次数=588, 读data字节数=5520, 读hash字节数=2352, 总耗时=175, 读data耗时=36, 读hash耗时=76, 运算耗时=63, 文本=and on the slopes at the foot of the hills there were houses with rich gardens and an open parkland with groves of trees and the white gleam of a classical templ Just beside him was that bare patch in the air as hard Oxford,hi whatever this new world was,, node_num=108, rst_size=3528
    [xxxtest wordseg_tree 015]        175	36	76	63
    [test wordseg_get_tts_text 016] text_len=211, cost_time=32 ms 
    [test wordseg_do 016] wordseg_do次数=1, 文本长度=211, 读data次数=122, 读hash次数=289, 读data字节数=3304, 读hash字节数=1156, 总耗时=86, 读data耗时=16, 读hash耗时=39, 运算耗时=31, 文本=Nowadays, within a short walk along a busy street, you are likely to find a chain store of some kind-a fast-food restaurant, a bakery or a convenience store Chain stores have become part of people's daily lives., token_num=63,
    [xxxtest wordseg_do 016]        86	16	39	31	32
    [test wordseg_tree 016] wordseg_do次数=85, 文本长度=211, 读data次数=159, 读hash次数=405, 读data字节数=4752, 读hash字节数=1620, 总耗时=123, 读data耗时=22, 读hash耗时=53, 运算耗时=48, 文本=Nowadays, within a short walk along a busy street, you are likely to find a chain store of some kind-a fast-food restaurant, a bakery or a convenience store Chain stores have become part of people's daily lives., node_num=85, rst_size=2920
    [xxxtest wordseg_tree 016]        123	22	53	48
    [test wordseg_get_tts_text 017] text_len=220, cost_time=36 ms 
    [test wordseg_do 017] wordseg_do次数=1, 文本长度=220, 读data次数=183, 读hash次数=409, 读data字节数=4832, 读hash字节数=1636, 总耗时=120, 读data耗时=31, 读hash耗时=52, 运算耗时=37, 文本=When she brought the too to my room , she told me it was a holiday in the USA-it's Independence Day , when the Internationd Student Certr-that's centre where I come from I met lots of young people from at over the werld., token_num=76,
    [xxxtest wordseg_do 017]        120	31	52	37	36
    [test wordseg_tree 017] wordseg_do次数=92, 文本长度=220, 读data次数=214, 读hash次数=526, 读data字节数=5784, 读hash字节数=2104, 总耗时=154, 读data耗时=32, 读hash耗时=63, 运算耗时=59, 文本=When she brought the too to my room , she told me it was a holiday in the USA-it's Independence Day , when the Internationd Student Certr-that's centre where I come from I met lots of young people from at over the werld., node_num=92, rst_size=3028
    [xxxtest wordseg_tree 017]        154	32	63	59
    [test wordseg_get_tts_text 018] text_len=509, cost_time=48 ms 
    [test wordseg_do 018] wordseg_do次数=1, 文本长度=509, 读data次数=63, 读hash次数=276, 读data字节数=2256, 读hash字节数=1104, 总耗时=100, 读data耗时=8, 读hash耗时=35, 运算耗时=57, 文本=abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdef ghij abcdefghi jabcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jefghi jabcdefghij abcdefghijabcdefghi jabcdefghijabcdefghijabcdefghidefghij abcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdei jabcdefghij abcdefghi jabcdefghijabcdefghi, token_num=83,
    [xxxtest wordseg_do 018]        100	8	35	57	48
    [test wordseg_tree 018] wordseg_do次数=84, 文本长度=509, 读data次数=67, 读hash次数=321, 读data字节数=2416, 读hash字节数=1284, 总耗时=117, 读data耗时=11, 读hash耗时=34, 运算耗时=72, 文本=abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdef ghij abcdefghi jabcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jefghi jabcdefghij abcdefghijabcdefghi jabcdefghijabcdefghijabcdefghidefghij abcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdei jabcdefghij abcdefghi jabcdefghijabcdefghi, node_num=84, rst_size=3908
    [xxxtest wordseg_tree 018]        117	11	34	72
    [test wordseg_get_tts_text 019] text_len=48, cost_time=41 ms 
    [test wordseg_do 019] wordseg_do次数=1, 文本长度=48, 读data次数=11, 读hash次数=24, 读data字节数=296, 读hash字节数=96, 总耗时=7, 读data耗时=2, 读hash耗时=3, 运算耗时=2, 文本=测试分词，白日依山尽，黄河入海流, token_num=6,
    [xxxtest wordseg_do 019]        7	2	3	2	41
    [test wordseg_tree 019] wordseg_do次数=24, 文本长度=48, 读data次数=38, 读hash次数=100, 读data字节数=1016, 读hash字节数=400, 总耗时=28, 读data耗时=3, 读hash耗时=15, 运算耗时=10, 文本=测试分词，白日依山尽，黄河入海流, node_num=24, rst_size=864
    [xxxtest wordseg_tree 019]        28	3	15	10
    [test wordseg_get_tts_text 020] text_len=113, cost_time=76 ms 
    [test wordseg_do 020] wordseg_do次数=1, 文本长度=113, 读data次数=64, 读hash次数=139, 读data字节数=1632, 读hash字节数=556, 总耗时=34, 读data耗时=7, 读hash耗时=19, 运算耗时=8, 文本=慈母手中线游子身上衣临行密密缝,意恐②迟迟归③。谁④}言⑤寸草心报得三春晖③。, token_num=9,
    [xxxtest wordseg_do 020]        34	7	19	8	76
    [test wordseg_tree 020] wordseg_do次数=52, 文本长度=113, 读data次数=161, 读hash次数=373, 读data字节数=4584, 读hash字节数=1492, 总耗时=98, 读data耗时=20, 读hash耗时=55, 运算耗时=23, 文本=慈母手中线游子身上衣临行密密缝,意恐②迟迟归③。谁④}言⑤寸草心报得三春晖③。, node_num=51, rst_size=1860
    [xxxtest wordseg_tree 020]        98	20	55	23
    [test wordseg_get_tts_text 021] text_len=288, cost_time=302 ms 
    [test wordseg_do 021] wordseg_do次数=1, 文本长度=288, 读data次数=293, 读hash次数=599, 读data字节数=7104, 读hash字节数=2396, 总耗时=157, 读data耗时=40, 读hash耗时=88, 运算耗时=29, 文本=贼又冷又饿，正在下树，抬头看见走来一个黑乎乎的东西，心想：“‘漏’又来了，这下我可活不成了！”他赶忙往树梢上爬，总嫌离地太近，紧爬慢爬，咔嚓一声，树枝断了，一个倒栽葱摔了下来，顺着山坡往下滚。, token_num=74,
    [xxxtest wordseg_do 021]        157	40	88	29	302
    [test wordseg_tree 021] wordseg_do次数=117, 文本长度=288, 读data次数=385, 读hash次数=839, 读data字节数=10040, 读hash字节数=3356, 总耗时=225, 读data耗时=50, 读hash耗时=118, 运算耗时=57, 文本=贼又冷又饿，正在下树，抬头看见走来一个黑乎乎的东西，心想：“‘漏’又来了，这下我可活不成了！”他赶忙往树梢上爬，总嫌离地太近，紧爬慢爬，咔嚓一声，树枝断了，一个倒栽葱摔了下来，顺着山坡往下滚。, node_num=117, rst_size=4004
    [xxxtest wordseg_tree 021]        225	50	118	57
    [test wordseg_get_tts_text 022] text_len=259, cost_time=285 ms 
    [test wordseg_do 022] wordseg_do次数=1, 文本长度=259, 读data次数=271, 读hash次数=548, 读data字节数=6368, 读hash字节数=2192, 总耗时=145, 读data耗时=36, 读hash耗时=78, 运算耗时=31, 文本=假如你所从事的工作，是你的爱好，这七万个小时，将是怎样快活和充满创意的时光!假如你不喜欢它，漫长的七万个小时，足以让花容磨损，日月无光，每一天都如同穿着淋湿的衬衣，针芒在身。, token_num=61,
    [xxxtest wordseg_do 022]        145	36	78	31	285
    [test wordseg_tree 022] wordseg_do次数=113, 文本长度=259, 读data次数=372, 读hash次数=812, 读data字节数=9544, 读hash字节数=3248, 总耗时=219, 读data耗时=47, 读hash耗时=111, 运算耗时=61, 文本=假如你所从事的工作，是你的爱好，这七万个小时，将是怎样快活和充满创意的时光!假如你不喜欢它，漫长的七万个小时，足以让花容磨损，日月无光，每一天都如同穿着淋湿的衬衣，针芒在身。, node_num=113, rst_size=3860
    [xxxtest wordseg_tree 022]        219	47	111	61
    [test wordseg_get_tts_text 023] text_len=276, cost_time=286 ms 
    [test wordseg_do 023] wordseg_do次数=1, 文本长度=276, 读data次数=245, 读hash次数=497, 读data字节数=6128, 读hash字节数=1988, 总耗时=130, 读data耗时=38, 读hash耗时=72, 运算耗时=20, 文本=上路后，车夫说我们用饭之际，所有的游客都已赶到，甚至还抢在了我们前面；“但是，”他把握十足地说，“不必为此烦恼——静下心来——不要浮躁——他们虽已扬尘远去，可不久就会消失在我们身后的。, token_num=67,
    [xxxtest wordseg_do 023]        130	38	72	20	286
    [test wordseg_tree 023] wordseg_do次数=118, 文本长度=276, 读data次数=342, 读hash次数=764, 读data字节数=9352, 读hash字节数=3056, 总耗时=205, 读data耗时=48, 读hash耗时=99, 运算耗时=58, 文本=上路后，车夫说我们用饭之际，所有的游客都已赶到，甚至还抢在了我们前面；“但是，”他把握十足地说，“不必为此烦恼——静下心来——不要浮躁——他们虽已扬尘远去，可不久就会消失在我们身后的。, node_num=118, rst_size=4024
    [xxxtest wordseg_tree 023]        205	48	99	58
    [test wordseg_get_tts_text 024] text_len=486, cost_time=92 ms 
    [test wordseg_do 024] wordseg_do次数=1, 文本长度=486, 读data次数=532, 读hash次数=1376, 读data字节数=13600, 读hash字节数=5504, 总耗时=400, 读data耗时=72, 读hash耗时=162, 运算耗时=166, 文本=giraffe...帅...ah h ba3b ahi ba3l ele ble om1mt016res.o......::sh902bo be.t at t tt+t D1e1.S wa wa A.::....::??2she::::::..9..:e3e?./P U/..::.1..D8G200.7.....200212.33.:::..12,345.13131.2.215.12:5.151.24-1.34.::::20221.::...02::.20..n0o0.13:0....24:59:5924:524:5.::90:....:...0:..:...1.::....22千米22千米....123434332....1234123123..012013:..:::藏::..词语:词语:.....:吾.....凿壁偷光满面春风.首学:长:...:31:..移::古请四::占词....::《入返出av:家工古...., token_num=259,
    [xxxtest wordseg_do 024]        400	72	162	166	92
    [test wordseg_tree 024] wordseg_do次数=294, 文本长度=486, 读data次数=1340, 读hash次数=3191, 读data字节数=34960, 读hash字节数=12764, 总耗时=941, 读data耗时=186, 读hash耗时=395, 运算耗时=360, 文本=giraffe...帅...ah h ba3b ahi ba3l ele ble om1mt016res.o......::sh902bo be.t at t tt+t D1e1.S wa wa A.::....::??2she::::::..9..:e3e?./P U/..::.1..D8G200.7.....200212.33.:::..12,345.13131.2.215.12:5.151.24-1.34.::::20221.::...02::.20..n0o0.13:0....24:59:5924:524:5.::90:....:...0:..:...1.::....22千米22千米....123434332....1234123123..012013:..:::藏::..词语:词语:.....:吾.....凿壁偷光满面春风.首学:长:...:31:..移::古请四::占词....::《入返出av:家工古...., node_num=293, rst_size=8548
    [xxxtest wordseg_tree 024]        941	186	395	360
    [test wordseg_get_tts_text 025] text_len=213, cost_time=247 ms 
    [test wordseg_do 025] wordseg_do次数=1, 文本长度=213, 读data次数=232, 读hash次数=451, 读data字节数=5720, 读hash字节数=1804, 总耗时=119, 读data耗时=38, 读hash耗时=61, 运算耗时=20, 文本=让那些看不起民众、贱视民众、顽固的倒退的人们去赞美那贵族化的楠木（那也是直挺秀颀的），去鄙视这极常见、极易生长的白杨树吧，我要高声赞美白杨树！, token_num=48,
    [xxxtest wordseg_do 025]        119	38	61	20	247
    [test wordseg_tree 025] wordseg_do次数=95, 文本长度=213, 读data次数=316, 读hash次数=678, 读data字节数=9088, 读hash字节数=2712, 总耗时=183, 读data耗时=46, 读hash耗时=104, 运算耗时=33, 文本=让那些看不起民众、贱视民众、顽固的倒退的人们去赞美那贵族化的楠木（那也是直挺秀颀的），去鄙视这极常见、极易生长的白杨树吧，我要高声赞美白杨树！, node_num=95, rst_size=3260
    [xxxtest wordseg_tree 025]        183	46	104	33
    [test wordseg_get_tts_text 026] text_len=306, cost_time=369 ms 
    [test wordseg_do 026] wordseg_do次数=1, 文本长度=306, 读data次数=340, 读hash次数=672, 读data字节数=8216, 读hash字节数=2688, 总耗时=178, 读data耗时=55, 读hash耗时=90, 运算耗时=33, 文本=这只鹦鹉对我母亲真是一往情深，它热烈地追求她：在她的身边用各种古怪的姿势跳舞，一下子把它漂亮的冠毛打开来，一下子又合上；而且无论她到哪儿去，它都跟着；如果她不在，它一定像初来时找我一样，孜孜不倦地去找她。, token_num=70,
    [xxxtest wordseg_do 026]        178	55	90	33	369
    [test wordseg_tree 026] wordseg_do次数=133, 文本长度=306, 读data次数=458, 读hash次数=986, 读data字节数=12400, 读hash字节数=3944, 总耗时=267, 读data耗时=54, 读hash耗时=147, 运算耗时=66, 文本=这只鹦鹉对我母亲真是一往情深，它热烈地追求她：在她的身边用各种古怪的姿势跳舞，一下子把它漂亮的冠毛打开来，一下子又合上；而且无论她到哪儿去，它都跟着；如果她不在，它一定像初来时找我一样，孜孜不倦地去找她。, node_num=133, rst_size=4572
    [xxxtest wordseg_tree 026]        267	54	147	66
    [test wordseg_get_tts_text 027] text_len=284, cost_time=121 ms 
    [test wordseg_do 027] wordseg_do次数=1, 文本长度=284, 读data次数=192, 读hash次数=480, 读data字节数=4872, 读hash字节数=1920, 总耗时=114, 读data耗时=25, 读hash耗时=66, 运算耗时=23, 文本=秦王^{②}使人谓安陵君^{③}曰:“寡人欲以五百里之地易^{④}安陵,安陵君其^{⑤}许寡人!”安陵君曰:“大王加惠^{⑥},以大易小,甚善;虽然,受地于先王,愿终守之,弗敢易!”秦王不说。安陵君因使唐雎使于秦。秦王谓唐昨, token_num=34,
    [xxxtest wordseg_do 027]        114	25	66	23	121
    [test wordseg_tree 027] wordseg_do次数=128, 文本长度=284, 读data次数=439, 读hash次数=1087, 读data字节数=11840, 读hash字节数=4348, 总耗时=276, 读data耗时=60, 读hash耗时=158, 运算耗时=58, 文本=秦王^{②}使人谓安陵君^{③}曰:“寡人欲以五百里之地易^{④}安陵,安陵君其^{⑤}许寡人!”安陵君曰:“大王加惠^{⑥},以大易小,甚善;虽然,受地于先王,愿终守之,弗敢易!”秦王不说。安陵君因使唐雎使于秦。秦王谓唐昨, node_num=127, rst_size=4544
    [xxxtest wordseg_tree 027]        276	60	158	58
    [test wordseg_get_tts_text 028] text_len=318, cost_time=363 ms 
    [test wordseg_do 028] wordseg_do次数=1, 文本长度=318, 读data次数=305, 读hash次数=611, 读data字节数=7088, 读hash字节数=2444, 总耗时=162, 读data耗时=46, 读hash耗时=79, 运算耗时=37, 文本=请同是诗人的建筑师建造《一千零一夜》的一千零一个梦，再添上一座座花园，一方方水池，一眼眼喷泉，加上成群的天鹅、朱鹭和孔雀，总而言之，请你假设人类幻想的某种令人眼花缭乱的洞府，其外观是神庙，是宫殿，那就是这座园林。, token_num=70,
    [xxxtest wordseg_do 028]        162	46	79	37	363
    [test wordseg_tree 028] wordseg_do次数=140, 文本长度=318, 读data次数=440, 读hash次数=958, 读data字节数=12224, 读hash字节数=3832, 总耗时=259, 读data耗时=54, 读hash耗时=145, 运算耗时=60, 文本=请同是诗人的建筑师建造《一千零一夜》的一千零一个梦，再添上一座座花园，一方方水池，一眼眼喷泉，加上成群的天鹅、朱鹭和孔雀，总而言之，请你假设人类幻想的某种令人眼花缭乱的洞府，其外观是神庙，是宫殿，那就是这座园林。, node_num=140, rst_size=4820
    [xxxtest wordseg_tree 028]        259	54	145	60
    [test wordseg_get_tts_text 029] text_len=495, cost_time=99 ms 
    [test wordseg_do 029] wordseg_do次数=1, 文本长度=495, 读data次数=222, 读hash次数=659, 读data字节数=7232, 读hash字节数=2636, 总耗时=214, 读data耗时=39, 读hash耗时=83, 运算耗时=92, 文本=quán lán lan huà de zhè zhāng huàbà ba gāng xià quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn bǎ huà tiē zài le qiáng shàng lán lan bù míng bai wèn wǒ zhīshì huà le zì jī de xiǎo shǒu wa wǒ yǒu nà me duō huà nín wèi shén me zhǐ tiē zhè yì zhāng ne quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn, token_num=203,
    [xxxtest wordseg_do 029]        214	39	83	92	99
    [test wordseg_tree 029] wordseg_do次数=204, 文本长度=495, 读data次数=446, 读hash次数=1103, 读data字节数=16272, 读hash字节数=4412, 总耗时=355, 读data耗时=68, 读hash耗时=151, 运算耗时=136, 文本=quán lán lan huà de zhè zhāng huàbà ba gāng xià quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn bǎ huà tiē zài le qiáng shàng lán lan bù míng bai wèn wǒ zhīshì huà le zì jī de xiǎo shǒu wa wǒ yǒu nà me duō huà nín wèi shén me zhǐ tiē zhè yì zhāng ne quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn, node_num=204, rst_size=7616
    */

    wordseg_offline_test(ws_engine->resident_buf, "客路青山外");
    wordseg_offline_test(ws_engine->resident_buf, "客路青山外，行舟绿水前。");
    wordseg_offline_test(ws_engine->resident_buf, "客路青山外，行舟绿水前。潮平两岸阔");
    wordseg_offline_test(ws_engine->resident_buf, "客路青山外，行舟绿水前。潮平两岸阔，风正一帆悬。海日生残夜，江春入旧年。");
    wordseg_offline_test(ws_engine->resident_buf, "阅读的时候，既读进去，又想开去，不仅可以深化对课文思想内容的理解，而且可以活跃思想，激发创造力。我们应该在这方面多下功夫。");
    wordseg_offline_test(ws_engine->resident_buf, "会场在天安门广场。广场成丁字形。丁字形一横的北面是一道河，河上并排架着五座白石桥；再北面是城墙，城墙中央高高耸起天安门的城楼。丁字形的一竖向南直伸到中华门。在一横一竖的交点的南面，场中挺立着一根电动旗杆。");
    wordseg_offline_test(ws_engine->resident_buf, "今天全省大部地区晴好依旧，上午10点气温来看，大部地区在20℃上下，较昨日同时又有2-4℃的升幅，而在下午13时10分左右，湖州最高温则达到了26.9 ℃。可惜这舒适的阳光将暂时退场，连晴了数日却依旧有所不舍。明天下午起，湖州多云转阴，还可能有阵雨的降临。春天天气向来如此多变，未来五天我省晴雨转换频繁，气温起伏变化较大。");
    wordseg_offline_test(ws_engine->resident_buf, "闶竳憞逅頜苬夐擞苬襢苬讚髣絽屗憬剓产鎶檤磴予淥颍虄局烠泸旯涖鄇旮旯胓夤旮旯胓薈僶旮旯酽頕鸰頜鼂颍鸮旯膱犴鏹鎧驾郐旯屗~载媒旯君厮旯苬恿播爿襢姦弰旯襢峞鸮旯竳躈旯槠軵旯蹋軵襢胓竳睮頠軵局頠覎釲篣絨攪櫜劵讚釲旯釲旯辸旯頜諢莬烥頜灻芛舝窙炗邌筪譡鈛弨蒦舝丷潁涖廜");
    wordseg_offline_test(ws_engine->resident_buf, "Whether or not you can do this well depends on your learning habits.");
    wordseg_offline_test(ws_engine->resident_buf, "朝阳 凿壁偷光 登金陵凤凰台 摄提贞于孟陬兮,惟庚寅吾以降");
    wordseg_offline_test(ws_engine->resident_buf, "for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his not");
    wordseg_offline_test(ws_engine->resident_buf, "New Zealand has its own government, but it is also part of the British Commonwealth, and therefore the official head of state is Elizabeth Ⅱ, the Queen of England, Scotland and Wales, New Zealand was the first country in the world to give the vote to women in 1893, to have old age pensions and the eight-hour working day.");
    wordseg_offline_test(ws_engine->resident_buf, "It is the fastest and the second cheapest, but you may have to wait for hours at the airport because of bad weather.By the time California elected to become the thirty-first federal state of the USA in 1850, it was already a multicultural society LATER ARRIVALS Although Chinese immigrants began to arrive during the Gold Rush Period, it was the building of the rail network from the west to the east coast that brought even larger numbers to California in the 1860s.");
    wordseg_offline_test(ws_engine->resident_buf, "Unfortunately for him, the water he sent over his balcony every day ended up on the McKay's, or too often, on the McKays themselves \" For the last fortnight, since Smith moved into the flat above us, we have no hardly dared go onto our balcony, \"said Laurene.");
    wordseg_offline_test(ws_engine->resident_buf, "and on the slopes at the foot of the hills there were houses with rich gardens and an open parkland with groves of trees and the white gleam of a classical templ Just beside him was that bare patch in the air as hard Oxford,hi whatever this new world was,");
    wordseg_offline_test(ws_engine->resident_buf, "Nowadays, within a short walk along a busy street, you are likely to find a chain store of some kind-a fast-food restaurant, a bakery or a convenience store Chain stores have become part of people's daily lives.");
    wordseg_offline_test(ws_engine->resident_buf, "When she brought the too to my room , she told me it was a holiday in the USA-it's Independence Day , when the Internationd Student Certr-that's centre where I come from I met lots of young people from at over the werld.");
    wordseg_offline_test(ws_engine->resident_buf, "abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdef ghij abcdefghi jabcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jefghi jabcdefghij abcdefghijabcdefghi jabcdefghijabcdefghijabcdefghidefghij abcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdei jabcdefghij abcdefghi jabcdefghijabcdefghi");
    wordseg_offline_test(ws_engine->resident_buf, "测试分词，白日依山尽，黄河入海流");
    wordseg_offline_test(ws_engine->resident_buf, "慈母手中线游子身上衣临行密密缝,意恐②迟迟归③。谁④}言⑤寸草心报得三春晖③。");	
    wordseg_offline_test(ws_engine->resident_buf, "贼又冷又饿，正在下树，抬头看见走来一个黑乎乎的东西，心想：“‘漏’又来了，这下我可活不成了！”他赶忙往树梢上爬，总嫌离地太近，紧爬慢爬，咔嚓一声，树枝断了，一个倒栽葱摔了下来，顺着山坡往下滚。");
    wordseg_offline_test(ws_engine->resident_buf, "假如你所从事的工作，是你的爱好，这七万个小时，将是怎样快活和充满创意的时光!假如你不喜欢它，漫长的七万个小时，足以让花容磨损，日月无光，每一天都如同穿着淋湿的衬衣，针芒在身。");
    wordseg_offline_test(ws_engine->resident_buf, "上路后，车夫说我们用饭之际，所有的游客都已赶到，甚至还抢在了我们前面；“但是，”他把握十足地说，“不必为此烦恼——静下心来——不要浮躁——他们虽已扬尘远去，可不久就会消失在我们身后的。");    
    wordseg_offline_test(ws_engine->resident_buf, "giraffe...帅...ah h ba3b ahi ba3l ele ble om1mt016res.o......::sh902bo be.t at t tt+t D1e1.S wa wa A.::....::??2she::::::..9..:e3e?./P U/..::.1..D8G200.7.....200212.33.:::..12,345.13131.2.215.12:5.151.24-1.34.::::20221.::...02::.20..n0o0.13:0....24:59:5924:524:5.::90:....:...0:..:...1.::....22千米22千米....123434332....1234123123..012013:..:::藏::..词语:词语:.....:吾.....凿壁偷光满面春风.首学:长:...:31:..移::古请四::占词....::《入返出av:家工古....");
    wordseg_offline_test(ws_engine->resident_buf, "让那些看不起民众、贱视民众、顽固的倒退的人们去赞美那贵族化的楠木（那也是直挺秀颀的），去鄙视这极常见、极易生长的白杨树吧，我要高声赞美白杨树！");
    wordseg_offline_test(ws_engine->resident_buf, "这只鹦鹉对我母亲真是一往情深，它热烈地追求她：在她的身边用各种古怪的姿势跳舞，一下子把它漂亮的冠毛打开来，一下子又合上；而且无论她到哪儿去，它都跟着；如果她不在，它一定像初来时找我一样，孜孜不倦地去找她。");
    wordseg_offline_test(ws_engine->resident_buf, "秦王^{②}使人谓安陵君^{③}曰:“寡人欲以五百里之地易^{④}安陵,安陵君其^{⑤}许寡人!”安陵君曰:“大王加惠^{⑥},以大易小,甚善;虽然,受地于先王,愿终守之,弗敢易!”秦王不说。安陵君因使唐雎使于秦。秦王谓唐昨");
    wordseg_offline_test(ws_engine->resident_buf, "请同是诗人的建筑师建造《一千零一夜》的一千零一个梦，再添上一座座花园，一方方水池，一眼眼喷泉，加上成群的天鹅、朱鹭和孔雀，总而言之，请你假设人类幻想的某种令人眼花缭乱的洞府，其外观是神庙，是宫殿，那就是这座园林。");
    wordseg_offline_test(ws_engine->resident_buf, "quán lán lan huà de zhè zhāng huàbà ba gāng xià quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn bǎ huà tiē zài le qiáng shàng lán lan bù míng bai wèn wǒ zhīshì huà le zì jī de xiǎo shǒu wa wǒ yǒu nà me duō huà nín wèi shén me zhǐ tiē zhè yì zhāng ne quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn");

    wordseg_uninit_entry();
    
    LOGD("---------------wordseg_offline_test_entry exit----------------use time %d ms--\n", os_ticks_get()-tick);

    return 0;
}

static int offline_test_wordseg_tree(void *engine, friso_token_t result, int result_num, FILE *fp)
{
	int i;
	int call_wordseg_num = 0;
	if (1 == result_num) {
		return 0;
	}
	char **ppWord = (char **)os_mem_alloc_ext(result_num * sizeof(char *), OS_MEM_ERAM);
	for (i = 0; i < result_num; i++) {
		char *word = (char *)os_mem_alloc_ext(result[i].length + 1, OS_MEM_ERAM);
		strcpy(word, result[i].word);
		ppWord[i] = word;
	}
	for (i = 0; i < result_num; i++)
	{
		friso_token_t result_tmp = NULL;
		int result_tmp_num = 0;
         g_info->call_num ++;    
		int ret = wordseg_do(engine, ppWord[i], &result_tmp_num, &result_tmp, NULL);
        g_info->token_num += result_tmp_num;
        // printf_wordseg_result(result_tmp, result_tmp_num);
		if (1 == result_tmp_num) {
			call_wordseg_num++;
		}
		else
		{
			if (NULL != fp) {
				fprintf(fp, "wordsegtree: text=%s, wordseg=/", ppWord[i]);
				for (int j = 0; j < result_tmp_num; j++) {
					fprintf(fp, "%s/", result_tmp[j].word);
				}
				fprintf(fp, "\r\n");
			}
			call_wordseg_num += offline_test_wordseg_tree(engine, result_tmp, result_tmp_num, fp);
		}
	}

	for (i = 0; i < result_num; i++) {
		os_mem_free(ppWord[i]);
	}
	os_mem_free(ppWord);

	return call_wordseg_num;
}

//用于离线测试和统计分词性能
static int wordseg_offline_test(void *engine, char *text)
{
    friso_token_t result = NULL;    
	int result_num = 0;
    static int test_num = 0;

    LOGW("[wordseg_offline_test enter]\n");  

    test_num ++;

    //测试获取tts干预文本的接口
#if 1
    int tts_buf_size = strlen(text)*4 + 2048;
    char *tts_buf = os_mem_alloc_ext(tts_buf_size, OS_MEM_ERAM);
    int32_t t5 = os_ticks_get();
    wordseg_get_tts_text(engine, text, tts_buf, tts_buf_size);
    int32_t t6 = os_ticks_get();
    LOGD("[test wordseg_get_tts_text %03d] text_len=%d, cost_time=%d ms \r\n", test_num, strlen(text), t6 - t5);
    os_mem_free(tts_buf);
#endif

    memset(g_info, 0, sizeof(g_info));
    g_info->call_num ++;    
    int32_t t1 = os_ticks_get();
	int ret = wordseg_do(engine, text, &result_num, &result, ws_engine->attr);
    int32_t t2 = os_ticks_get();
    g_info->cost_time += (t2 - t1);
    printf_wordseg_result(result, result_num);

    g_info->token_num += result_num;
    
    //log第一次分词的信息
    LOGD("[test wordseg_do %03d] wordseg_do次数=%d, 文本长度=%d, 读data次数=%d, 读hash次数=%d, 读data字节数=%d, 读hash字节数=%d, 总耗时=%d, 读data耗时=%d, 读hash耗时=%d, 运算耗时=%d, 文本=%s, token_num=%d,\r\n",
        test_num, g_info->call_num, strlen(text), g_info->read_data_res_num,g_info->read_hash_res_num,g_info->read_data_res_size, g_info->read_hash_res_size, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, text, g_info->token_num);
    LOGD("[xxxtest wordseg_do %03d]        %d\t%d\t%d\t%d\t%d\r\n", test_num, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, t6-t5);//方便统计到表格中

    t2 = os_ticks_get();
#if 0
    if(result_num > 1)
    {
        offline_test_wordseg_tree(engine, result, result_num, NULL);
    }        
    int32_t t3 = os_ticks_get();
    g_info->cost_time += (t3 - t2);    

    //log分词树的信息(应用的调用逻辑)
    printk("[test wordseg_tree %03d] wordseg_do次数=%d, 文本长度=%d, 读data次数=%d, 读hash次数=%d, 读data字节数=%d, 读hash字节数=%d, 总耗时=%d, 读data耗时=%d, 读hash耗时=%d, 运算耗时=%d, 文本=%s, token_num=%d,\r\n",
        test_num, g_info->call_num, strlen(text), g_info->read_data_res_num,g_info->read_hash_res_num,g_info->read_data_res_size, g_info->read_hash_res_size, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, text, g_info->token_num);
    printk("[test2 wordseg_tree %03d]        %d\t%d\t%d\t%d\r\n", test_num, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time);//方便统计到表格中
#else
    memset(g_info, 0, sizeof(g_info));
    g_info->call_num ++;    
    tree_result_t *tree_rst = NULL;
    int tree_rst_size = 0;
    wordseg_tree_do(engine, text, &tree_rst, &tree_rst_size);    
    int32_t t3 = os_ticks_get();
    g_info->cost_time += (t3 - t2);    

    //log分词树的信息(应用的调用逻辑)
    LOGD("[test wordseg_tree %03d] wordseg_do次数=%d, 文本长度=%d, 读data次数=%d, 读hash次数=%d, 读data字节数=%d, 读hash字节数=%d, 总耗时=%d, 读data耗时=%d, 读hash耗时=%d, 运算耗时=%d, 文本=%s, node_num=%d, rst_size=%d\r\n",
        test_num, tree_rst->call_wordsegdo_cnt, strlen(text), g_info->read_data_res_num,g_info->read_hash_res_num,g_info->read_data_res_size, g_info->read_hash_res_size, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, text, tree_rst->node_num, tree_rst_size);
    LOGD("[xxxtest wordseg_tree %03d]        %d\t%d\t%d\t%d\r\n", test_num, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time);//方便统计到表格中
    if(tree_rst != NULL){
        os_mem_free(tree_rst);
    }
#endif

    LOGW("[wordseg_offline_test exit]\n");  
    return 0;
}

// 执行一次分词的入口
static int wordseg_run_entry(char *text)
{   
#if READ_DATA_RES_LOG
    memset(g_info, 0, sizeof(g_info));
#endif

    int wordseg_result_num = 0;
    friso_token_t token = NULL;
    g_info->call_num ++;    
    uint32_t t1 = os_ticks_get();
    int ret = wordseg_do(ws_engine->resident_buf, text, &wordseg_result_num, &token, ws_engine->attr);
    if (0 != ret)
    {
        LOGE("[wordseg]wordseg_do err=%d\n", ret);
        wordseg_result_num = 0;
        return -1;
    }
    uint32_t t2 = os_ticks_get();     
    g_info->cost_time = t2 - t1;

#if 0
    // 该接口是为了在core1上调用分词，结果需要使用内部RAM传回给core0,占用内存不能太大
    ret = wordseg_result_convert(token, wordseg_result_num, ws_engine->result, ws_engine->result_size);
    if (0 != ret)
    {
        printk("[wordseg]wordseg_result_convert err=%d", ret);
        wordseg_result_num = 0;
        return -1;
    }
    uint32_t t3 = os_ticks_get();    
#endif

// 打印分词结果
#if 1
#if READ_DATA_RES_LOG
    LOGD("[wordseg_do] run_times=%d, token_num=%d, text_len=%d, use_time=%d ms, read_res_num=%d, read_res_size=%d, read_res_time=%d ms, text=%s\n", g_info->call_num,
           wordseg_result_num, strlen(text), g_info->cost_time, g_info->read_data_res_num, g_info->read_data_res_size, g_info->read_data_res_time, text);
#else
    printk("[wordseg_do] run_times=%d, token_num=%d, text_len=%d, use_time=%d ms, text=%s\n",g_info->call_num, wordseg_result_num, strlen(text), g_info->cost_time, text);
#endif
#if 0
    printk("[wordseg]attr type=%d, word_delpunc=%s, word_tts=%s", ws_engine->attr->text_type, ws_engine->attr->word_delpunc, ws_engine->attr->word_tts);
    for (int i = 0; i < wordseg_result_num; i++)
    {
        printk("   token%03d: %s, %s, %s", i + 1, token[i].word, token[i].word_delpunc, token[i].word_tts);
        if (0 != strcmp(token[i].word, ws_engine->result[i].word) || 0 != strcmp(token[i].word_delpunc, ws_engine->result[i].word_delpunc ||
                                                                                                             0 != strcmp(token[i].word_tts, ws_engine->result[i].word_tts)))
        {
            // printk("   tokenExt%03d: %s, %s, %s", i+1, ws_engine->result[i].word, ws_engine->result[i].word_delpunc, ws_engine->result[i].word_tts);
            // printk("error---------------------------");
        }
    }
#endif
#endif

    // printk("[wordseg]wordseg_do result_num=%d", wordseg_result_num);

    return 0;
}

static void printf_wordseg_result(friso_token_t result, int num)
{
    if(NULL == result || num < 1)   return;
    for (int i = 0; i < num; i++)
    {
        LOGD("   token%03d: %s, %s, %s\n", i + 1, result[i].word, result[i].word_delpunc, result[i].word_tts);      
    }
}

static void print_buf(char *log_str, unsigned char *buf, int size)
{
    LOGD("%s, buf=%p\n", log_str, buf);
    for (int i = 0; i < size; i += 16)
    {
        if (i + 16 > size)
            break;

        int j = i;
        LOGD("0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, ",
                    buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++],
                    buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++]);
    }
    LOGD("---------------------------------------\n");
}
#endif

static void * malloc_callback(unsigned int size, int type)
{
    // LOGD("malloc_callback size=%d, type=PSRAM\n", size);
    return os_mem_alloc_ext(size, OS_MEM_ERAM);
    
}

static void free_callback(void *p)
{
	os_mem_free(p);
}
