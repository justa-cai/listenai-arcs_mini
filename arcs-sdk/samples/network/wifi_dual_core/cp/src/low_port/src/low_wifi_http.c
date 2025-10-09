 
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
// #include <arpa/inet.h>
#include "errno.h"

#include "string.h"
#include "plat_os.h"
// #include "http_parser27.h"

//mbedtls
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"
//#include "tls_certificate.h"

// #include "robot_cfg.h"
// #include "wifi_http.h"
#include "low_wifi.h"
// #include "hw_wifi.h"
#include "low_wifi_http.h"
// #include "k_str.h"

#include "cc/array.h"

#define DNS_LIBC    0 //使用libc的dns解析
#define DNS_PY      1 //使用python中的dns解析
#define DNS_EVDNS   2 //使用libevent做异步的dns
#define DNS2        DNS_LIBC

#define HTTP_GET                    0
#define HTTP_POST                   1
#if DNS2 == DNS_EVDNS
#define DNS_TRY_TIME                3 //如果使用的是libevent,可以控制超时时间,我们多尝试1次
#else
#define DNS_TRY_TIME                2 //连上网后的第一次访问可能会出问题,我们尝试2次
#endif
#define MAX_REQ_BUF_LEN             512

#define MAX_HOST_LEN                64
#define MAX_PORT_LEN                16
#define MAX_READ_BUFFER_SIZE        1460
#define MAX_WRITE_BUFFER_SIZE       1460
#define GET_DATA_BUF_SIZE           (MAX_READ_BUFFER_SIZE + 4)
#define POST_HEADER_GUESS_MAX_LEN    512 //post的时候我们假设http头不会超过这个值，如果后续发现有超过的情况修改这个值
#define POST_DATA_BUF_SIZE      (MAX_WRITE_BUFFER_SIZE + POST_HEADER_GUESS_MAX_LEN) //post上传和下载时使用的buf大小,要比MAX_ONCE_POST_LEN的值大

#define PREFIX_HTTP     "http://"
#define PREFIX_HTTPS    "https://"

const struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
};

static char *header_token = NULL;

#define HTTP_DNS_CACHE   1

/*  
************************* 临时实现代码 **************************
* 由于 low_http 依赖陶云自己实现代码，为了不过度移植，这里暂时
* 实现依赖陶云实现的代码，后期 low_http.c 给到陶云时，
* 只需要把这些暂时实现的代码删掉，并添加陶云自己的库。
***************************************************************
*/
static char mac_str[20];
#define USE_HEAP_APPEND_SIZE    (4 * 1024) //使用堆时不够4k用来凑4k的宏
#define PRODUCT_ID                  1177 //词典笔D1

#define http_parser_url_init(url) do { (void)(url); } while (0)
#define http_parser_parse_url(url, len, is_connect, u) (0)
#define wifi_https_send(https, data, len)    0
#define wifi_https_recv(https, data, len)    0
#define wifi_https_delete(a)                 0
#define wifi_https_create(socket, host) (NULL)

#define k_data_chunk_init(a, b, c, d)                           0
#define k_data_chunk_fill(a, b, c, d)                           0
#define k_data_chunk_read(a, b, c, d)                           0
#define k_data_chunk_len_set(a, b)                              0

enum http_parser_url_fields
  { UF_SCHEMA           = 0
  , UF_HOST             = 1
  , UF_PORT             = 2
  , UF_PATH             = 3
  , UF_QUERY            = 4
  , UF_FRAGMENT         = 5
  , UF_USERINFO         = 6
  , UF_MAX              = 7
  };

  struct http_parser_url {
    uint16_t field_set;           /* Bitmask of (1 << UF_*) values */
    uint16_t port;                /* Converted UF_PORT string */
  
    struct {
      uint16_t off;               /* Offset into buffer in which field starts */
      uint16_t len;               /* Length of run in buffer */
    } field_data[UF_MAX];
  };

  typedef struct https_obj_struct
  {
      mbedtls_entropy_context entropy;
      mbedtls_ctr_drbg_context ctr_drbg;
      mbedtls_ssl_context ssl;
      mbedtls_x509_crt cacert;
      mbedtls_ssl_config conf;
      mbedtls_net_context server_fd;
  }https_obj_t;

char* hw_wifi_mac_str(void)
{
    uint8_t mac[6];
    low_wifi_get_mac_addr(mac);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_str;
}

char *k_strdup(char *src)
{
    if(src == NULL || *src == '\0')
    {
        return NULL;
    }
    uint32_t size = strlen(src) + 4;
    char *str = os_mem_alloc(size);
    if(str == NULL)
        return NULL;
    strcpy(str, src);
    return str;
}

/*  
************************* 临时实现代码结束 ************************
*/



#if HTTP_DNS_CACHE
static os_semaphore_t dns_sema;
typedef struct
{
    char host[128];
    char port[8];
    struct sockaddr ai_addr;
    size_t ai_addrlen;
} http_dns_cache_t;

static Array *dns_cache_array = NULL;  //http_dns_cache_t
/*
 *
 * retval:
 * -1:没有找到
 * 0：找到缓存DNS
 */
static http_dns_cache_t *low_http_dns_cache_get(char *host, char *port)
{
    if (os_semaphore_get(&dns_sema,1000)) {
        printf("[low_http]dns cache get]fail to get dns sema!!\n");
        return NULL;
    }
    for(int32_t i = 0; i < array_size(dns_cache_array); i++)
    {
        http_dns_cache_t *dns_cache = array_get_for(dns_cache_array, i);
        if(!strcmp(host, dns_cache->host) && !strcmp(port, dns_cache->port))
        {
            os_semaphore_put(&dns_sema);
            return dns_cache;
        }
    }
    os_semaphore_put(&dns_sema);
    return NULL;
}

static int32_t low_http_dns_cache_add(char *host, char *port, struct sockaddr *ai_addr, size_t ai_addrlen)
{
    //防止多线程操作缓存导致结果出错
    if (os_semaphore_get(&dns_sema,1000)) {
        printf("[low_http]dns cache add]fail to get dns sema!!\n");
        return -1;
    }
    http_dns_cache_t *dns_cache = os_mem_alloc(sizeof(http_dns_cache_t));
    snprintf(dns_cache->host, 128,"%s",host);
    snprintf(dns_cache->port, 8,"%s",port);
    memcpy(&dns_cache->ai_addr, ai_addr, sizeof(struct sockaddr));
    dns_cache->ai_addrlen = ai_addrlen;
    array_add(dns_cache_array, dns_cache);
    os_semaphore_put(&dns_sema);
    return 0;
}

void low_http_dns_cache_reset(void) //可能正在使用缓存的dns，不建议调用
{
    array_destroy_cb(dns_cache_array, os_mem_free);
    dns_cache_array = NULL;
}

#endif

int32_t low_http_init(void)
{
    uint32_t tick=os_ticks_get();
    #if HTTP_DNS_CACHE
    if(os_semaphore_create(&dns_sema,"dns_sema",1))
    {
        printf("[low http]failed to create dns cache semaphore!!\n");
        return -1;
    }
    array_new(&dns_cache_array);
    #endif
    printf("[low http]init:%ums\n",os_ticks_get()-tick);
    return 0;
}

int32_t low_http_term(void)
{
    if(header_token)
    {
        os_mem_free(header_token);
        header_token = NULL;
    }
    return 0;
}

void os_tick_count_start(uint32_t *tick)
{
    *tick = os_ticks_get();
}

uint32_t os_tick_count_end(uint32_t *tick)
{
    return (os_ticks_get() - *tick);
}

void low_http_chunked_init(http_chunked_t chunked)
{
	chunked->flag = HTTP_CHUNKED_FLAG_LEN_CRLF;
	memset(chunked->len_buf, 0, sizeof(chunked->len_buf));
	chunked->len_buf_index = 0;
	chunked->left = 0;
	chunked->data_buf = NULL;
	chunked->data_size = 0;
}

int32_t low_http_chunked_parse(http_chunked_t chunked, uint8_t *buf, int32_t size)
{
	int32_t rev = 0;	// 检索数据量
	
	while (size)
	{
		printf("[%d,%d,%d]\n", chunked->flag, size, rev);
		switch (chunked->flag)
		{
			case HTTP_CHUNKED_FLAG_LEN_CRLF:
				chunked->len_buf[chunked->len_buf_index++] = *buf;
				if (*buf == '\n')
				{
					sscanf(chunked->len_buf, "%x", &chunked->left);
					printf("[low_http]chunked parse 0x%x\n", chunked->left);
					if (chunked->left != 0)
					{
						chunked->flag = HTTP_CHUNKED_FLAG_DATA;
					}
					else
					{
						chunked->flag = HTTP_CHUNKED_FLAG_END;
					}
					memset(chunked->len_buf, 0, sizeof(chunked->len_buf));
					chunked->len_buf_index = 0;
				}
				rev++;
				buf++;
				size--;
				break;
			case HTTP_CHUNKED_FLAG_DATA:
				size = chunked->left < size ? chunked->left : size;
				chunked->left -= size;
				if (chunked->left == 0)
				{
					chunked->flag = HTTP_CHUNKED_FLAG_DATA_CRLF;
				}
				chunked->data_buf = buf;
				chunked->data_size = size;
				rev += size;
				buf += size;
				size = 0;		// 跳出循环
				break;
			case HTTP_CHUNKED_FLAG_DATA_CRLF:
				chunked->data_buf = NULL;
				chunked->data_size = 0;
				if (*buf == '\n')
				{
					chunked->flag = HTTP_CHUNKED_FLAG_LEN_CRLF;
				}
				rev++;
				buf++;
				size--;
				break;
			case HTTP_CHUNKED_FLAG_END:
				rev++;
				buf++;
				size--;
				break;
			default:
				break;
		}
	}
	return rev;
}

/*
 * 检查一个url是否为https
 * retval:
 *  1:https
 *  0:http
 */
int32_t url_https_chk(char *url)
{
    if(strncmp(url, PREFIX_HTTPS, strlen(PREFIX_HTTPS)) == 0)
    {
        return 1;
    }
    return 0;
}

/*
 * 由url生成对应的http请求
 * url:
 * body:
 *  如果是post的话，post的数据量很小时这里可以存放要post的数据 
 *  然后把要post的数据和头一起一次性地发送给服务器
 * body_len:
 *  如果是post的话，这里存放要post的数据的总长度
 * request:
 *  存放生成的请求数据
 * message_type:
 *  HTTP_GET/HTTP_POST 
 * req:
 *  应用层调用http的时候，可能会有一些额外的控制信息，放在这里
 * retval:
 * >0: 生成的http请求的长度
 */
