
#ifndef __low_wifi_http_h__
#define __low_wifi_http_h__
// #include "k_data.h"

/*  
************************* 临时实现代码 **************************
* 由于 low_http 依赖陶云自己实现代码，为了不过度移植，这里暂时
* 实现依赖陶云实现的代码，后期 low_http.c 给到陶云时，
* 只需要把这些暂时实现的代码删掉，并添加陶云自己的库。
***************************************************************
*/

#include "stdint.h"

#define DATA_CHUNK_NAME_SIZE	8

typedef struct data_chunk_struct
{
	int32_t len;	//当前数据块中的数据量
	int32_t vol;	//数据块可以保存的最大数据量，单位为字节
	uint8_t *buf;	//指向数据块的数据区
	char name[DATA_CHUNK_NAME_SIZE]; //数据块的名字
}data_chunk_s, *data_chunk_t;

/*  
************************* 临时实现代码结束 ************************
*/

#define HTTP_URL_LEN        (512 + 128)
#define HTTP_VER_1_0        "1.0" 
#define HTTP_VER_1_1        "1.1" 
#define HTTP_PROTOCOL_STR   "HTTP"

#define next_str(str) (char*)(str + strlen(str) + 1)

#define HTTP_VER_NUM_1_0    0
#define HTTP_VER_NUM_1_1    1

#define HTTP_HOST_LEN       64
#define HTTP_PORT_LEN       8

typedef struct http_head_struct
{
    int32_t status_code;
    int64_t content_len;
    int32_t version;
	int32_t chunked;
    char new_url[HTTP_URL_LEN];
    char *auto_update;//用于检测固件更新
}http_header_s, *http_header_t;

enum
{
	HTTP_CHUNKED_FLAG_LEN_CRLF,			// 检索chunked的长度和CRLF
	HTTP_CHUNKED_FLAG_DATA,				// 检索chunked的数据
	HTTP_CHUNKED_FLAG_DATA_CRLF,		// 检索chunked的数据后CRLF
	HTTP_CHUNKED_FLAG_END				// chunked结束
};

typedef struct http_chunked_struct
{
	int32_t		flag;
	char		len_buf[8];
	int32_t		len_buf_index;
	int32_t		left;
	uint8_t		*data_buf;
	int32_t		data_size;
	uint8_t		rawdata_buf[4096];
}http_chunked_s, *http_chunked_t;

/*
 * 启动http get时的参数
 */
typedef struct http_get_param
{
    char *url;
    char *append_header; //应用层如果有特别的http头的话放在这里
    int64_t offset;  //下载数据的偏移
    int32_t pre_stop; //建立连接的过程中，应用层可以通过这个标志来中止连接

    int32_t timout_dns;  //dns的超时时间,暂不实现,固定由low_wifi_http.c来决定
    int32_t timout_conn; //http连接建立的超时时间,暂不实现
    int32_t timout_send; //发送请求的超时时间,暂不实现
    int32_t timout_recv; //每次recv的超时时间,已经实现
}http_get_cfg_t;

/*
 * 由平台决定的，下载结构体
 */
typedef struct http_get_struct
{
    http_get_cfg_t *cfg;     //这个必须要有!!!，用来在下载数据超时的时候检查下载有没有被应用层中止
    int socket;             //get使用的socket
    int64_t content_len;    //本次http次求，收到的头里面的content
    int64_t recv_len;       //本次连接已经接收到的content字节数
    uint8_t *data;          //recv的数据数据首先会保存到这里
    int32_t chunk_offset;   //当get的chunk中读数据的偏移
    data_chunk_s chunk;
	http_chunked_t http_chunked;	//标志http的head数据Transfer-Encoding: chunked

    int32_t https_flag;     //标志当前请求是否为https
    char host[HTTP_HOST_LEN];
    char port[HTTP_PORT_LEN];
    void *https;
    char *auto_update;//用于检测固件更新
}http_get_str_s, *http_get_str_t;

/*
 * 启动http post时的参数
 */
