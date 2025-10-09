
#include "wordseg.h"
#include "wordseg_res.h"
#include "mpu_wrappers.h"
#include "appinc.h"
#include "venus_ap.h"

/* 备忘：查看psram和ram配置等看gcc_arm.ld 38行左右，分配内部ram用malloc，分配psram用heap_psram_malloc */

#define OS_MEM_IRAM    0   //芯片内部ram
#define OS_MEM_ERAM    1   //芯片外部ram 
#ifndef NULL
#define NULL 0
#endif
#define wordseg_log    printk // printk //LOGE LOGI
typedef unsigned int size_t;
/*----------------------------------------------------*/

#define TEST_HASH_RES_IN_NORFLASH       (0) // 测试hash资源放在norflash和psram的性能区别,若设置1,hash.bin需要烧录到csk,首地址要搞对
#define TEST_HASH_RES_IN_TF             (1) //测试hash资源直接放在tf卡上裸读，不在一次加载到psram中 20230411


#define READ_DATA_RES_LOG               (1) // 打印资源读取次数等信息


/*----------------------------以下信息和TF卡资源存放位置、csk的烧录位置强相关，请修改------------------------*/
#define HASH_RES_BUF_ADDR               (0x19000000) // hash.bin在norflash的地址,要和烧录的地址配套
#define HASH_RES_DATA_SIZE              (2877936)   // hash.bin文件字节数


//8GB lethink FAT32 64KB
#define HASH_RES_SD_CARD_OFFSET         (0x12A90000 + 2048 * SD_CARD_SECTOR_SIZE) // hash.bin资源在TF卡的偏移地址
#define DATA_RES_SD_CARD_OFFSET         (0x13260000 + 2048 * SD_CARD_SECTOR_SIZE) // data.bin资源在TF卡的偏移地址

/*----------------------------------------------------------------------------------------------------------*/


#define SD_CARD_SECTOR_SIZE             (512)                                      // 等于low_sdcard.c里的s_card->csd.sector_size

#define ivGridSize_u64(n, m) ((unsigned long long)(((unsigned long long)(n) + ((m)-1)) & (~(m) + 1)))
#define ivGridSize(n, m) ((size_t)(((size_t)(n) + ((m)-1)) & (~(m) + 1)))

static void *ws_resident_buf = NULL, *ws_tmp_buf = NULL;
static int ws_resident_buf_size = 0, ws_tmp_buf_size = 0;

static int32_t read_sdres_buf_size = 0;
static void *read_sdres_buf = NULL; // 用于读data.bin/hash.bin回调中,每次需要读的未必是block_size的倍数，需要前后多读点，再拷贝需要的
#if TEST_HASH_RES_IN_TF
static int32_t read_hash_buf_size = 0;
static void *read_hash_buf = NULL; // 用于读data.bin回调中,每次需要读的未必是block_size的倍数，需要前后多读点，再拷贝需要的
static int read_hash_res_in_file(void *p, int offset, int size, void **dst, int dst_size);
#endif

// 分词结果
static wordseg_result_t * wordseg_result_ext = NULL; // 分词结果
static int32_t wordseg_result_ext_size = 0;
static text_attr_t wordseg_attr[1] = {0};

static int read_data_res_in_file(void *p, int offset, int size, void **dst, int dst_size);
static void *my_mem_alloc(unsigned int size, int type);
static void my_mem_free(void *ptr);
static int wordseg_init_entry();
static int wordseg_run_entry(char *text);
static uint32_t os_ticks_get(void);

//用于离线测试和统计分词性能
static int offline_test_wordseg(void *engine, char *text);
static int offline_test_wordseg_tree(void *engine, friso_token_t result, int result_num, FILE *fp);
static void printf_wordseg_result(friso_token_t result, int num);
static void test_tf();


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