int32_t url2req(char* url, uint8_t *body, int32_t body_len, char *request, 
    uint8_t message_type, http_request_t req)
{
int32_t tmp;
char *phost, *path;
char buf[64];
char* myurl = NULL;
myurl = os_mem_alloc(HTTP_URL_LEN + USE_HEAP_APPEND_SIZE); //从psram中分配

if(myurl == NULL)
{
    printf("[low_http]url2req alloc myurl NULL\n");
    while(1); //目前我们认为这个时候不会分配不到内存， 如果发生了，就死在这里。
}
sprintf(myurl, "%s", url?:"");

if(message_type == HTTP_GET)
{
    strcat(request,"GET ");
}
else 
{
    strcat(request,"POST ");
}

tmp = 0;
if(strncmp(myurl, PREFIX_HTTP, strlen(PREFIX_HTTP)) == 0)
{
    tmp = strlen(PREFIX_HTTP);
}
else if(strncmp(myurl, PREFIX_HTTPS, strlen(PREFIX_HTTPS)) == 0)
{
   tmp = strlen(PREFIX_HTTPS);
}
if(tmp)
{
    memmove(myurl, myurl + tmp, strlen(url) - tmp);
    memset(myurl + strlen(url) - tmp, 0, tmp);
}

for(phost = myurl; *phost != '/' && *phost != '\0'; ++phost);
if((int)(phost - myurl) == strlen(myurl))
{   
    sprintf(buf, "%s", "/");
    //sprintf(buf, "%s", url?:"");
    path = buf;
}
else
{   
    path = phost;
}
strcat(request, path);

*phost = '\0';
strcat(request, " HTTP/1.1\r\n");
strcat(request, "Host: ");
strcat(request, myurl);
strcat(request, "\r\n");

snprintf(buf, 64, "Device-Unique: %s\r\n", hw_wifi_mac_str()?:"");
strcat(request, buf);
sprintf(buf, "product-type: %d\r\n", PRODUCT_ID);
strcat(request, buf);
sprintf(buf, "device-type: %d\r\n", PRODUCT_ID);
strcat(request, buf);

if(req != NULL)
{
    if(req->offset > 0)
    {
        sprintf(buf, "Range: bytes=%lld-\r\n", req->offset);
        strcat(request, buf);
    }
    if(req->append_header)
    {
        strcat(request, req->append_header);
    }
}

if((message_type == HTTP_POST) && strstr(request, "Content-Type:") == NULL)
{
    strcat(request, "Content-Type: application/x-www-form-urlencoded\r\n");
}
if(strstr(url,"https:")&&header_token) //http不带token，https的才能带token
{
    char token[128];
    snprintf(token, 128, "token: %s\r\n", header_token?:"");
    strcat(request, token);
}

if(body_len > 0) //如果设置了content长度，这里要增加Content-Length
{
    sprintf(buf, "%d", body_len);
    strcat(request, "Content-Length: ");
    strcat(request, buf);
    strcat(request, "\r\n");
}

strcat(request,"\r\n");
#if 0
printf("[low_http]url2req %s:"
        "\n++++++++++++++++++++++++\n%s++++++++++++++++++++++++\n", 
        message_type == HTTP_GET ? "GET" : "POST", request);
#endif

tmp = strlen(request);
if (body != NULL) //调用url2req的时候会保证这里的body不会超过request的大小
{
    memmove(&request[tmp], body, body_len);
    tmp = tmp + body_len;
}
os_mem_free(myurl);
return tmp;
}

//可以指定type
int32_t url2req_type(char* url, uint8_t *body, int32_t body_len, char *request, 
    uint8_t message_type, http_request_t req,char* content_type)
{
int32_t tmp;
char *phost, *path;
char buf[64];
char* myurl = NULL;
myurl = os_mem_alloc(HTTP_URL_LEN + USE_HEAP_APPEND_SIZE); //从psram中分配

if(myurl == NULL)
{
    printf("[low_http]url2req alloc myurl NULL\n");
    while(1); //目前我们认为这个时候不会分配不到内存， 如果发生了，就死在这里。
}
sprintf(myurl, "%s", url?:"");

if(message_type == HTTP_GET)
{
    strcat(request,"GET ");
}
else 
{
    strcat(request,"POST ");
}

tmp = 0;
if(strncmp(myurl, PREFIX_HTTP, strlen(PREFIX_HTTP)) == 0)
{
    tmp = strlen(PREFIX_HTTP);
}
else if(strncmp(myurl, PREFIX_HTTPS, strlen(PREFIX_HTTPS)) == 0)
{
   tmp = strlen(PREFIX_HTTPS);
}
if(tmp)
{
    memmove(myurl, myurl + tmp, strlen(url) - tmp);
    memset(myurl + strlen(url) - tmp, 0, tmp);
}

for(phost = myurl; *phost != '/' && *phost != '\0'; ++phost);
if((int)(phost - myurl) == strlen(myurl))
{   
    sprintf(buf, "%s", "/");
    //sprintf(buf, "%s", url?:"");
    path = buf;
}
else
{   
    path = phost;
}
strcat(request, path);

*phost = '\0';
strcat(request, " HTTP/1.1\r\n");
strcat(request, "Host: ");
strcat(request, myurl);
strcat(request, "\r\n");

snprintf(buf, 64, "Device-Unique: %s\r\n", hw_wifi_mac_str()?:"");
strcat(request, buf);
sprintf(buf, "product-type: %d\r\n", PRODUCT_ID);
strcat(request, buf);
sprintf(buf, "device-type: %d\r\n", PRODUCT_ID);
strcat(request, buf);

if(req != NULL)
{
    if(req->offset > 0)
    {
        sprintf(buf, "Range: bytes=%lld-\r\n", req->offset);
        strcat(request, buf);
    }
    if(req->append_header)
    {
        strcat(request, req->append_header);
    }
}

if(content_type)
{
    strcat(request, "Content-Type: ");
    strcat(request,content_type);
    strcat(request,"\r\n");
}
else if((message_type == HTTP_POST) && strstr(request, "Content-Type:") == NULL) //default type
{
    strcat(request, "Content-Type: application/x-www-form-urlencoded\r\n");
}

if(body_len > 0) //如果设置了content长度，这里要增加Content-Length
{
    sprintf(buf, "%d", body_len);
    strcat(request, "Content-Length: ");
    strcat(request, buf);
    strcat(request, "\r\n");
}

strcat(request,"\r\n");

tmp = strlen(request);
if (body != NULL) //调用url2req的时候会保证这里的body不会超过request的大小
{
    memmove(&request[tmp], body, body_len);
    tmp = tmp + body_len;
}
os_mem_free(myurl);
return tmp;
}


/*
 * 从url中解析出host和port
 * host:
 *  存放解析出来的域名
 * port:
 *  存放解析出来的商品，如果没有设置的话，http使用80,https使用443
 * url:
 * retval:
 *  0:ok
 * -1:failed
 */
static int32_t low_http_fetch_host_and_port(char *host, char *port, char *url, 
    int32_t is_https)
{
struct http_parser_url u;

http_parser_url_init(&u);
if (0 != http_parser_parse_url(url, strlen(url), 0, &u))
{
    printf("[low_http]get_connect parse url error\n");
    return -1;
}
if(host)
{
    snprintf(host, u.field_data[UF_HOST].len + 1, "%s", 
            (&(url[u.field_data[UF_HOST].off]))?:"");
}
if(u.field_data[UF_PORT].len != 0 && port)
{
    snprintf(port, u.field_data[UF_PORT].len + 1, "%s",
            (&(url[u.field_data[UF_PORT].off]))?:"");
}
else if(is_https && port)
{
    sprintf(port, "%s", "443");
}
else if(port)
{
    sprintf(port, "%s", "80");
}
return 0;
}

#if DNS2 == DNS_PY //使用python中的dns解析函数
extern int getaddrinfo2(const char*hostname, const char*servname,
    const struct addrinfo *hints, struct addrinfo **res);
extern char * gai_strerror2(int ecode);
extern void freeaddrinfo2(struct addrinfo *ai);
#elif DNS2 == DNS_EVDNS //使用libevent来做dns解析
#include <event2/event.h>
#include <event2/dns.h>
#include <event2/dns_struct.h>
#include <event2/util.h>
int32_t libevent_getaddrinfo(const char *hostname, const char *servname,
            const struct addrinfo *hints_in, struct addrinfo **res, int32_t *pre_stop);
#endif
void low_wifi_http_dns_serv_chg(void);

/*
 * 域名解析 
 * host:
 * port:
 * res:
 *  解析到的结果存放在这里
 * pre_stop:
 *  get或者port中的cfg->pre_stop,用来控制中止操作的
 * type:
 *  只是用来打印的，从log可以看出本次dns是为get还是post的
 * retval:
 *  0:success
 * -1:failed
 */
static int32_t low_http_dns(char *host, char *port, struct addrinfo **res, struct addrinfo **res_head,
    int32_t *pre_stop, char *type)
{
uint32_t tick;
int32_t err, dns_cnt;
dns_cnt = 0;
*res=NULL,*res_head=NULL;
tick = os_ticks_get();
while(1)
{
#if DNS2 == DNS_PY
    err = getaddrinfo2(host, port, &hints, res);
#elif DNS2 == DNS_EVDNS
    err = libevent_getaddrinfo(host, port, &hints, res, pre_stop);
#else
    err = getaddrinfo(host, port, &hints, res);
#endif
    if(err != 0 || *res == NULL)
    {
#if DNS2 == DNS_PY
        printf("[low_http]%s DNS failed err = %s res = %p\n", type?:"", gai_strerror2(err)?:"", *res);
#elif DNS2 == DNS_EVDNS
        printf("[low_http]%s DNS failed err = %s res = %p\n", type?:"", evutil_gai_strerror(err)?:"", *res);
#else
        printf("[low_http]%s DNS failed err = %d res = %p\n", type?:"", err, *res);
#endif
        if(*res)
        {
    #if DNS2 == DNS_PY
            freeaddrinfo2(*res);
    #elif DNS2 == DNS_EVDNS
            evutil_freeaddrinfo(*res);
    #else
            freeaddrinfo(*res);
    #endif
            *res=NULL;
            printf("[low_http] dns]free res\n");
        }
        if(*pre_stop)
        {
            printf("[low_http]%s pre_stop when get DNS\n", type?:"");
            return -1;
        }
        if(++dns_cnt >= DNS_TRY_TIME)
        {
            return -1;
        }
        low_wifi_http_dns_serv_chg();
        os_thread_sleep(200);
    }
    else
    {
        struct addrinfo *rp;
        int32_t sfd=0;
        for (rp=*res;rp!=NULL;rp=rp->ai_next)
        {
            sfd=socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
            if(sfd==-1) 
                continue;
            if(bind(sfd,rp->ai_addr,rp->ai_addrlen)==0) 
            {
                close(sfd);
                break;
            }
            close(sfd);
        }
        if(rp)
        {
            *res_head=*res;
            *res=rp;
            tick = os_ticks_get() - tick;
            //if(tick>100)
            {
                struct in_addr addr = ((struct sockaddr_in *)(*res)->ai_addr)->sin_addr;
                printf("[low_http]%s DNS success IP=%s cnt:%d used:%d,res==res_head:%d\n", type?:"", inet_ntoa(addr)?:"", dns_cnt, tick,*res==*res_head);
            }
            break;
        }
        else 
        {
            printf("[low http]dns got res but fail to bind,try cnt:%d\n",dns_cnt);
            if(*res)
            {
        #if DNS2 == DNS_PY
                freeaddrinfo2(*res);
        #elif DNS2 == DNS_EVDNS
                evutil_freeaddrinfo(*res);
        #else
                freeaddrinfo(*res);
        #endif
                *res=NULL;
                printf("[low_http] dns]free res\n");
            }
        }
        if(++dns_cnt >= DNS_TRY_TIME)
        {
            return -1;
        }
    }
}
return 0;
}