typedef struct http_post_parm
{
    char* url;
    char* append_header;
    uint8_t* data;  //要上传的数据
    int32_t size;   //要上传的数据量，单位为字节
    int32_t pre_stop; //建立连接过程中，应用层可以通过这个标志来中止连接
    int32_t timout;
}http_post_cfg_s, *http_post_cfg_t;

/*
 * 由平台决定的，post结构体
 */
typedef struct http_post_struct
{
    int socket;
    int32_t content_len;
    int32_t recv_len;

    uint8_t* data;
    int32_t chunk_offset;
    data_chunk_s chunk;
    http_chunked_t http_chunked;	//标志http的head数据Transfer-Encoding: chunked

    int32_t https_flag;     //标志当前请求是否为https
    char host[HTTP_HOST_LEN];
    char port[HTTP_PORT_LEN];
    void *https;
}http_post_str_s, *http_post_str_t;

typedef struct http_request_struct
{
    int64_t offset; //本次下载的数据偏移
    char *append_header; //应用层如果有特殊的http头就放到这里,connect的时候会原封不动地把所有内容加到请求头中去
}http_request_s, *http_request_t;

/*
 * http模块初始化
 */
int32_t low_http_init(void);

/*
 * http模块的逆操作
 */
int32_t low_http_term(void);

/*
 * 复位get_cfg为默认值 
 */
void low_http_get_cfg_reset(http_get_cfg_t *cfg);

/*
 * 启动一个http get连接
 * cfg[in]:
 *  启动参数
 * status_code:
 *  服务端返回的状态保存到这里
 * retval:
 *  http get连接的对象
 *  NULL:失败
 */
http_get_str_t low_http_get_connect(http_get_cfg_t *cfg, int32_t *status_code);

/*
 * 读get到的数据
 * get[in]:
 *  已经建立的http get连接对象
 * buffer[ou]:
 *  读到的数据放到这里
 * size[in]:
 *  想要读的数据量
 * retval:
 *  >0:实际读到的数据量
 *  ==0:所有数据已经读完了
 *  -1:失败
 */
int32_t low_http_get_read(http_get_str_t get, void* buffer, int32_t size);

/*
 * 断开连接
 * get[in]:
 *  已经建立的http get连接
 * retval:
 *  0:ok
 * -1:err
 */
int32_t low_http_get_disconnect(http_get_str_t get);

/*
 * 获取本次get的总数据量是多少
 * get[in]:
 *  已经建立的http get连接
 * retval:
 *  >=0:总的数据量
 *   -1:失败
 */
int64_t low_http_get_content_len(http_get_str_t get);

/*
 * 获取本次get已经收到的数据量
 * get[in]:
 *  已经建立的http get连接
 * retval:
 *  >=0:已经接收的数据量
 *   -1:失败
 */
int64_t low_http_get_recv_len(http_get_str_t get);

/*
 * 启动一个http post
 * cfg[in]:
 *  post的参数
 * status_code:
 *  服务端返回的状态保存到这里
 * retval:
 *  非0:建立成功的连接对象
 *  0: 失败
 */
http_post_str_t low_http_post_connect(http_post_cfg_t cfg, int32_t *status_code);

//可以指定content type
http_post_str_t low_http_post_connect_type(http_post_cfg_t cfg, int32_t *status_code,char* content_type);

/*
 * 读http post返回的结果
 * post[in]:
 *  已经建立的http post
 * buffer[ou]:
 *  存放读到的数据
 * size[in]:
 *  读这么多数据量
 * retval:
 *  >0:实际读到的数据
 *  ==0:数据读完了
 *  -1 :失败
 */
int32_t low_http_post_read(http_post_str_t post, void* buffer, int32_t size);

/*
 * 断开连接
 * post[in]:
 *  已经建立的http post
 */
int32_t low_http_post_disconnect(http_post_str_t post);

/*
 * 获取本次post服务端返回的总数据量是多少
 * post[in]:
 *  已经建立的http post连接
 * retval:
 *  >=0:总的数据量
 *   -1:失败
 */
int32_t low_http_post_content_len(http_post_str_t post);

/*
 * 设置http请求头中的token
 * token:
 *  NULL:清除token
 *  非NULL:设置token
 */
void low_http_header_token_set(char *token);

#endif