// 测试分词性能入口函数
int test_wordseg()
{
    int ret = 0;
    
    wordseg_log("----------------test_wordseg begin------------------\n");
    wordseg_log("free heap:%d", xPortGetFreeHeapSize());

    int32_t tick = os_ticks_get();

    test_tf(); //测试TF卡读速度

#if TEST_HASH_RES_IN_TF
    wordseg_log("hash资源放在TF中\n");
#else
    wordseg_log("hash资源放在%s中\n", TEST_HASH_RES_IN_NORFLASH != 0 ? "norflash" : "psram");
#endif

    ret = wordseg_init_entry();
    if (0 != ret)
    {
        wordseg_log("wordseg_init_entry err. ret=%d\n", ret);
        return ret;
    }
#if 0
    ret = wordseg_run_entry("测试分词，白日依山尽，黄河入海流");
    if (0 != ret)
    {
        wordseg_log("wordseg_run_entry err. ret=%d\n", ret);
        return ret;
    }
#else    
    offline_test_wordseg(ws_resident_buf, "客路青山外");
    offline_test_wordseg(ws_resident_buf, "客路青山外，行舟绿水前。");
    offline_test_wordseg(ws_resident_buf, "客路青山外，行舟绿水前。潮平两岸阔");
    offline_test_wordseg(ws_resident_buf, "客路青山外，行舟绿水前。潮平两岸阔，风正一帆悬。海日生残夜，江春入旧年。");
    offline_test_wordseg(ws_resident_buf, "阅读的时候，既读进去，又想开去，不仅可以深化对课文思想内容的理解，而且可以活跃思想，激发创造力。我们应该在这方面多下功夫。");
    offline_test_wordseg(ws_resident_buf, "会场在天安门广场。广场成丁字形。丁字形一横的北面是一道河，河上并排架着五座白石桥；再北面是城墙，城墙中央高高耸起天安门的城楼。丁字形的一竖向南直伸到中华门。在一横一竖的交点的南面，场中挺立着一根电动旗杆。");
    offline_test_wordseg(ws_resident_buf, "今天全省大部地区晴好依旧，上午10点气温来看，大部地区在20℃上下，较昨日同时又有2-4℃的升幅，而在下午13时10分左右，湖州最高温则达到了26.9 ℃。可惜这舒适的阳光将暂时退场，连晴了数日却依旧有所不舍。明天下午起，湖州多云转阴，还可能有阵雨的降临。春天天气向来如此多变，未来五天我省晴雨转换频繁，气温起伏变化较大。");
    offline_test_wordseg(ws_resident_buf, "闶竳憞逅頜苬夐擞苬襢苬讚髣絽屗憬剓产鎶檤磴予淥颍虄局烠泸旯涖鄇旮旯胓夤旮旯胓薈僶旮旯酽頕鸰頜鼂颍鸮旯膱犴鏹鎧驾郐旯屗~载媒旯君厮旯苬恿播爿襢姦弰旯襢峞鸮旯竳躈旯槠軵旯蹋軵襢胓竳睮頠軵局頠覎釲篣絨攪櫜劵讚釲旯釲旯辸旯頜諢莬烥頜灻芛舝窙炗邌筪譡鈛弨蒦舝丷潁涖廜");
    offline_test_wordseg(ws_resident_buf, "Whether or not you can do this well depends on your learning habits.");
    offline_test_wordseg(ws_resident_buf, "朝阳 凿壁偷光 登金陵凤凰台 摄提贞于孟陬兮,惟庚寅吾以降");
    offline_test_wordseg(ws_resident_buf, "for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his notes, for a computer program to organise his not");
    offline_test_wordseg(ws_resident_buf, "New Zealand has its own government, but it is also part of the British Commonwealth, and therefore the official head of state is Elizabeth Ⅱ, the Queen of England, Scotland and Wales, New Zealand was the first country in the world to give the vote to women in 1893, to have old age pensions and the eight-hour working day.");
    offline_test_wordseg(ws_resident_buf, "It is the fastest and the second cheapest, but you may have to wait for hours at the airport because of bad weather.By the time California elected to become the thirty-first federal state of the USA in 1850, it was already a multicultural society LATER ARRIVALS Although Chinese immigrants began to arrive during the Gold Rush Period, it was the building of the rail network from the west to the east coast that brought even larger numbers to California in the 1860s.");
    offline_test_wordseg(ws_resident_buf, "Unfortunately for him, the water he sent over his balcony every day ended up on the McKay's, or too often, on the McKays themselves \" For the last fortnight, since Smith moved into the flat above us, we have no hardly dared go onto our balcony, \"said Laurene.");
    offline_test_wordseg(ws_resident_buf, "and on the slopes at the foot of the hills there were houses with rich gardens and an open parkland with groves of trees and the white gleam of a classical templ Just beside him was that bare patch in the air as hard Oxford,hi whatever this new world was,");
    offline_test_wordseg(ws_resident_buf, "Nowadays, within a short walk along a busy street, you are likely to find a chain store of some kind-a fast-food restaurant, a bakery or a convenience store Chain stores have become part of people's daily lives.");
    offline_test_wordseg(ws_resident_buf, "When she brought the too to my room , she told me it was a holiday in the USA-it's Independence Day , when the Internationd Student Certr-that's centre where I come from I met lots of young people from at over the werld.");
    offline_test_wordseg(ws_resident_buf, "abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdef ghij abcdefghi jabcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghij abcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdefghi jefghi jabcdefghij abcdefghijabcdefghi jabcdefghijabcdefghijabcdefghidefghij abcdefghijabcdefghi jabcdefghi jabcdefghi jabcdefghi jabcdei jabcdefghij abcdefghi jabcdefghijabcdefghi");
    offline_test_wordseg(ws_resident_buf, "测试分词，白日依山尽，黄河入海流");
    offline_test_wordseg(ws_resident_buf, "慈母手中线游子身上衣临行密密缝,意恐②迟迟归③。谁④}言⑤寸草心报得三春晖③。");	
    offline_test_wordseg(ws_resident_buf, "贼又冷又饿，正在下树，抬头看见走来一个黑乎乎的东西，心想：“‘漏’又来了，这下我可活不成了！”他赶忙往树梢上爬，总嫌离地太近，紧爬慢爬，咔嚓一声，树枝断了，一个倒栽葱摔了下来，顺着山坡往下滚。");
    offline_test_wordseg(ws_resident_buf, "假如你所从事的工作，是你的爱好，这七万个小时，将是怎样快活和充满创意的时光!假如你不喜欢它，漫长的七万个小时，足以让花容磨损，日月无光，每一天都如同穿着淋湿的衬衣，针芒在身。");
    offline_test_wordseg(ws_resident_buf, "上路后，车夫说我们用饭之际，所有的游客都已赶到，甚至还抢在了我们前面；“但是，”他把握十足地说，“不必为此烦恼——静下心来——不要浮躁——他们虽已扬尘远去，可不久就会消失在我们身后的。");    
    offline_test_wordseg(ws_resident_buf, "giraffe...帅...ah h ba3b ahi ba3l ele ble om1mt016res.o......::sh902bo be.t at t tt+t D1e1.S wa wa A.::....::??2she::::::..9..:e3e?./P U/..::.1..D8G200.7.....200212.33.:::..12,345.13131.2.215.12:5.151.24-1.34.::::20221.::...02::.20..n0o0.13:0....24:59:5924:524:5.::90:....:...0:..:...1.::....22千米22千米....123434332....1234123123..012013:..:::藏::..词语:词语:.....:吾.....凿壁偷光满面春风.首学:长:...:31:..移::古请四::占词....::《入返出av:家工古....");
    offline_test_wordseg(ws_resident_buf, "让那些看不起民众、贱视民众、顽固的倒退的人们去赞美那贵族化的楠木（那也是直挺秀颀的），去鄙视这极常见、极易生长的白杨树吧，我要高声赞美白杨树！");
    offline_test_wordseg(ws_resident_buf, "这只鹦鹉对我母亲真是一往情深，它热烈地追求她：在她的身边用各种古怪的姿势跳舞，一下子把它漂亮的冠毛打开来，一下子又合上；而且无论她到哪儿去，它都跟着；如果她不在，它一定像初来时找我一样，孜孜不倦地去找她。");
    offline_test_wordseg(ws_resident_buf, "秦王^{②}使人谓安陵君^{③}曰:“寡人欲以五百里之地易^{④}安陵,安陵君其^{⑤}许寡人!”安陵君曰:“大王加惠^{⑥},以大易小,甚善;虽然,受地于先王,愿终守之,弗敢易!”秦王不说。安陵君因使唐雎使于秦。秦王谓唐昨");
    offline_test_wordseg(ws_resident_buf, "请同是诗人的建筑师建造《一千零一夜》的一千零一个梦，再添上一座座花园，一方方水池，一眼眼喷泉，加上成群的天鹅、朱鹭和孔雀，总而言之，请你假设人类幻想的某种令人眼花缭乱的洞府，其外观是神庙，是宫殿，那就是这座园林。");
    offline_test_wordseg(ws_resident_buf, "quán lán lan huà de zhè zhāng huàbà ba gāng xià quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn bǎ huà tiē zài le qiáng shàng lán lan bù míng bai wèn wǒ zhīshì huà le zì jī de xiǎo shǒu wa wǒ yǒu nà me duō huà nín wèi shén me zhǐ tiē zhè yì zhāng ne quán jiā rén dōu xǐ huan lán lan huà de zhè zhāng huàbà ba gāng xià bān huí lái ná qǐ huà kàn le yòu kàn");
#endif

    wordseg_log("----------------test_wordseg end. used time %d ms------------------\n", os_ticks_get()-tick);

    return 0;
}