#if 0
#define SO_CONTIMEO             0
#define SO_SNDTIMEO             1
#define SO_RCVTIMEO             2
#endif

#define CONNECT_RETRY_TIM       2 //connect失败时的重试次数
#define SEND_RETRY_TIM          3 //send失败时的重试次数
#define RECV_RETRY_TIM          3 //recv失败时的重试次数

#define TIMOUT_NUM_CON      3
#define TIMOUT_NUM_SEND     3
#define TIMOUT_NUM_RECV     5

const int32_t timout_buf_con[]  = {1, 2, 4};
const int32_t timout_buf_send[] = {1, 2, 3};
const int32_t timout_buf_recv[] = {1, 2, 3, 3, 3};

static int32_t low_http_timout_set(int socket, int32_t type, int32_t* index)
{
    struct timeval tv;
    int32_t rev, tmp;
    tv.tv_usec = 0;
    if((uint32_t)index <= 64)     //传入的不是偏移量的指针，而是时间强转的指针, 直接设定超时时间
    {
        tv.tv_sec = (uint32_t)index;
        goto low_wifi_set_socket_timeout;
    }
    //printf("[low http]timeout set index>64,type:%d,index:%u\n",type,index);
    tmp = *index;
    switch(type)
    {
        case SO_CONTIMEO:
            return 0;
            tv.tv_sec = timout_buf_con[tmp++];
            if(tmp < TIMOUT_NUM_CON)
                *index = tmp;
            break;
        case SO_SNDTIMEO:
            tv.tv_sec = timout_buf_send[tmp++];
            if(tmp < TIMOUT_NUM_SEND)
                *index = tmp;
            break;
        case SO_RCVTIMEO:
            tv.tv_sec = timout_buf_recv[tmp++];
            if(tmp < TIMOUT_NUM_RECV)
                *index = tmp;
            break;
        default:
            printf("[low_http]timout_set err type:0x%x\n", type);
            return -1;
    }
low_wifi_set_socket_timeout:
    rev = setsockopt(socket, SOL_SOCKET, type, &tv, sizeof(tv));
    if(rev != 0)
    { //这里不能打印*index,index可能是强转的数字，*index是一个不存在的值
        printf("[low_http]timout_set:%d type:0x%x sec:%ld failed:%d\n",
                socket, type, tv.tv_sec, rev);
        return -1;
    }
    //printf("[low http]timeout set end,index:%u\n",index);
    return 0;
}

/*
 * 计算尝试多少次操作会达到total这么长的超时时间
 * type : SO_CONTIMEO/SO_SNDTIMEO/SO_RCVTIMEO
 * total:想要的超时时长
 * index:当前使用的timout_buf_xxx[]的索引
 * retval:
 *  要尝试的次数据
 */
static int32_t low_http_try_cnt_calc(int32_t type, int32_t total, int32_t index)
{
    const int32_t *timout_buf;
    int32_t tmp, index_max, try_cnt;
    switch(type)
    {
        case SO_CONTIMEO:
            timout_buf = timout_buf_con;
            index_max = TIMOUT_NUM_CON;
            break;
        case SO_SNDTIMEO:
            timout_buf = timout_buf_send;
            index_max = TIMOUT_NUM_SEND;
            break;
        case SO_RCVTIMEO:
            timout_buf = timout_buf_recv;
            index_max = TIMOUT_NUM_RECV;
            break;
        default:
            timout_buf = timout_buf_recv;
            index_max = TIMOUT_NUM_RECV;
            break;
    }
    tmp = 0;
    try_cnt = 0;
    while(tmp < total)
    {
        if(index >= index_max)
            index = 0;
        tmp += timout_buf[index++];
        try_cnt++;
    }
    printf("---------->[low_http]try_cnt_calc want time:%d, try_cnt:%d, real_time:%d\n",
            total, try_cnt, tmp);
    return try_cnt;
}

/*
 * 作socket的connect操作,失败的话会重试，重试次数由CONNECT_RETRY_TIM决定
 * socket_ptr:
 *  链接使用的的socket的地址，如果连接失败会换一个socket重试连接,
 *  会修改传过来的socket值,所以这里要用指针
 * addr:
 * addrlen:
 *  用来确定要连的服务器地址
 * pre_stop:
 *  连接慢的话，应用层有可能会中途中卡操作，通过这个变量传递
 * type:
 *  只是用来打印本次连接是post还是get 
 * retval:
 *  0:ok
 * -1:failed
 */
static int32_t low_http_connect(int *socket_ptr, struct sockaddr *addr, 
    socklen_t addrlen, int32_t *pre_stop, char *type)
{
    int32_t try_cnt, rev, timout_index, socket_id;
    int32_t error = 0, err_len = 0;
    int32_t ret = 0;
    int32_t rc = 0;
    int32_t old_f = 0;
    struct timeval tm;
    fd_set set_w;
    fd_set set_r;

    socket_id = *socket_ptr;
    timout_index = 0;
    try_cnt = 0;
low_http_connect_loop:
    error = 0;
    err_len = 4;
#if 1
    rc = 0;
    ret = 0;
    if (timout_index >= TIMOUT_NUM_CON) {
        timout_index = TIMOUT_NUM_CON - 1;
    }
    tm.tv_sec = timout_buf_con[timout_index++];
    tm.tv_usec = 0;

    old_f = fcntl(socket_id, F_GETFL,0);
    if (-1 == fcntl(socket_id, F_SETFL, old_f | O_NONBLOCK)) {
        printf("[low_http]connect set nonblock fail\n");
    }

    uint32_t ticks = os_ticks_get();
    ret = connect(socket_id, addr, addrlen);
    ticks = os_ticks_get() - ticks;
    if (ret != 0) {
        if (errno != EINPROGRESS) {
            printf("[low_http]connect fail at once\n");
            rev = -1;
        } else {
            FD_ZERO(&set_w);
            FD_ZERO(&set_r);
            FD_SET(socket_id, &set_w);
            FD_SET(socket_id, &set_r);
            int32_t ticks_2 = os_ticks_get();
            rc = select(socket_id + 1, &set_r, &set_w, NULL, &tm);
            ticks_2 = os_ticks_get() - ticks_2;
            if (rc < 0) {
                printf("[low_http]connect select fail:%d\n", rc);
                rev = -1;
            } else if (rc == 0) {
                printf("[low_http]connect select timeout\n");
                rev = -1;
            } else {
                int32_t err_ret = getsockopt(socket_id, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&err_len);
                if ((1 == rc) && (FD_ISSET(socket_id, &set_w))) {
                    // printf("[low_http]connect select success\n");
                    rev = 0;
                } else {
                    printf("[low_http]connect select fail\n");
                    rev = -1;
                }
            }
            // if(rev||ticks+ticks_2>300)
            printf("[low_http]connect cnt:%d err:%d ticks:%d ticks2:%d\n", rc, error, ticks, ticks_2);
        }
    } else {
        printf("[low_http]connect sucess at once ticks:%d\n", ticks);
        rev = 0;
    }
    if (fcntl(socket_id, F_SETFL, old_f) == -1) {
        printf("[low_http]connect set block fail\n");
        rev = -1;
    }
#else
    rev = connect(socket_id, addr, addrlen);
#endif

    if (rev != 0) {
        if ((++try_cnt >= CONNECT_RETRY_TIM) || (*pre_stop != 0)) {
            printf("[low_http]%s socket:%d failed:%d err:%s pre_stop:%d\n", type ?: "", socket_id, rev,
                   strerror(errno) ?: "", *pre_stop);
            return -1;
        }
        error = 0;
        err_len = 4;
        if (rc > 0) {
            getsockopt(socket_id, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&err_len);
            printf("[low_http]%s errno=%d err:%d retry:%d\n", type ?: "", errno, error, try_cnt);
        } else {
            printf("[low_http]%s err, connecting,can't get errcode, retry:%d\n", type ?: "", try_cnt);
        }
        close(socket_id);
        socket_id = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_id >= 0) {
            printf("[low_http]%s change socket old:%d new:%d\n", type ?: "", *socket_ptr, socket_id);
            *socket_ptr = socket_id;
            goto low_http_connect_loop;
        }
        *socket_ptr = -1;
        printf("[low_http]%s change new socket failed\n", type ?: "");
        return -1;
    }
    return 0;
}

/*
 * socket的发送操作
 *  socket_id:
 *  data:
 *  len:
 * try_max:
 *  出错的话，会进行重试操作，用来控制最多执行多少次send操作
 * timout:
 *  初始的超时序号,传进来的值需要被修改，所以这里使用指针
 * pre_stop:
 * type:
 * https:
 *  如果链接是https的话，这个结构体保存的是https需要使用的
 * retval:
 *>=0:实际发送的数据量
 * -1:发送被中止了
 * -2:failed
 *   :如果使用https的话，会是别的错误值
 */
static int32_t low_http_send(int32_t socket_id, uint8_t *data, int32_t len, int32_t try_max, int32_t *timout,
                             int32_t *pre_stop, char *type, https_obj_t *https)
{
    int32_t rev, timout_index, try_cnt;
    uint32_t tick;
    try_cnt = 0;
    if (timout == NULL) {
        timout_index = 0;
        timout = &timout_index;
    }
low_http_send_loop:
    low_http_timout_set(socket_id, SO_SNDTIMEO, timout);
    os_tick_count_start(&tick);
    if (https != NULL) {
        rev = wifi_https_send(https, data, len);
    } else {
        rev = send(socket_id, data, len, 0);
    }
    if (rev < 0) {
        printf("[low_http]%s send:%d rev:%d used:%d\n", type ?: "", len, rev, os_tick_count_end(&tick));
        if (*pre_stop) {
            return -1;
        }
        if (++try_cnt < try_max) {
            printf("[low_http]%s send retry:%d\n", type ?: "", try_cnt);
            goto low_http_send_loop;
        }
    }
    return rev;
}

/*
 * socket的接收操作
 * socket_id:
 * data:
 * len:
 *  本次最大接收数据量 
 * try_max:
 *  出错的话，会进行重试操作，用来控制最多执行多少次recv操作
 * timout:
 *  初始的超时序号,传进来的值需要被修改，所以这里使用指针
 * pre_stop:
 * type:
 * https:
 *  如果链接是https的话，这个结构体保存的是https需要使用的
 * retval:
 *>=0:实际接收到的数据量
 * -1:发送被中止了
 * -2:failed,
 *   :如果是https的话，会是别的负的错误值
 */
static int32_t low_http_recv(int32_t socket_id, uint8_t *data, int32_t len, int32_t try_max, int32_t *timout,
                             int32_t *pre_stop, char *type, https_obj_t *https)
{
    int32_t rev, try_cnt;
    uint32_t tick;
    try_cnt = 0;
low_http_recv_loop:
    low_http_timout_set(socket_id, SO_RCVTIMEO, timout);
    os_tick_count_start(&tick);
    if (https != NULL) {
        rev = wifi_https_recv(https, data, len);
    } else {
        rev = recv(socket_id, data, len, 0);
    }
    if (rev < 0) {
        printf("[low_http]%s recv:%d rev:%d used:%d\n", type ?: "", len, rev, os_tick_count_end(&tick));
        if (*pre_stop) {
            return -1;
        }
        if (++try_cnt < try_max) {
            printf("[low_http]%s recv retry:%d\n", type ?: "", try_cnt);
            goto low_http_recv_loop;
        }
    }
    return rev;
}

/*
 * 申请一个http_get_str_t对象
 * retval:
 * NULL:申请失败,要么是没有内存了要么是申请socket失败
 * !NULL:指向申请成功的http_get_str_t
 */
static http_get_str_t low_http_get_obj_malloc(void)
{
    int32_t try_cnt;
    http_get_str_t get;
    get = (http_get_str_t)os_mem_alloc(sizeof(http_get_str_s));
    if(get == NULL)
    {
        printf("[low_http]get_obj_malloc NULL0\n");
        return NULL;
    }
    memset(get, 0, sizeof(http_get_str_s));
    get->data = (uint8_t*)os_mem_alloc(GET_DATA_BUF_SIZE + USE_HEAP_APPEND_SIZE); //凑够4k,使用psram
    if(get->data == NULL)
    {
        os_mem_free(get);
        printf("[low_http]get_obj_malloc NULL1\n");
        return NULL;
    }
    try_cnt = 5;
low_http_get_obj_malloc_socket_loop:
    get->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(get->socket < 0)
    {
        if(try_cnt > 0)
        {
            printf("[low_http]get_obj_malloc get socket failed:%d\n", try_cnt--);
            os_thread_sleep(20);
            goto low_http_get_obj_malloc_socket_loop;
        }
        os_mem_free(get->data);
        os_mem_free(get);
        printf("[low_http]get_obj_malloc socket failed\n");
        return NULL;
    }
	get->http_chunked = NULL;
    return get;
}

/*
 * 释放一个http_get_str_t对象
 */
static int32_t low_http_get_obj_free(http_get_str_t get)
{
    if(get == NULL)
    {
        return 0;
    }
    if(get->socket >= 0)
    {
        close(get->socket);
    }
    if(get->data)
    {
        os_mem_free(get->data);
        get->data = NULL;
    }
	if(get->http_chunked)
	{
		os_mem_free(get->http_chunked);
		get->http_chunked = NULL;
	}
    if(get->https) //如果有https的话，这时要释放掉
    {
        wifi_https_delete(get->https);
    }
    if(get->auto_update)
    {
        os_mem_free(get->auto_update);
        get->auto_update = NULL;
    }
    os_mem_free(get);
    return 0;
}

static http_post_str_t low_http_post_obj_malloc(void)
{
    http_post_str_t post;
    post = (http_post_str_t)os_mem_alloc(sizeof(http_post_str_s));
    if(post == NULL)
    {
        printf("[low_http]post_obj_malloc NULL0\n");
        return NULL;
    }
    memset(post, 0, sizeof(http_post_str_s));
    post->data = (uint8_t*)os_mem_alloc(POST_DATA_BUF_SIZE + USE_HEAP_APPEND_SIZE); //凑够4k,使用psram
    if(post->data == NULL)
    {
        os_mem_free(post);
        printf("[low_http]post_obj_malloc NULL1\n");
        return NULL;
    }
    post->socket = socket(AF_INET, SOCK_STREAM, 0);
    if(post->socket < 0)
    {
        os_mem_free(post->data);
        os_mem_free(post);
        printf("[low_http]post_obj_malloc socket failed\n");
        return NULL;
    }
    post->http_chunked = NULL;
    return post;
}

static int32_t low_http_post_obj_free(http_post_str_t post)
{
    if(post == NULL)
        return 0;
    if(post->socket >= 0)
        close(post->socket);
    if(post->data)
        os_mem_free(post->data);
    if(post->http_chunked)
        os_mem_free(post->http_chunked);
    if(post->https != NULL) //如果有https的话这里要释放掉
        wifi_https_delete(post->https);
    os_mem_free(post);
    return 0;
}

/*
 * 检查一个字符串中是是否包含http响应的头结束符
 * 结束符是两个"\r\n\r\n"
 * retval:
 *  0:没有结束符
 *　1:有结束符
 */
static int32_t low_http_has_respond_head_end(char *resp)
{
    if(strstr(resp, "\r\n\r\n"))
    {
        return 1;
    }
    return 0;
}

/*
 * 从返回的http数据中找我们关心的http头信息,这个函数会修改输入的数据
 * header:
 *  找到的信息放到这个头里面去
 * buf:
 *  从这个buf里面检查
 * len:
 *  buf的长度 
 * retval:http头中信息数据的长度
 *  -1:err
 */
static int low_http_header_info_load(http_header_t header, char* buf, int32_t len)
{
    char *ptr, *str, *delim, *key_str, *val_str;
    int32_t info_len;
    int64_t temp;
    //printf("[low_http]header_info_load start len:%d\n", len);
    //printf("%s\n", buf);
    info_len = 0;
    str = buf;
    ptr = strstr(str, "\r\n\r\n");
    if(!ptr)
    {
        printf("[low_http]header_info_load err: no header end!\n");
        return -1;
    }
    info_len = ptr - buf;
    info_len += strlen("\r\n\r\n");
    
    while(1)
    {
        delim = strchr(str, ':');
        if(!delim)
        {
            break;
        }
        *delim = '\0';
        key_str = str;
        str = next_str(str) + 1;
        delim = strstr(str, "\r\n");
        *delim = '\0';
        val_str = str;
        //printf("[low_http]head_info %s=%s\n", key_str?:"", val_str?:"");
        if(strcasecmp(key_str, "Content-Length") == 0)
        {
            temp = atoll(val_str);
            //printf("[low_http]head_info------------>content len:%lld\n", temp);
            header->content_len = temp;
        }
        else if(strcasecmp(key_str, "Location") == 0)
        {
            sprintf(header->new_url, "%s", val_str?:"");
        }
		else if(strcasecmp(key_str, "Transfer-Encoding") == 0)
		{
			if (strcasecmp(val_str, "chunked") == 0)
			{
				header->chunked = 1;
			}
		}
        else if(strcasecmp(key_str, "X-Allow-AutoUpdate") == 0)
        {
            if(val_str)
            {
                header->auto_update = k_strdup(val_str);
                //printf("\n[low_http]allow autoupdate:%s\n",val_str?:"");
            }
        }

        str = next_str(str) + 1;
        if((*str == '\r') && (*(str + 1) == '\n'))
        {
            //printf("[low_http]head_info find end\n");
            break;
        }
    }
    return info_len;
}

/*
 * 返回数据的起始偏移,也就是http整个头的长度
 * retval:整个http头的长度
 *  -1:err
 *  >0:http头的长度
 */
static int low_http_header_parse(http_header_t header, char* buf, int32_t len)
{
    char *http_ver, *status_str, *substr;
    char *str = buf;
    char *delim, *ptr;
    int32_t info_offset;
    http_ver = status_str = substr = ptr = NULL;
    //printf("[low_http]header_parse start len:%d\n%s\n", len, buf);
    delim = strchr(str, '/');
    if(delim)
    {
        *delim = '\0';
        ptr = str;
    }
    if(!ptr || strcasecmp(ptr, HTTP_PROTOCOL_STR))
    {
        if(ptr)
        {
            printf("[low_http]header protocol mismatch:%s\n", ptr?:"");
            printf("[low_http]header_parse len:%d\n%s\n", len, buf?:"");
        }
        else
        {
            printf("[low_http]header protocol mismatch maybe null:%s\n", str?:"");
        }
        return -1;
    }
    //printf("[low_http]header protocol:%s\n", str?:"");
    str = next_str(str);
    delim = strchr(str, ' ');
    if(delim)
    {
        *delim = '\0';
        http_ver = str;
    }
    //printf("[low_http]header ver:%s\n", str?:"");
    if(!http_ver)
    {
        printf("[low_http]header protocol version not found\n");
        return -1;
    }
    else
    {
       if(!strcasecmp(http_ver, HTTP_VER_1_0))
       {
           header->version = HTTP_VER_NUM_1_0;
       }
       else if(!strcasecmp(http_ver, HTTP_VER_1_1))
       {
           header->version = HTTP_VER_NUM_1_1;
       }
       else
       {
           printf("[low_http]header protocol version mismatch\n");
           return -1;
       }
    }
    str = next_str(http_ver);
    delim = strchr(str, ' ');
    if(delim)
    {
        *delim = '\0';
        status_str = str;
    }
    if(!status_str)
    {
        printf("[low_http]header status code not found\n");
        return -1;
    }
    //printf("[low_http]header status code:%s\n", status_str?:"");
    header->status_code = strtol(status_str, NULL, 10);
    str = next_str(status_str);
    substr = strstr(str, "\r\n");
    if(substr)
    {
        *substr = '\0';
        //printf("[low_http]header phrase:%s\n", str?:"");
    }
    else
    {
        printf("[low_http]header status code string not found\n");
        return -1;
    }
    str = next_str(str) + 1;
    info_offset = str - buf; //头信息的偏移
    //printf("[low_http]header head len:%d\n", info_offset);
    len = len - info_offset; //剩下的长度
    len = low_http_header_info_load(header, str, len);
    if(len >= 0)
    {
        len += info_offset;
        //printf("[low_http]header total header len:%d\n", len);
        return len;
    }
    return -1;
}

void low_http_get_cfg_reset(http_get_cfg_t *cfg)
{
    cfg->url         = NULL;
    cfg->append_header = NULL;
    cfg->offset      = 0;
    cfg->pre_stop    = 0;
    cfg->timout_conn = 0;
    cfg->timout_send = 0;
    cfg->timout_recv = 0;
    cfg->timout_dns  = 0;
}