static int offline_test_wordseg_tree(void *engine, friso_token_t result, int result_num, FILE *fp)
{
	int i;
	int call_wordseg_num = 0;
	if (1 == result_num) {
		return 0;
	}
	char **ppWord = (char **)my_mem_alloc(result_num * sizeof(char *), OS_MEM_ERAM);
	for (i = 0; i < result_num; i++) {
		char *word = (char *)my_mem_alloc(result[i].length + 1, OS_MEM_ERAM);
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
        //printf_wordseg_result(result_tmp, result_tmp_num);
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
		my_mem_free(ppWord[i]);
	}
	my_mem_free(ppWord);

	return call_wordseg_num;
}

//用于离线测试和统计分词性能
static int offline_test_wordseg(void *engine, char *text)
{
    friso_token_t result = NULL;    
	int result_num = 0;
    static int test_num = 0;

    test_num ++;

    //测试获取tts干预文本的接口
#if 1
    int tts_buf_size = strlen(text)*4 + 2048;
    char *tts_buf = my_mem_alloc(tts_buf_size, OS_MEM_ERAM);
    int32_t t5 = os_ticks_get();
    wordseg_get_tts_text(engine, text, tts_buf, tts_buf_size);
    int32_t t6 = os_ticks_get();
    wordseg_log("[qungao_tts wordseg_get_tts_text %03d] text_len=%d, cost_time=%d ms \r\n", test_num, strlen(text), t6 - t5);
    my_mem_free(tts_buf);
#endif

    memset(g_info, 0, sizeof(g_info));
    g_info->call_num ++;    
    int32_t t1 = os_ticks_get();
	int ret = wordseg_do(engine, text, &result_num, &result, wordseg_attr);
    int32_t t2 = os_ticks_get();
    g_info->cost_time += (t2 - t1);
    //printf_wordseg_result(result, result_num);

    g_info->token_num += result_num;
    
    //log第一次分词的信息
    wordseg_log("[qungao wordseg_do %03d] wordseg_do次数=%d, 文本长度=%d, 读data次数=%d, 读hash次数=%d, 读data字节数=%d, 读hash字节数=%d, 总耗时=%d, 读data耗时=%d, 读hash耗时=%d, 运算耗时=%d, 文本=%s, token_num=%d,\r\n",
        test_num, g_info->call_num, strlen(text), g_info->read_data_res_num,g_info->read_hash_res_num,g_info->read_data_res_size, g_info->read_hash_res_size, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, text, g_info->token_num);
    wordseg_log("[qungao_tab wordseg_do %03d]        %d\t%d\t%d\t%d\t%d\r\n", test_num, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, t6-t5);//方便统计到表格中

    t2 = os_ticks_get();
    if(result_num > 1)
    {
        offline_test_wordseg_tree(engine, result, result_num, NULL);
    }        
    int32_t t3 = os_ticks_get();
    g_info->cost_time += (t3 - t2);    

    //log分词树的信息(应用的调用逻辑)
    wordseg_log("[qungao wordseg_tree %03d] wordseg_do次数=%d, 文本长度=%d, 读data次数=%d, 读hash次数=%d, 读data字节数=%d, 读hash字节数=%d, 总耗时=%d, 读data耗时=%d, 读hash耗时=%d, 运算耗时=%d, 文本=%s, token_num=%d,\r\n",
        test_num, g_info->call_num, strlen(text), g_info->read_data_res_num,g_info->read_hash_res_num,g_info->read_data_res_size, g_info->read_hash_res_size, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time, text, g_info->token_num);
    wordseg_log("[qungao_tab wordseg_tree %03d]        %d\t%d\t%d\t%d\r\n", test_num, g_info->cost_time, g_info->read_data_res_time, g_info->read_hash_res_time,
        g_info->cost_time-g_info->read_data_res_time-g_info->read_hash_res_time);//方便统计到表格中

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
    int ret = wordseg_do(ws_resident_buf, text, &wordseg_result_num, &token, wordseg_attr);
    if (0 != ret)
    {
        wordseg_log("[wordseg]wordseg_do err=%d\n", ret);
        wordseg_result_num = 0;
        return -1;
    }
    uint32_t t2 = os_ticks_get();     
    g_info->cost_time = t2 - t1;

#if 0
    // 该接口是为了在core1上调用分词，结果需要使用内部RAM传回给core0,占用内存不能太大
    ret = wordseg_result_convert(token, wordseg_result_num, wordseg_result_ext, wordseg_result_ext_size);
    if (0 != ret)
    {
        wordseg_log("[wordseg]wordseg_result_convert err=%d\n", ret);
        wordseg_result_num = 0;
        return -1;
    }
    uint32_t t3 = os_ticks_get();    
#endif

// 打印分词结果
#if 1
#if READ_DATA_RES_LOG
    wordseg_log("[wordseg_do] run_times=%d, token_num=%d, text_len=%d, use_time=%d ms, read_res_num=%d, read_res_size=%d, read_res_time=%d ms, text=%s\n", g_info->call_num,
           wordseg_result_num, strlen(text), g_info->cost_time, g_info->read_data_res_num, g_info->read_data_res_size, g_info->read_data_res_time, text);
#else
    wordseg_log("[wordseg_do] run_times=%d, token_num=%d, text_len=%d, use_time=%d ms, text=%s\n",g_info->call_num, wordseg_result_num, strlen(text), g_info->cost_time, text);
#endif
#if 0
    wordseg_log("[wordseg]attr type=%d, word_delpunc=%s, word_tts=%s\n", wordseg_attr->text_type, wordseg_attr->word_delpunc, wordseg_attr->word_tts);
    for (int i = 0; i < wordseg_result_num; i++)
    {
        wordseg_log("   token%03d: %s, %s, %s\n", i + 1, token[i].word, token[i].word_delpunc, token[i].word_tts);
        if (0 != strcmp(token[i].word, wordseg_result_ext[i].word) || 0 != strcmp(token[i].word_delpunc, wordseg_result_ext[i].word_delpunc ||
                                                                                                             0 != strcmp(token[i].word_tts, wordseg_result_ext[i].word_tts)))
        {
            // wordseg_log("   tokenExt%03d: %s, %s, %s\n", i+1, wordseg_result_ext[i].word, wordseg_result_ext[i].word_delpunc, wordseg_result_ext[i].word_tts);
            // wordseg_log("error---------------------------\n");
        }
    }
#endif
#endif

    // wordseg_log("[wordseg]wordseg_do result_num=%d\n", wordseg_result_num);

    return 0;
}

static void printf_wordseg_result(friso_token_t result, int num)
{
    if(NULL == result || num < 1)   return;
    for (int i = 0; i < num; i++)
    {
        wordseg_log("   token%03d: %s, %s, %s\n", i + 1, result[i].word, result[i].word_delpunc, result[i].word_tts);      
    }
}

static uint32_t os_ticks_get(void)
{
    return xTaskGetTickCount()*os_ticks_get; //ms
}

static void print_buf(char *log_str, unsigned char *buf, int size)
{
    wordseg_log("%s, buf=%p\n", log_str, buf);
    for (int i = 0; i < size; i += 16)
    {
        if (i + 16 > size)
            break;

        int j = i;
        wordseg_log("0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, \n",
                    buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++],
                    buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++], buf[j++]);
    }
    wordseg_log("---------------------------------------\n");
}

// 分词引擎初始化相关
static int wordseg_init_entry()
{
    int ret = 0;
    int32_t is_failed = 0;

    int ticks = os_ticks_get();

    PHashResHdr hash_hdr = NULL;
    void *hash_res_buf = NULL; // hash.bin资源文件加载到内存buf指针

    wordseg_log("[wordseg]version: %s\n", wordseg_getversion());

#if 0
    // 读取分词hash资源   
    int hash_res_size = 0;

    hash_res_buf = HASH_RES_BUF_ADDR;
    hash_res_size = HASH_RES_DATA_SIZE;
    hash_hdr = (PHashResHdr)hash_res_buf;
    wordseg_log("hash_res_hdr info: buf=%p, file_crc=%u, file_crc_size=%u\n", hash_res_buf, hash_hdr->file_crc, hash_hdr->file_crc_size);
    if (hash_hdr->file_crc != 1227404828 || hash_hdr->file_crc_size != 1438086) // 分词资源强相关，tmp
    {
        wordseg_log("error:hash res is wrong.\n");
        return -1;
    }

#if !TEST_HASH_RES_IN_NORFLASH && !TEST_HASH_RES_IN_TF
    //验证hash.bin放到psram中的性能
    char *buf_new = my_mem_alloc(hash_res_size, OS_MEM_ERAM);
    if(NULL == buf_new)
    {
        wordseg_log("malloc %d bytes failed.\n", hash_res_size);
        return -1;
    }
    memcpy(buf_new, hash_res_buf, hash_res_size);
    hash_res_buf = buf_new;    
#endif
#endif

    // 读取data.bin资源
    int data_res_handle = 0; // data.bin资源句柄，如果是使用读文件方式就是文件句柄，如果是裸读sd卡，就是文件在sd卡的偏移地址
    data_res_handle = DATA_RES_SD_CARD_OFFSET;

#if 1 // 看下SD卡里的资源对不对
    PDataResHdr data_hdr = (PDataResHdr)data_res_handle;
    wordseg_log("data_res_hdr info: sd offset=%p\n", data_res_handle);

    int tmp_buf_size = SD_CARD_SECTOR_SIZE;
    char *tmp_buf = (char *)my_mem_alloc(tmp_buf_size, OS_MEM_IRAM);
    if (NULL == tmp_buf)
    {
        wordseg_log("error. tmp_buf malloc failed.\n");
        return -1;
    }
    memset(tmp_buf, 0, tmp_buf_size);

    read_data_res_in_file(0x100, 0, tmp_buf_size, &tmp_buf, tmp_buf_size);
    data_hdr = (PDataResHdr)tmp_buf;    
    wordseg_log("data res hdr info: crc=%u, crc_size=%u, version=%s, checkstr=%u, matchcrc=%u\n",
                data_hdr->file_crc, data_hdr->file_crc_size, data_hdr->version, data_hdr->check_str, data_hdr->hash_data_match_crc);
    // if (data_hdr->file_crc != 3026961302 || data_hdr->file_crc_size != 4086461) // 分词资源强相关，tmp
    // {
    //     wordseg_log("error:data res hdr is wrong.\n");
    //     return -1;
    // }

#if TEST_HASH_RES_IN_TF
    read_hash_res_in_file(0x100, 0, tmp_buf_size, &tmp_buf, tmp_buf_size);
    hash_hdr = (PHashResHdr)tmp_buf;
    wordseg_log("hash res hdr info: crc=%u, crc_size=%u, version=%s, checkstr=%u, matchcrc=%u\n",
                hash_hdr->file_crc, hash_hdr->file_crc_size, hash_hdr->version, hash_hdr->check_str, hash_hdr->hash_data_match_crc);
#endif

    my_mem_free(tmp_buf);
#endif

    // 分配传递分词结果的结构体
    if (NULL == wordseg_result_ext)
    {
        wordseg_result_ext_size = WS_TOKEN_MAX_NUM * (sizeof(wordseg_result_t) + 3) + WS_INPUT_TEXT_MAX_LEN * 3; // 第一个3表示word/word_delpunc/word_tts结尾的/0,第二个3上述三种文本总字符大小不会超过输入文本最大值WS_INPUT_TEXT_MAX_LEN
        wordseg_result_ext = my_mem_alloc(wordseg_result_ext_size, OS_MEM_IRAM);
        if (wordseg_result_ext == NULL)
        {
            wordseg_log("[wordseg]malloc IRAM %d Bytes failed.\n", wordseg_result_ext_size);
            is_failed = 1;
            goto wordseg_init_end;
        }
        wordseg_log("[wordseg]malloc IRAM: wordseg_result_ext size=%d, p=%p\n", wordseg_result_ext_size, wordseg_result_ext);
    }
#if TEST_HASH_RES_IN_TF
    read_sdres_buf_size = 4 * 1024;
    read_sdres_buf = my_mem_alloc(read_sdres_buf_size, OS_MEM_IRAM); //wordseg_init就会用到,先分配一次
#endif

    // 获取分词引擎需要的内存
#if TEST_HASH_RES_IN_TF
    hash_res_buf = my_mem_alloc(4, OS_MEM_IRAM); //其实没用，给个假的先
    ret = wordseg_init_ext(NULL, &ws_resident_buf_size, NULL, &ws_tmp_buf_size, hash_res_buf, read_hash_res_in_file, (void *)(data_res_handle), read_data_res_in_file);
#else
    ret = wordseg_init(NULL, &ws_resident_buf_size, NULL, &ws_tmp_buf_size, hash_res_buf, (void *)(data_res_handle), read_data_res_in_file);
#endif
    if (ret != TC_WSErrID_TellSize)
    {
        wordseg_log("[wordseg]wordseg_init err:%d\n", ret);
        is_failed = 1;
        goto wordseg_init_end;
    }    
    ws_resident_buf = my_mem_alloc(ws_resident_buf_size, OS_MEM_ERAM);
    ws_tmp_buf = my_mem_alloc(ws_tmp_buf_size, OS_MEM_ERAM);
    if (NULL == ws_resident_buf || NULL == ws_tmp_buf)
    {
        wordseg_log("[wordseg] malloc ws_resident_buf/ws_tmp_buf failed. \n");
        is_failed = 1;
        goto wordseg_init_end;
    }
    wordseg_log("[wordseg]malloc PSRAM: ws_resident_buf size=%d, p=%p\n", ws_resident_buf_size, ws_resident_buf);
    wordseg_log("[wordseg]malloc PSRAM: ws_tmp_buf size=%d, p=%p\n", ws_tmp_buf_size, ws_tmp_buf);


    // 分词引擎初始化
#if TEST_HASH_RES_IN_TF    
    ret = wordseg_init_ext(ws_resident_buf, &ws_resident_buf_size, ws_tmp_buf, &ws_tmp_buf_size, hash_res_buf, read_hash_res_in_file, (void *)(data_res_handle), read_data_res_in_file);
#else
    ret = wordseg_init(ws_resident_buf, &ws_resident_buf_size, ws_tmp_buf, &ws_tmp_buf_size, hash_res_buf, (void *)(data_res_handle), read_data_res_in_file);
#endif
    if (ret != 0)
    {
        wordseg_log("[wordseg]wordseg_init err:%d\n", ret);
        is_failed = 1;
        goto wordseg_init_end;
    }

    wordseg_get_read_data_buf_size(ws_resident_buf, &read_sdres_buf_size);
    if (read_sdres_buf != NULL)
    {
        my_mem_free(read_sdres_buf);
        read_sdres_buf = NULL;
    }
    read_sdres_buf = my_mem_alloc(read_sdres_buf_size, OS_MEM_IRAM);
    if (NULL == read_sdres_buf)
    {
        wordseg_log("[wordseg]malloc read_sd_res_buf size=%d, failed.\n", read_sdres_buf_size);
        is_failed = 1;
        goto wordseg_init_end;
    }
    wordseg_log("[wordseg]malloc IRAM: read_sd_res_buf size=%d, p=%p\n", read_sdres_buf_size, read_sdres_buf);

    wordseg_log("[wordseg_init_entry]ok. used time=%d ms\n", os_ticks_get() - ticks);

    return 0;

wordseg_init_end:
    if (is_failed)
    {
        if (NULL != wordseg_result_ext)
        {
            my_mem_free(wordseg_result_ext);
            wordseg_result_ext = NULL;
        }
        if(NULL != read_sdres_buf)
        {
            my_mem_free(read_sdres_buf);
            read_sdres_buf = NULL;
        }
        if (NULL != hash_res_buf)
        {
            my_mem_free(hash_res_buf);
            hash_res_buf = NULL;
        }
        if (NULL != ws_resident_buf)
        {
            my_mem_free(ws_resident_buf);
            ws_resident_buf = NULL;
        }
        if (NULL != ws_tmp_buf)
        {
            my_mem_free(ws_tmp_buf);
            ws_tmp_buf = NULL;
        }

        wordseg_log("[wordseg]engine_wordseg_init failed. used time=%d ms\n", os_ticks_get() - ticks);
    }

    return -1;
}