http_get_str_t low_http_get_connect(http_get_cfg_t *cfg, int32_t *status_code)
{
    http_get_str_t get;
    http_header_t header;
    http_request_s req;
    int get_socket, rev, rb;
    char *url, *request_buf, *host, *port;
    int32_t try_cnt, timout_index, recv_timout_index, recv_try_cnt;
    uint32_t tick;

	struct in_addr host_addr;
    struct addrinfo *res = NULL, *res_head = NULL;
    if(!cfg){
        return NULL;
    }

    get = low_http_get_obj_malloc();
    if(get == NULL)
    {
        printf("[low_http]get_connect obj malloc failed\n");
        return NULL;
    }
    host        = get->host;
    port        = get->port;
    get_socket  = get->socket;
    request_buf = (char*)get->data;
    header      = NULL;
    url         = cfg->url;
    req.offset  = cfg->offset;
    req.append_header = cfg->append_header;
    recv_timout_index = 0; //接收第一份数据时我们重新建连接，要在这里把参数设置好
    recv_try_cnt      = 3; //重试3次

low_http_get_connect_loop:
    if(get->https != NULL) //可能是出错之后跳转到这里的，要把出错的https释放掉
    {
        wifi_https_delete(get->https);
        get->https = NULL;
    }
    get->https_flag     = url_https_chk(url);
    memset(request_buf, 0, MAX_READ_BUFFER_SIZE);
    url2req(url, NULL, 0, request_buf, HTTP_GET, &req);

    if(low_http_fetch_host_and_port(host, port, url, get->https_flag))
    {
        goto low_http_get_connect_failed;
    }
    printf("[low_http]get_connect host:%s port:%s\n", host?:"", port?:"");
    //dns
    os_tick_count_start(&tick); 

    struct sockaddr *ai_addr;
    size_t ai_addrlen;
#if HTTP_DNS_CACHE
    http_dns_cache_t *dns_cache = low_http_dns_cache_get(host, port);
    if(dns_cache)
    {
        //printf("---------->[low_http]get_connect getaddr info cache\n");
        ai_addr = &dns_cache->ai_addr;
        ai_addrlen = dns_cache->ai_addrlen;
    }
    else
#endif
    {
        if(low_http_dns(host, port, &res, &res_head, &cfg->pre_stop, "get_connect") != 0)
        {
            goto low_http_get_connect_failed;
        }
        if(!res)
        {
            printf("[low http]get dns but res is NULL!\n");
            goto low_http_get_connect_failed;
        }
        ai_addr = res->ai_addr;
        ai_addrlen = res->ai_addrlen;
#if HTTP_DNS_CACHE
        low_http_dns_cache_add(host, port, ai_addr, ai_addrlen);
#endif
    }
    host_addr = ((struct sockaddr_in *)ai_addr)->sin_addr;
    //if(os_tick_count_end(&tick)>100)
        printf("[low_http]get_connect getaddr info used:%d\n", os_tick_count_end(&tick));
    //connect
    os_tick_count_start(&tick);
    rev = low_http_connect(&get->socket, ai_addr, ai_addrlen, 
        &cfg->pre_stop, "get_connect");
    get_socket = get->socket;
    if(res)
    {
        printf("[low http]get conn]free res\n");
        if(res_head)
            res=res_head;
#if DNS2 == DNS_PY
        freeaddrinfo2(res);
#elif DNS2 == DNS_EVDNS
        evutil_freeaddrinfo(res);
#else
        freeaddrinfo(res);
#endif
        res=NULL;
        if(res_head)
            res_head=NULL;
        printf("[low http]get conn]free res end\n");
    }
    if(rev != 0)
    {
        goto low_http_get_connect_failed;
    }
    if(os_tick_count_end(&tick)>300)
        printf("---------->[low_http]get_connect connect used:%d\n", os_tick_count_end(&tick));
    //https
    if(get->https_flag)
    {
        if((get->https = wifi_https_create(get->socket, get->host)) == NULL)
        {
            printf("[low_http]get_connect https create failed\n");
            goto low_http_get_connect_failed;
        }
    }
    //send
    rb = strlen(request_buf);
    os_tick_count_start(&tick);
    rev = low_http_send(get_socket, request_buf, rb, 1, 0, &cfg->pre_stop, 
            "get_connect", get->https);
    if(rev != rb)
    {
        goto low_http_get_connect_failed;
    }
    if(os_tick_count_end(&tick)>300)
        printf("---------->[low_http]get_connect send request:%d rev:%d used:%d\n",  rb, rev, os_tick_count_end(&tick));
    //recv
    os_tick_count_start(&tick);
    //recv这里只执行一次，如果出错的话，换一个socket重新操作
    
    int32_t first_recv_cnt, first_rb, try_max;
    uint8_t *first_data = get->data;
    first_recv_cnt = 0;
    first_rb = 0;
    rb = 0;
    try_max = 2;
    if(cfg->timout_recv > 0) //设置了接收的超时时间
    {
        try_max = low_http_try_cnt_calc(SO_RCVTIMEO, cfg->timout_recv,
                recv_timout_index);
        //如果设置了超时时间,说明应用对本次的recv操作很清楚,出错就不再做重试操作
        recv_try_cnt = 0; 
    }
    memset(get->data, 0, MAX_READ_BUFFER_SIZE);
http_get_connect_first_recv_loop:
    
    rb = low_http_recv(get_socket, first_data + first_rb, 
            MAX_READ_BUFFER_SIZE - first_rb, try_max, &recv_timout_index,
            &cfg->pre_stop, "get_connect", get->https);
    if(os_tick_count_end(&tick)>300)
        printf("---------->[low_http]get_connect first recv:%d used:%d\n", rb, os_tick_count_end(&tick));
    if(rb <= 0) //移动热点有第一次recv返回0的情况
    {
        printf("[low_http]get_connect first recv failed rev:%d errno:%d\n", rb, errno);
        if(cfg->pre_stop)
        {
            printf("[low_http]get_connect pre stop when first recv\n");
            goto low_http_get_connect_failed;
        }
        else if(recv_try_cnt > 0) //出错的话，我们把当前的socket关掉，新建一个socket来重试
        {
            printf("[low_http]get_connect first recv retry:%d\n", recv_try_cnt--);
            close(get_socket);
            get_socket = socket(AF_INET, SOCK_STREAM, 0);
            if(get_socket >= 0)
            {
                printf("[low_http]get_connect first recv change socket old:%d new:%d\n", get->socket, get_socket);
                get->socket = get_socket;
                goto low_http_get_connect_loop;
            }
            else
            {
                printf("[low_http]get_connect first recv change new socket failed\n");
                get->socket = -1;
            }
        }
        goto low_http_get_connect_failed;
    }
    first_rb += rb;
    if(!low_http_has_respond_head_end(get->data) && (++first_recv_cnt < 2))
    {//第一次接收的数据中有可能没有完整的http响应头,我们要再recv一次
        printf("[low_http]get_connect none respond end should recv again cnt:%d\n", first_recv_cnt);
        os_thread_sleep(100);
        goto http_get_connect_first_recv_loop;
    }
    
    rb = first_rb;
    if(header == NULL)
    {
        header = (http_header_t)os_mem_alloc(sizeof(http_header_s));
    }
    memset(header, 0, sizeof(http_header_s));
    rev = low_http_header_parse(header, (char*)get->data, rb); //得到整个头的长度
    if(rev < 0)
    {
        goto low_http_get_connect_failed;
    }
    if(status_code)
    {
        *status_code = header->status_code;
    }
    printf("[low_http]get status code:%d content_len:%lld\n", header->status_code, header->content_len);
    if(header->status_code == 200 || header->status_code == 206)
    {
        if (header->chunked != 0)
        {
            get->http_chunked = (http_chunked_t)os_mem_alloc(sizeof(http_chunked_s));
            low_http_chunked_init(get->http_chunked);
        }
        k_data_chunk_init(&get->chunk, get->data, GET_DATA_BUF_SIZE, "get");
        if(rev != rb)
        {
            //printf("[low_http]get_connect some data in header:%d\n", rb - rev);
            if (get->http_chunked == NULL)
            {
                get->recv_len = rb - rev;
                k_data_chunk_fill(&get->chunk, &get->data[rev], 0, get->recv_len);
            }
            else
            {
                uint8_t *buf = &get->data[rev];
                int32_t size = rb - rev;
                int32_t offset = 0;
                while (size)
                {
                    rev = low_http_chunked_parse(get->http_chunked, buf, size);
                    if (get->http_chunked->data_size != 0)
                    {
                        k_data_chunk_fill(&get->chunk, get->http_chunked->data_buf, offset, get->http_chunked->data_size);
                        offset += get->http_chunked->data_size;
                    }
                    buf += rev;
                    size -= rev;
                }
                get->recv_len = offset;
            }
        }
        else
        {
            get->recv_len = 0;
        }
        get->chunk_offset   = 0;
        get->content_len    = header->content_len;
        if(header->auto_update)
        {
            get->auto_update = k_strdup(header->auto_update);
            os_mem_free(header->auto_update);
        }
        os_mem_free(header);
        timout_index = 0;
        low_http_timout_set(get_socket, SO_RCVTIMEO, &timout_index);
        get->cfg = cfg;
        return get;
    }
    else if(header->status_code == 302)
    {
        int32_t soc_try_cnt;
        char* cut_char = NULL;
        char ip[20];
        url = header->new_url;
        printf("[low_http]new url:%s\n", url?:"");
        snprintf(ip, 20, "/%s/", inet_ntoa(host_addr)?:"");
        cut_char = strstr(url, ip);
        if(cut_char != NULL)
        {
            char* tmp_url = os_mem_alloc(HTTP_URL_LEN);
            memset(tmp_url, 0, HTTP_URL_LEN);
            strncpy(tmp_url, url, cut_char-url);
            strcat(tmp_url, "/");
            strcat(tmp_url, host);
            strcat(tmp_url, "/");
            strcat(tmp_url, cut_char+strlen(ip));
            strcpy(url, tmp_url);
            os_mem_free(tmp_url);
            printf("[low_http]cvt url:%s\n", url?:"");
        }
        close(get->socket);
        soc_try_cnt = 0;
#define SOC_TRY_CNT    3
low_http_get_connect_302_socket_loop:
        get->socket = -1;
        get_socket = socket(AF_INET, SOCK_STREAM, 0);
        if(get_socket < 0)
        {
            if(soc_try_cnt++ < SOC_TRY_CNT)
            {
                printf("[low_http]get_connect sock try:%d\n", soc_try_cnt);
                os_thread_sleep(50);
                goto low_http_get_connect_302_socket_loop;
            }
            printf("[low_http]get_connect get sock failed try:%d\n", soc_try_cnt);
            goto low_http_get_connect_failed;
        }
        get->socket = get_socket;
        if(soc_try_cnt > 0)
        {
            printf("[low_http]get_connect get sock ok try:%d\n", soc_try_cnt);
        }
        goto low_http_get_connect_loop;
    }
    else if(header->status_code == 416)
    { //请求的偏移有问题，请求出错
        printf("[low_http]get_connect err range:%lld\n", cfg->offset);
        goto low_http_get_connect_failed;
    }
low_http_get_connect_failed:
    if(header != NULL)
    {
        if(header->auto_update)
        {
            os_mem_free(header->auto_update);
        }
        os_mem_free(header);
    }
    low_http_get_obj_free(get);
    return NULL;
}

/*
 * return:
 * 0:
 * -1: 
 * -2: recv返回0但数据还没有接收完
 * -3: recv出错，并且收到了rst包
 */