//
static int low_sd_read_sectors(unsigned char *buff, unsigned long sector, unsigned int count)
{
    return gm_sdc_api_sdcard_sector_read(0 /*SD_0*/, sector, count, buff);
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
            wordseg_log("[wordseg]error out of memory %d, %d\n", read_size, read_sdres_buf_size);
            return TC_WSErrID_ReadDataResErr;            
        }
        // 这种情况一般是wordseg_do调用,直接用外部申请的IRAM指针read_data_buf进行low_sd_read_sectors加速
        ret = low_sd_read_sectors(buffer, sector, count);
        if (ret != 0)
        {
            wordseg_log("[wordseg]read_data low_sd_read_sectors 2 failed. ret=%d\n", ret);
            return TC_WSErrID_ReadDataResErr;            
        }
        *dst = buffer + offset % SD_CARD_SECTOR_SIZE;
    }
    else if(NULL != *dst && (*dst) >= 0x30000000 && (*dst) <= 0x307FFFFF) //psram address
    {        
        // 这种情况只在wordseg_init的TEST_LEX_IN_RAM=1是会出现
        unsigned char *buf = my_mem_alloc(count * SD_CARD_SECTOR_SIZE, OS_MEM_IRAM);
        if (NULL == buf)
        {
            wordseg_log("[wordseg]my_read_sd_sectors malloc IRAM %d failed \n", count * SD_CARD_SECTOR_SIZE);
            return TC_WSErrID_ReadDataResErr;
        }                
        ret = low_sd_read_sectors(buf, sector, count);
        if (ret != 0)
        {
            wordseg_log("[wordseg]read_data low_sd_read_sectors 1 failed. ret=%d\n", ret);
            return TC_WSErrID_ReadDataResErr;
        }
        memcpy(*dst, buf + offset % SD_CARD_SECTOR_SIZE, size);        
        my_mem_free(buf);
    }
    else if(NULL != *dst && (*dst) >= 0x00080000 && (*dst) <= 0x000CFFFF) //sram address
    {        
        unsigned char *buf = (unsigned char *)(*dst);
        if (dst_size < count * SD_CARD_SECTOR_SIZE)
        {
            wordseg_log("[wordseg]read_res_in_file error. out of memory. %d, %d<%d\n", size, dst_size, count * SD_CARD_SECTOR_SIZE);
            return TC_WSErrID_ReadDataResErr;
        }
        // wordseg_log("offset=%d, size=%d, read_size=%d, sector=%d, count=%d\n", offset, size, count*SD_CARD_SECTOR_SIZE, sector, count);
        // 这种情况不是wordseg_do时调用，一般是wordseg_init调用，直接psram读sector慢点也没关系
        ret = low_sd_read_sectors(buf, sector, count);
        if (ret != 0)
        {
            wordseg_log("[wordseg]read_data low_sd_read_sectors 1 failed. ret=%d\n", ret);
            return TC_WSErrID_ReadDataResErr;
        }
        if (offset % SD_CARD_SECTOR_SIZE > 0)
        {
            memcpy(buf, buf + offset % SD_CARD_SECTOR_SIZE, size); // TF卡按照sector读的，起始位置和需要的offset可能不对
        }
    }

    return 0;
}
#if TEST_HASH_RES_IN_TF
static int read_hash_res_in_file(void *p, int offset, int size, void **dst, int dst_size)
{   
    uint32_t t1 = os_ticks_get();

    // 参数p没用

    // if (NULL == p || NULL == dst || dst_size < size) {
    //	return TC_WSErrID_InvCal;
    // }    
    int ret = my_read_sd_sectors(p, offset, size, dst, dst_size, HASH_RES_SD_CARD_OFFSET, read_sdres_buf, read_sdres_buf_size);
    //wordseg_log("read_hash_res_in_file: p=%p, offset=%d, size=%d, *dst=%p, dst_size=%d, dst_value=%d\n", p, offset, size, *dst, dst_size, ((int *)(*dst))[0]);

#if READ_DATA_RES_LOG
    uint32_t t2 = os_ticks_get();
    g_info->read_hash_res_num++;
    g_info->read_hash_res_size += size;
    g_info->read_hash_res_time += (t2 - t1);
#endif

read_data_res_in_file_end:

    return ret;
}
#endif