int32_t low_http_get_read(http_get_str_t get, void *buffer, int32_t size)
{
    int32_t rb, https;
    uint8_t *recv_dst;
    data_chunk_t chunk;
    chunk = &get->chunk;
    rb = k_data_chunk_read(chunk, buffer, get->chunk_offset, size);
    if(rb > 0)
    {
        get->chunk_offset += rb;
        return rb;
    }
    else if(rb == 0) 
    {
        int32_t timout_index, try_cnt;
        if(((get->http_chunked == NULL) && (get->recv_len == get->content_len)) 
                || ((get->http_chunked != NULL) 
                && (get->http_chunked->flag == HTTP_CHUNKED_FLAG_END)))
        {
            //printf("\n[low_http]get_read over:%lld\n", get->recv_len);
            return 0;
        }

        timout_index = 0;
        try_cnt = RECV_RETRY_TIM;
        https = get->https_flag;
low_http_get_read_loop:
        recv_dst = (get->http_chunked == NULL) ? get->data : get->http_chunked->rawdata_buf;
        memset(recv_dst, 0, MAX_READ_BUFFER_SIZE);
        rb = (https == 0) ? recv(get->socket, recv_dst, MAX_READ_BUFFER_SIZE, 0) :
            wifi_https_recv(get->https, recv_dst, MAX_READ_BUFFER_SIZE);

        if(rb < 0)
        {
            if(get->cfg->pre_stop)
            {
                printf("[low_http]get_read pres top when recv\n");
                return 0;
            }
            if(errno == 104) //设备收到异常的rst数据包，这里返回出错，交给应用层来决定怎么办
            {
                printf("[low_http]get_read rev:%d errno:%d ECONNRESET\n", rb, errno);
                return -3;
            }
            if(try_cnt > 0)
            {
                printf("[low_http]get_read recv rev:%d errno:%d retry:%d timout_index:%d\n", 
                        rb, errno, try_cnt--, timout_index);
                low_http_timout_set(get->socket, SO_RCVTIMEO, &timout_index);
                goto low_http_get_read_loop;
            }
            return -1;
        }
        else if(rb == 0)
        {
            if(get->recv_len != get->content_len)
            {
                if(get->content_len > 0)
                {
                    printf("[low_http]get_read err rev0 but content:%d recv_len:%d\n",
                            get->content_len, get->recv_len);
                    return -2; //数据还没有下载完，但却recv 0,这里返回出错，交给应用层来决定怎么办
                }
                else if(get->content_len == 0 && try_cnt > 0)
                {//碰到一个路由器http响应没有content_len也不是chunk我们重试读几次,没有的话,就认为数据读完了
                    printf("[low_http]get_read rev0 and content_len = 0 try:%d recv_len:%d\n",
                            try_cnt--, get->recv_len);
                    os_thread_sleep(100);
                    goto low_http_get_read_loop;
                }
                return 0;
            }
            else //数据下载完
            {
                return 0;
            }
        }
        if(timout_index > 0)
        {
            timout_index = 0;
            low_http_timout_set(get->socket, SO_RCVTIMEO, &timout_index);
        }
        //printf("[low_http]read new:\n%s\n", recv_dst?:"");

		if (get->http_chunked != NULL)
		{
			uint8_t *buf = get->http_chunked->rawdata_buf;
			int32_t size = rb;
			int32_t offset = 0;
			int32_t rev;
			while (size)
			{
				rev = low_http_chunked_parse(get->http_chunked, buf, size);
				if (get->http_chunked->data_size != 0)
				{
					k_data_chunk_fill(&get->chunk, get->http_chunked->data_buf, offset, get->http_chunked->data_size);
					offset += get->http_chunked->data_size;
				}
				buf += rev;
				size -= rev;
			}
			rb = offset;
		}

        get->recv_len += rb;
        k_data_chunk_len_set(chunk, rb);
        rb = k_data_chunk_read(chunk, buffer, 0, size);
        get->chunk_offset = rb;
    }
    return rb;
}

int32_t low_http_get_disconnect(http_get_str_t get)
{
    int32_t rev;
    if(get == NULL)
    {
        return 0;
    }
    rev = low_http_get_obj_free(get);
    return rev;
}

int64_t low_http_get_content_len(http_get_str_t get)
{
    return get->content_len;
}

int64_t low_http_get_recv_len(http_get_str_t get)
{
    return get->recv_len;
}

http_post_str_t low_http_post_connect(http_post_cfg_t cfg, int32_t *status_code)
{
    http_post_str_t post;
    http_header_t header;
    http_request_s req;
    int32_t post_socket, len_posting, req_len, rev, len, timout_index;
    uint32_t tick;
    char* url, *request_buf, *host, *port;

    struct addrinfo *res = NULL, *res_head = NULL;
    if(!cfg){
        return NULL;
    }

    post = low_http_post_obj_malloc();
    if(post == NULL)
    {
        return NULL;
    }
    host        = post->host;
    port        = post->port;
    post_socket = post->socket;
    request_buf = (char*)post->data;
    header      = NULL;
    url         = cfg->url;
    req.offset  = 0;    //post没有offset
    req.append_header = cfg->append_header;

low_http_post_connect_loop:
    if(post->https != NULL)
    {
        wifi_https_delete(post->https);
        post->https = NULL;
    }
    post->https_flag = url_https_chk(url);
    memset(request_buf, 0, POST_DATA_BUF_SIZE);
    if(cfg->size < (MAX_WRITE_BUFFER_SIZE - POST_HEADER_GUESS_MAX_LEN)) //如果数据比较少，就放在http头里面一次发掉
    {
        len_posting = cfg->size;
        req_len = url2req(url, cfg->data, cfg->size, request_buf, HTTP_POST, &req);
    }
    else //如果数据量比较大，就先只发送http头，数据后面再发
    {
        len_posting = 0;
        req_len = url2req(url, NULL, cfg->size, request_buf, HTTP_POST, &req);
    }

    //printf("\n[low_http]post_connect request len:%d\n", req_len);
    //printf("%s\n", request_buf?:"");

    //host and port
    if(low_http_fetch_host_and_port(host, port, url, post->https_flag) != 0)
    {
        goto low_http_post_connect_failed;
    }
    printf("[low_http]post_connect host:%s port:%s\n", host?:"", port?:"");
    //dns
    os_tick_count_start(&tick);

    struct sockaddr *ai_addr;
    size_t ai_addrlen;
#if HTTP_DNS_CACHE
    http_dns_cache_t *dns_cache = low_http_dns_cache_get(host, port);
    if(dns_cache)
    {
        //printf("[low_http]post connect getaddr info cache\n");
        ai_addr = &dns_cache->ai_addr;
        ai_addrlen = dns_cache->ai_addrlen;
    }
    else
#endif
    {
        if(low_http_dns(host, port, &res, &res_head, &cfg->pre_stop, "post_connect") != 0)
        {
            goto low_http_post_connect_failed;
        }
        if(!res)
        {
            printf("[low http] post]got dns but res is NULL!\n");
            goto low_http_post_connect_failed;
        }
        ai_addr = res->ai_addr;
        ai_addrlen = res->ai_addrlen;
#if HTTP_DNS_CACHE
        low_http_dns_cache_add(host, port, ai_addr, ai_addrlen);
#endif
    }
    //if(os_tick_count_end(&tick)>100)
        printf("[low_http]post connect getaddr info used:%d\n", os_tick_count_end(&tick));
    //connect
    os_tick_count_start(&tick);
    rev = low_http_connect(&post->socket, ai_addr, ai_addrlen,
            &cfg->pre_stop, "post_connect");
    post_socket = post->socket;
    if(res)
    {
        printf("[low http]get conn]free res\n");
        if(res_head)
            res=res_head;
#if DNS2 == DNS_PY
        freeaddrinfo2(res);
#elif DNS2 == DNS_EVDNS
        evutil_freeaddrinfo(res);
#else
        freeaddrinfo(res);
#endif
    res=NULL;
    if(res_head)
        res_head=NULL;
    printf("[low http]get conn]free res end\n");
    }
    if(rev != 0)
    {
        goto low_http_post_connect_failed;
    }
    if(os_tick_count_end(&tick)>300)
        printf("[low_http]post connect connect used:%d\n", os_tick_count_end(&tick));
     
    if(post->https_flag)
    {
        if((post->https = wifi_https_create(post->socket, post->host)) == NULL)
        {
            printf("[low_http]post_connect https create failed\n");
            goto low_http_post_connect_failed;
        }
    }
    rev = low_http_send(post_socket, request_buf, req_len, 1, NULL, 
            &cfg->pre_stop, "post_connect", post->https);
    if(rev < 0)
    {
        goto low_http_post_connect_failed;
    }
    else if(rev != req_len) //如果没有发送完该怎么办? 后续根据需要，看是不是重新发送，现在就认为出错
    {
        printf("[low_http]post_connect send request:%d rev:%d\n", req_len, rev);
        goto low_http_post_connect_failed;
    }
    while(len_posting < cfg->size)
    {
        int32_t len, left;
        left = cfg->size - len_posting;
        len = left > MAX_WRITE_BUFFER_SIZE ? MAX_WRITE_BUFFER_SIZE : left;
        memmove(post->data, &cfg->data[len_posting], len);
        rev = low_http_send(post_socket, post->data, len, SEND_RETRY_TIM,
                NULL, &cfg->pre_stop, "post_connect", post->https);
        if(rev < 0)
        {
            goto low_http_post_connect_failed;
        }
        len_posting += rev;
    }
    //printf("[low_http]post_connect send data size:%d\n", len_posting);

    memset(post->data, 0, POST_DATA_BUF_SIZE);

    if(cfg->timout > 0)
    {//如果设置了超时事件,重试次数改为1，传入超时强转成指针,内部通过指针地址大小区分是时间,还是偏移
        printf("[low_http]post_recv set timeout:%d\n", cfg->timout);
        len = low_http_recv(post_socket, post->data, MAX_READ_BUFFER_SIZE, 
                1, (int32_t *)(cfg->timout), &cfg->pre_stop, "post_connect",
                post->https);
    }
    else
    {
        timout_index = 2;
        len = low_http_recv(post_socket, post->data, MAX_READ_BUFFER_SIZE, 
                3, &timout_index, &cfg->pre_stop, "post_connect", post->https);
    }
    if(len < 0)
    {
        goto low_http_post_connect_failed;
    }
    else if(len == 0)
    {
        printf("low_http]post_connect first recv empty\n");
        goto low_http_post_connect_failed;
    }

    if(header == NULL)
    {
        header = (http_header_t)os_mem_alloc(sizeof(http_header_s));
    }
    if(header == NULL)
    {
        printf("[low_http]post_connect alloc header NULL\n");
        goto low_http_post_connect_failed;
    }
    memset(header, 0, sizeof(http_header_s));
    rev = low_http_header_parse(header, (char*)post->data, len);
    if(rev <= 0)
    {
        goto low_http_post_connect_failed;
    }
    if(status_code)
    {
        *status_code = header->status_code;
    }
    printf("[low_http]post status code:%d content_len:%d\n", header->status_code, header->content_len);
    if(header->status_code == 200 || header->status_code == 201) //服务器创建文件成功
    {
        if (header->chunked != 0)
        {
            post->http_chunked = (http_chunked_t)os_mem_alloc(sizeof(http_chunked_s));
            low_http_chunked_init(post->http_chunked);
        }
        k_data_chunk_init(&post->chunk, post->data, POST_DATA_BUF_SIZE, "post");
        if(rev != len)
        {
            //printf("[low_http]post_connect some data in header\n");
            if (post->http_chunked == NULL)
            {
                post->recv_len = len - rev;
                k_data_chunk_fill(&post->chunk, &post->data[rev], 0, post->recv_len);
            }
            else
            {
                uint8_t *buf = &post->data[rev];
                int32_t size = len - rev;
                int32_t offset = 0;
                while (size)
                {
                    rev = low_http_chunked_parse(post->http_chunked, buf, size);
                    if (post->http_chunked->data_size != 0)
                    {
                        k_data_chunk_fill(&post->chunk, post->http_chunked->data_buf, offset, post->http_chunked->data_size);
                        offset += post->http_chunked->data_size;
                    }
                    buf += rev;
                    size -= rev;
                }
                post->recv_len = offset;
            }
        }
        else
        {
            post->recv_len = 0;
        }
        post->chunk_offset = 0;
        post->content_len  = header->content_len;
        if(header->auto_update)
        {
            os_mem_free(header->auto_update);
        }
        os_mem_free(header);
        return post;
    }
    else if(header->status_code == 302) //有重定向
    {
        url = header->new_url;
        printf("[low_http]post_connect new url:%s\n", url?:"");
        
        close(post->socket);
        post->socket = -1;
        post_socket = socket(AF_INET, SOCK_STREAM, 0);
        if(post_socket < 0)
        {
            printf("[low_http]post_connect get socket failed\n");
            goto low_http_post_connect_failed;
        }
        post->socket = post_socket;
        goto low_http_post_connect_loop;
    }
    else //其他status目前认为是错误的
    {
    }
low_http_post_connect_failed:
    if(header)
    {
        if(header->auto_update)
        {
            os_mem_free(header->auto_update);
        }
        os_mem_free(header);
    }
    low_http_post_obj_free(post);
    return NULL;
}