static int read_data_res_in_file(void *p, int offset, int size, void **dst, int dst_size)
{   
    uint32_t t1 = os_ticks_get();

    // 参数p没用

    // if (NULL == p || NULL == dst || dst_size < size) {
    //	return TC_WSErrID_InvCal;
    // }
    // wordseg_log("read_data_res_in_file: p=%p, offset=%d, size=%d, *dst=%p, dst_size=%d\n", p, offset, size, *dst, dst_size);
    int ret = my_read_sd_sectors(p, offset, size, dst, dst_size, DATA_RES_SD_CARD_OFFSET, read_sdres_buf, read_sdres_buf_size);
    //print_buf("data.bin", *dst, size);

#if READ_DATA_RES_LOG
    uint32_t t2 = os_ticks_get();
    g_info->read_data_res_num++;
    g_info->read_data_res_size += size;
    g_info->read_data_res_time += (t2 - t1);
#endif

read_data_res_in_file_end:

    return ret;
}

static void *my_mem_alloc(unsigned int size, int type)
{
    void *p = NULL;

#if 0
    p = os_mem_alloc_ext(size, type);
#else
    if (OS_MEM_IRAM == type)
    {
        p = pvPortMalloc(size);
    }
    else
    {
        p = heap_psram_malloc(size);
    }
#endif
    //wordseg_log("my_mem_alloc: size=%d, type=%s, p=%p\n", size, type == OS_MEM_IRAM ? "RAM" : "PSRAM", p);    
    return p;
}