http_post_str_t low_http_post_connect_type(http_post_cfg_t cfg, int32_t *status_code,char *content_type)
{
    http_post_str_t post;
    http_header_t header;
    http_request_s req;
    int32_t post_socket, len_posting, req_len, rev, len, timout_index;
    uint32_t tick;
    char* url, *request_buf, *host, *port;

    struct addrinfo *res=NULL, *res_head = NULL;
    if(!cfg){
        return NULL;
    }

    post = low_http_post_obj_malloc();
    if(post == NULL)
    {
        return NULL;
    }
    host        = post->host;
    port        = post->port;
    post_socket = post->socket;
    request_buf = (char*)post->data;
    header      = NULL;
    url         = cfg->url;
    req.offset  = 0;    //post没有offset
    req.append_header = cfg->append_header;

low_http_post_connect_loop:
    if(post->https != NULL)
    {
        wifi_https_delete(post->https);
        post->https = NULL;
    }
    post->https_flag = url_https_chk(url);
    memset(request_buf, 0, POST_DATA_BUF_SIZE);
    if(cfg->size < (MAX_WRITE_BUFFER_SIZE - POST_HEADER_GUESS_MAX_LEN)) //如果数据比较少，就放在http头里面一次发掉
    {
        len_posting = cfg->size;
        req_len = url2req_type(url, cfg->data, cfg->size, request_buf, HTTP_POST, &req,content_type);
    }
    else //如果数据量比较大，就先只发送http头，数据后面再发
    {
        len_posting = 0;
        req_len = url2req_type(url, NULL, cfg->size, request_buf, HTTP_POST, &req,content_type);
    }

    //printf("\n[low_http]post_connect request len:%d\n", req_len);
    //printf("%s\n", request_buf?:"");

    //host and port
    if(low_http_fetch_host_and_port(host, port, url, post->https_flag) != 0)
    {
        goto low_http_post_connect_failed;
    }
    printf("[low_http]post_connect host:%s port:%s\n", host?:"", port?:"");
    //dns
    os_tick_count_start(&tick);
    struct sockaddr *ai_addr;
    size_t ai_addrlen;
#if HTTP_DNS_CACHE
    http_dns_cache_t *dns_cache = low_http_dns_cache_get(host, port);
    if(dns_cache)
    {
        //printf("[low_http]post connect getaddr info cache\n");
        ai_addr = &dns_cache->ai_addr;
        ai_addrlen = dns_cache->ai_addrlen;
    }
    else
#endif
    {
        if(low_http_dns(host, port, &res, &res_head, &cfg->pre_stop, "post_connect") != 0)
        {
            goto low_http_post_connect_failed;
        }
        if(!res)
        {
            printf("[low http] post]got dns but res is NULL!\n");
            goto low_http_post_connect_failed;
        }
        ai_addr = res->ai_addr;
        ai_addrlen = res->ai_addrlen;
#if HTTP_DNS_CACHE
        low_http_dns_cache_add(host, port, ai_addr, ai_addrlen);
#endif
    }
    if(os_tick_count_end(&tick)>100)
        printf("[low_http]post connect getaddr info used:%d\n", os_tick_count_end(&tick));
    //connect
    os_tick_count_start(&tick);
    rev = low_http_connect(&post->socket, ai_addr, ai_addrlen,
            &cfg->pre_stop, "post_connect");
    post_socket = post->socket;
    if(res)
    {
        if(res_head)
            res=res_head;
    #if DNS2 == DNS_PY
    freeaddrinfo2(res);
    #elif DNS2 == DNS_EVDNS
        evutil_freeaddrinfo(res);
    #else
        freeaddrinfo(res);
    #endif
        res=NULL;
        if(res_head)
            res_head=NULL;
    }
    if(rev != 0)
    {
        goto low_http_post_connect_failed;
    }
    if(os_tick_count_end(&tick)>300)
        printf("[low_http]post connect connect used:%d\n", os_tick_count_end(&tick));
     
    if(post->https_flag)
    {
        if((post->https = wifi_https_create(post->socket, post->host)) == NULL)
        {
            printf("[low_http]post_connect https create failed\n");
            goto low_http_post_connect_failed;
        }
    }
    rev = low_http_send(post_socket, request_buf, req_len, 1, NULL, 
            &cfg->pre_stop, "post_connect", post->https);
    if(rev < 0)
    {
        goto low_http_post_connect_failed;
    }
    else if(rev != req_len) //如果没有发送完该怎么办? 后续根据需要，看是不是重新发送，现在就认为出错
    {
        printf("[low_http]post_connect send request:%d rev:%d\n", req_len, rev);
        goto low_http_post_connect_failed;
    }
    while(len_posting < cfg->size)
    {
        int32_t len, left;
        left = cfg->size - len_posting;
        len = left > MAX_WRITE_BUFFER_SIZE ? MAX_WRITE_BUFFER_SIZE : left;
        memmove(post->data, &cfg->data[len_posting], len);
        rev = low_http_send(post_socket, post->data, len, SEND_RETRY_TIM,
                NULL, &cfg->pre_stop, "post_connect", post->https);
        if(rev < 0)
        {
            goto low_http_post_connect_failed;
        }
        len_posting += rev;
    }
    //printf("[low_http]post_connect send data size:%d\n", len_posting);

    memset(post->data, 0, POST_DATA_BUF_SIZE);

    if(cfg->timout > 0)
    {//如果设置了超时事件,重试次数改为1，传入超时强转成指针,内部通过指针地址大小区分是时间,还是偏移
        printf("[low_http]post_recv set timeout:%d\n", cfg->timout);
        len = low_http_recv(post_socket, post->data, MAX_READ_BUFFER_SIZE, 
                1, (int32_t *)(cfg->timout), &cfg->pre_stop, "post_connect",
                post->https);
    }
    else
    {
        timout_index = 2;
        len = low_http_recv(post_socket, post->data, MAX_READ_BUFFER_SIZE, 
                3, &timout_index, &cfg->pre_stop, "post_connect", post->https);
    }
    if(len < 0)
    {
        goto low_http_post_connect_failed;
    }
    else if(len == 0)
    {
        printf("low_http]post_connect first recv empty\n");
        goto low_http_post_connect_failed;
    }

    if(header == NULL)
    {
        header = (http_header_t)os_mem_alloc(sizeof(http_header_s));
    }
    if(header == NULL)
    {
        printf("[low_http]post_connect alloc header NULL\n");
        goto low_http_post_connect_failed;
    }
    memset(header, 0, sizeof(http_header_s));
    rev = low_http_header_parse(header, (char*)post->data, len);
    if(rev <= 0)
    {
        goto low_http_post_connect_failed;
    }
    if(status_code)
    {
        *status_code = header->status_code;
    }
    printf("[low_http]post status code:%d content_len:%d\n", header->status_code, header->content_len);
    if(header->status_code == 200 || header->status_code == 201) //服务器创建文件成功
    {
        if (header->chunked != 0)
        {
            post->http_chunked = (http_chunked_t)os_mem_alloc(sizeof(http_chunked_s));
            low_http_chunked_init(post->http_chunked);
        }
        k_data_chunk_init(&post->chunk, post->data, POST_DATA_BUF_SIZE, "post");
        if(rev != len)
        {
            //printf("[low_http]post_connect some data in header\n");
            if (post->http_chunked == NULL)
            {
                post->recv_len = len - rev;
                k_data_chunk_fill(&post->chunk, &post->data[rev], 0, post->recv_len);
            }
            else
            {
                uint8_t *buf = &post->data[rev];
                int32_t size = len - rev;
                int32_t offset = 0;
                while (size)
                {
                    rev = low_http_chunked_parse(post->http_chunked, buf, size);
                    if (post->http_chunked->data_size != 0)
                    {
                        k_data_chunk_fill(&post->chunk, post->http_chunked->data_buf, offset, post->http_chunked->data_size);
                        offset += post->http_chunked->data_size;
                    }
                    buf += rev;
                    size -= rev;
                }
                post->recv_len = offset;
            }
        }
        else
        {
            post->recv_len = 0;
        }
        post->chunk_offset = 0;
        post->content_len  = header->content_len;
        os_mem_free(header);
        return post;
    }
    else if(header->status_code == 302) //有重定向
    {
        url = header->new_url;
        printf("[low_http]post_connect new url:%s\n", url?:"");
        
        close(post->socket);
        post->socket = -1;
        post_socket = socket(AF_INET, SOCK_STREAM, 0);
        if(post_socket < 0)
        {
            printf("[low_http]post_connect get socket failed\n");
            goto low_http_post_connect_failed;
        }
        post->socket = post_socket;
        goto low_http_post_connect_loop;
    }
    else //其他status目前认为是错误的
    {
    }
low_http_post_connect_failed:
    if(header)
    {
        os_mem_free(header);
    }
    low_http_post_obj_free(post);
    return NULL;
}

int32_t low_http_post_read(http_post_str_t post, void* buffer, int32_t size)
{
    int32_t rb, https;
    uint8_t *recv_dst;
    data_chunk_t chunk;
    chunk = &post->chunk;
    rb = k_data_chunk_read(chunk, buffer, post->chunk_offset, size);
    if(rb > 0)
    {
        post->chunk_offset += rb;
        return rb;
    }
    else if(rb == 0)
    {
        if(((post->http_chunked == NULL) && (post->recv_len == post->content_len)) 
                || ((post->http_chunked != NULL) 
                && (post->http_chunked->flag == HTTP_CHUNKED_FLAG_END)))
        {
            //printf("\n[low_http]post_read over:%d\n", post->recv_len);
            return 0;
        }
        https = post->https_flag;
        recv_dst = (post->http_chunked == NULL) ? post->data : post->http_chunked->rawdata_buf;
        rb = (https == 0) ? recv(post->socket, recv_dst, MAX_READ_BUFFER_SIZE, 0) :
            wifi_https_recv(post->https, recv_dst, MAX_READ_BUFFER_SIZE);
        if(rb < 0)
        {
            return -1;
        }
        else if(rb == 0)
        {
            return 0;
        }

        if (post->http_chunked != NULL)
        {
            uint8_t *buf = post->http_chunked->rawdata_buf;
            int32_t size = rb;
            int32_t offset = 0;
            int32_t rev;
            while (size)
            {
                rev = low_http_chunked_parse(post->http_chunked, buf, size);
                if (post->http_chunked->data_size != 0)
                {
                    k_data_chunk_fill(&post->chunk, post->http_chunked->data_buf, offset, post->http_chunked->data_size);
                    offset += post->http_chunked->data_size;
                }
                buf += rev;
                size -= rev;
            }
            rb = offset;
        }
        
        post->recv_len += rb;
        k_data_chunk_len_set(chunk, rb);
        rb = k_data_chunk_read(chunk, buffer, 0, size);
        post->chunk_offset = rb;
    }
    return rb;
}

int32_t low_http_post_disconnect(http_post_str_t post)
{
    int32_t rev;
    if(post == NULL)
    {
        return 0;
    }
    rev = low_http_post_obj_free(post);
    return rev;
}

int32_t low_http_post_content_len(http_post_str_t post)
{
    return post->content_len;
}

void low_http_header_token_set(char *token)
{
    if(header_token)
    {
        os_mem_free(header_token);
        header_token = NULL;
    }
    header_token = k_strdup(token);
}

void low_wifi_http_dns_serv_chg(void)
{ //实测119.29.29.29比223的快
static const char *dns_buf[] = {"119.29.29.29", "223.6.6.6", "114.114.114.114"}; 
static int32_t dns_index = 1;
    int32_t num;
    const char *ip;
    num = sizeof(dns_buf) / sizeof(dns_buf[0]);
    ip = dns_buf[dns_index++];
    if(dns_index >= num)
    {
        dns_index = 0;
    }
#if 0 //linux
    char cmd[128];
    printf("[low_http]dns_serv_chg to [%s]\n", ip);
    sprintf(cmd, "echo nameserver %s > /etc/resolv.conf", ip);
    system(cmd);
#else //esp32
    printf("[low_http]dns_serv_chg to [%s]\n", ip);
    low_wifi_set_dns_server((char *)ip);
#endif
}

#if DNS2 == DNS_EVDNS

#define EVE_DNS_START   0
#define EVE_DNS_OVER    1

struct user_data
{
    struct event_base *base;
    int32_t n_pending_requests;
    char *name;
    struct addrinfo **res;
    int32_t errcode;
    int32_t flag;
};

void libevent_cb(int errcode, struct evutil_addrinfo *addr, void *ptr)
{
    struct user_data *data = ptr;
    struct evutil_addrinfo *ai;
    data->errcode = errcode;
    *data->res = addr;
    if(errcode)
    {
        //printf("[low_http]event_cb err:%d\n", errcode);
        data->flag = EVE_DNS_OVER;
    }
    if(--data->n_pending_requests == 0)
    {
        event_base_loopexit(data->base, NULL);
    }
    data->flag = EVE_DNS_OVER;
}

#define TIMOUT_NUM_DNS      3
#define TIMOUT_DNS_ONCE     10
static int32_t timout_index_dns = 0;
static float timout_buf_dns[] = {1.000, 2.000, 2.000};

#include "low_port.h"

int32_t libevent_getaddrinfo(const char *hostname, const char *servname,
            const struct addrinfo *hints_in, struct addrinfo **res, int32_t *pre_stop)
{
    uint32_t tick;
    char timeout_buf[16];
    float timeout;
    struct event_base *base;
    struct evdns_base *dnsbase;
    struct evutil_addrinfo hints;
    struct evdns_getaddrinfo_request *req;
    struct user_data user_data;
    base = event_base_new();
    if(!base)
    {
        printf("[low_http]evdns base_new failed\n");
        return -1;
    }
    dnsbase = evdns_base_new(base, 1);
    if(!dnsbase)
    {
        event_base_free(base);
        printf("[low_http]evdns dns_base_new failed\n");
        return -1;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = EVUTIL_AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    user_data.n_pending_requests = 1;
    user_data.base = base;
    user_data.name = k_strdup(hostname);
    user_data.res  = res;
    user_data.flag = EVE_DNS_START;

    timeout = timout_buf_dns[timout_index_dns];
    sprintf(timeout_buf, "%.3f", timeout);
    evdns_base_set_option(dnsbase, "timeout:", timeout_buf);
    evdns_base_set_option(dnsbase, "attempts:", "0"); //dns重试0次
    tick = os_ticks_get();
    req = evdns_getaddrinfo(dnsbase, hostname, NULL, &hints, libevent_cb, &user_data);
    if(req == NULL)
    {
        printf("[low_http]evdns-------------->return immediately\n");
    }
    if(user_data.n_pending_requests)
    {
        event_base_dispatch(base);
    }
    free(user_data.name);
    evdns_base_free(dnsbase, 0);
    event_base_free(base);
    if(*user_data.res != NULL)
    {
        ((struct sockaddr_in*)((*user_data.res)->ai_addr))->sin_port = htons(atoi(servname));
    }
    if(user_data.errcode == 0) //成功了,把超时时间设成最短
    {
        timout_index_dns = 0;
    }
    else //出错了, 我们变一下超时时间
    {
        printf("[low_http]evdns err:%d timeout:%.3f\n", user_data.errcode, timeout);
        int32_t tmp = timout_index_dns + 1;
        tmp = tmp >= TIMOUT_NUM_DNS ? 0 : tmp;
        timout_index_dns = tmp;
    }
    return user_data.errcode;
}
#endif


/******************************test****************************/

void low_wifi_http_get_test(void);
void low_wifi_http_post_test(void);
void low_wifi_http_mbedtls_test(void);

void low_wifi_http_test(void)
{
    low_wifi_http_mbedtls_test();
    //low_wifi_http_post_test();
    //low_wifi_http_get_test();
}

//const char test_get_url[] = "https://www.baidu.com";
const char test_get_url[] = "https://www.jd.com";

void low_wifi_http_get_test(void)
{
    int32_t rev, content, size, total;
    uint8_t *buf;
    http_get_cfg_t cfg;
    http_get_str_t get;

    low_http_get_cfg_reset(&cfg);
    cfg.url      = (char*)test_get_url;
    get = low_http_get_connect(&cfg, NULL);
    if(get == NULL)
    {
        printf("[http]get_test connect NULL|n");
        while(1);
    }
    content = low_http_get_content_len(get);
    printf("[http]get_test content of %s is\n", cfg.url?:"");
    size    = 4096;
    buf     = os_mem_alloc(size);
    total   = 0;
    while(1)
    {
        memset(buf, 0, size);
        rev = low_http_get_read(get, buf, size);
        if(rev > 0)
        {
            total += rev;
        }
        else if(rev == 0)
        {
            printf("\n[http]get_test read over\n");
        }
        else if(rev < 0)
        {
            printf("\n[http]get_test read failed\n");
            break;
        }
        printf("[%d]", rev);
        printf("%s", buf);
        if(rev == 0)
        {
            break;
        }
    }
    os_mem_free(buf);
    low_http_get_disconnect(get);
    printf("[http]get content:%d get total:%d, size, total\n",
            content, total);
    while(1);
}

void low_wifi_http_post_test(void)
{
    ;
}

#define WEB_SERVER "www.baidu.com"
#define WEB_PORT "443"
#define WEB_URL "https://www.baidu.com"

#if 0
static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "\r\n";
#else
static const char *REQUEST = "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: www.baidu.com\r\n"
    "\r\n";
#endif

void low_wifi_http_mbedtls_test(void)
{
#define BUF_SIZE    512
    int ret, flags, len, i;
    const char *cert;
    char *buf;
    buf = os_mem_alloc(BUF_SIZE);

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    printf("[https]test start\n");

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    printf("Seeding the random number generator\n");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0)
    {
        printf("mbedtls_ctr_drbg_seed returned %d\n", ret);
        abort();
    }

    printf("Loading the CA root certificate...\n");

    printf("Setting hostname for TLS session...\n");

     /* Hostname set here should match CN in server certificate */
    if((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {
        printf("mbedtls_ssl_set_hostname returned -0x%x\n", -ret);
        abort();
    }

    printf("Setting up the SSL/TLS structure...\n");

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        printf("mbedtls_ssl_config_defaults returned %d\n", ret);
        goto exit;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    extern int32_t tls_certificate_set(mbedtls_x509_crt *cert_chain, char *host);
    tls_certificate_set(&cacert, WEB_SERVER);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

#if 1
    //mbedtls_esp_enable_debug_log(&conf, 4);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        printf("mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        goto exit;
    }

    printf("Connecting to %s:%s...", WEB_SERVER, WEB_PORT);

    if ((ret = mbedtls_net_connect(&server_fd, WEB_SERVER,
                                  WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        printf("mbedtls_net_connect returned -%x\n", -ret);
        goto exit;
    }

    printf("Connected.\n");

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    printf("Performing the SSL/TLS handshake...\n");

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            printf("mbedtls_ssl_handshake returned -0x%x\n", -ret);
            goto exit;
        }
    }

    printf("Verifying peer X.509 certificate...\n");

    if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
    {
        /* In real life, we probably want to close connection if ret != 0 */
        printf("Failed to verify peer certificate!\n");
        bzero(buf, BUF_SIZE);
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
        printf("verification info: %s\n", buf);
    }
    else {
        printf("Certificate verified.\n");
    }

    printf("Writing HTTP request...\n");

    printf("https request header:\n");
    printf("----------------------\n");
    printf("%s\n", REQUEST);
    printf("----------------------\n");
    while((ret = mbedtls_ssl_write(&ssl, (const unsigned char *)REQUEST, strlen(REQUEST))) <= 0)
    {
        if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            printf("mbedtls_ssl_write returned -0x%x\n", -ret);
            goto exit;
        }
    }

    len = ret;
    printf("%d bytes written\n", len);
    printf("Reading HTTP response...\n");

    do
    {
        len = BUF_SIZE - 1;
        bzero(buf, BUF_SIZE);
        ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);

        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;

        if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            ret = 0;
            break;
        }

        if(ret < 0)
        {
            printf("mbedtls_ssl_read returned -0x%x\n", -ret);
            break;
        }

        if(ret == 0)
        {
            printf("connection closed\n");
            break;
        }

        len = ret;
        //printf("\n<%d bytes read>\n", len);
        /* Print response directly to stdout as it is read */
        for(i = 0; i < len; i++) {
            putchar(buf[i]);
        }
    } while(1);

    mbedtls_ssl_close_notify(&ssl);

exit:
    os_mem_free(buf);
    mbedtls_ssl_session_reset(&ssl);
    mbedtls_net_free(&server_fd);

    printf("\n[https]test over\n");
    while(1);
}