static void my_mem_free(void *ptr)
{
    //wordseg_log("my_mem_free: p=%p\n", ptr);

#if 0
    wordseg_log("---1\n");
    os_mem_free(ptr);
    wordseg_log("---2\n");
#else
    if(ptr >= 0x30000000 && ptr <= 0x307FFFFF) //psram address
    {
        heap_psram_free(ptr);
    }
    else if(ptr >= 0x00080000 && ptr <= 0x000CFFFF) //sram address
    {        
        vPortFree(ptr);     
    }    
#endif
}

static int random_int(int min, int max) {
    srand(time(NULL)); // 设置种子
    return (rand() % (max - min + 1)) + min;
}

//简单测试裸读TF卡接口速度
//#include <time.h>
static void test_tf()
{
    int read_size = 128*1024;
    // char *buf = my_mem_alloc(256*SD_CARD_SECTOR_SIZE, OS_MEM_IRAM);    
    // int count_lst[6] ={1, 2, 3, 4, 64,128};
    char *buf = my_mem_alloc(32*SD_CARD_SECTOR_SIZE, OS_MEM_IRAM);    
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
        printk("rand%d\n");
        int t1 = os_ticks_get();
        for(int j=0; j<read_size/read_once_size; j++)
        {
             low_sd_read_sectors(buf, sector+j+100, count);      
             read_cnt[i]++;       
        }
        cost_time_lst[i] = os_ticks_get() - t1;;
    }
    wordseg_log("\nsector | count | read_cnt | size(KB) | rd_time(ms) | rd_speed(MB/s)\n");
    for(int i=0; i<sizeof(count_lst)/sizeof(count_lst[0]); i++)
    {        
        wordseg_log("%-6d | %-5d | %-8d | %-8d | %-11d | %.2f\n", SD_CARD_SECTOR_SIZE, count_lst[i], read_cnt[i], read_size/1024, cost_time_lst[i], 1000.0/cost_time_lst[i]*read_size/1024.0/1024.0);
    }
    wordseg_log("-----------------------------------------------------\n");
// ret = low_sd_read_sectors(buffer, sector, count);
    my_mem_free(buf);
}