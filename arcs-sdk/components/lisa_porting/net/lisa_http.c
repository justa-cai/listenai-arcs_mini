#define TAG "hal_http"

#include "lisa_http.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lisa_typedef.h"
#include "HTTPCUsr_api.h"


#define RESP_BUF_SIZE 4096
#define DOWNLOAD_SIZE 1024

lisa_http_t *lisa_http_init(lisa_http_request_t *req)
{
	int ret = 0;
	lisa_http_t *http_session = NULL;

	http_session = (lisa_http_t *)lisa_mem_calloc(1, sizeof(lisa_http_t));
	if (http_session == NULL) {
		LISA_LOGE(TAG, "no memory for lisa_http_t.\n");
		return NULL;
	}

	strcpy(http_session->url, req->url);
	if (req->body_len > 0) {
		memcpy(http_session->body, req->body, req->body_len);
	}
	http_session->header_addr = req->headers;
	http_session->body_len = req->body_len;
	http_session->method = req->method;
	http_session->timeout = req->timeout;
	http_session->inter_on_data = req->on_data;
	http_session->user = req->user;

	return http_session;
}

lisa_http_err_e lisa_http_perform(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_COMMON_ERR;
	lisa_http_data_t http_data;
	int ret = 0;

	char *buf = NULL;
	LISA_LOGI(TAG, "lisa_http_perform, url: %s, method: %d\r\n", ins->url, ins->method);
	HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		goto ERR_HTTP;
	}

	strcpy(http_param->Uri, ins->url);
	if (ins->method == LISA_HTTP_GET) {
		http_param->HttpVerb = VerbGet;
	} else if (ins->method == LISA_HTTP_POST) {
		http_param->HttpVerb = VerbPost;
	}
	
	http_param->nTimeout = ins->timeout;
	http_param->pData = ins->body;
	http_param->pLength = ins->body_len;

	HTTP_CLIENT http_client = {0};
	if ((ret = HTTPC_open(http_param)) != 0) {
        LISA_LOGE(TAG, "http open err..");
        goto ERR;
    }
    if ((ret = HTTPC_request(http_param, ins->header_addr)) != 0) {
        LISA_LOGE(TAG, "http request err..  ret: %d\r\n", ret);
        goto ERR;
    }
    if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
        LISA_LOGE(TAG, "http get request info err.. %d\r\n", ret);
        goto ERR;
    }
    if (http_client.TotalResponseBodyLength != 0) {
        unsigned int received = 0;
        unsigned int readsize = 0;
        buf = lisa_mem_calloc(1, http_client.TotalResponseBodyLength + 1);
        do
		{
            if (HTTPC_read(http_param, buf + readsize, RESP_BUF_SIZE, (void *)&received) != 0) {
                if (received > 0) readsize += received;
                break;
            } else {
                readsize += received;
            }

        } while (1);
        ret = 0;
        http_data.buf = buf;
        http_data.len = http_client.TotalResponseBodyLength;
        http_data.user = ins->user;
        if (ins->inter_on_data) {
            ins->inter_on_data(&http_data);
        }
    } else {
        unsigned int received = 0;
        unsigned int readsize = 0;
        buf = lisa_mem_calloc(1, 4096 + 1);
        do
        {
            if (HTTPC_read(http_param, buf + readsize, RESP_BUF_SIZE, (void *)&received) != 0) {
                if (received > 0) readsize += received;
                break;
            } else {
                readsize += received;
            }

        } while (1);
        ret = 0;
        http_data.buf = buf;
        http_data.len = http_client.TotalResponseBodyLength;
        http_data.user = ins->user;
        if (ins->inter_on_data) {
            ins->inter_on_data(&http_data);
        }
//        printk("------------------------ lisa_http_perform: %d\r\n", __LINE__);
//        ret = -1;
    }

ERR:
	HTTPC_close(http_param);
	lisa_mem_free(http_param);
ERR_HTTP:
	lisa_mem_free(buf);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}

lisa_http_err_e lisa_http_download(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_COMMON_ERR;
	lisa_http_data_t http_data;
	int ret = 0;

	char *buf = NULL;

	HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		goto ERR_HTTP;
	}

	strcpy(http_param->Uri, ins->url);
	if (ins->method == LISA_HTTP_GET) {
		http_param->HttpVerb = VerbGet;
	} else if (ins->method == LISA_HTTP_POST) {
		http_param->HttpVerb = VerbPost;
	}
	
	http_param->nTimeout = ins->timeout;
	http_param->pData = ins->body;
	http_param->pLength = ins->body_len;

	HTTP_CLIENT http_client = {0};
	if ((ret = HTTPC_open(http_param)) != 0) {
		LISA_LOGE(TAG, "http open err..");
		goto ERR;
	}
	if ((ret = HTTPC_request(http_param, ins->header_addr)) != 0) {
		LISA_LOGE(TAG, "http request err..  ret: %d", ret);
		goto ERR;
	}
	if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
		LISA_LOGE(TAG, "http get request info err..");
		goto ERR;
	}

	if (http_client.TotalResponseBodyLength != 0) {
		unsigned int received = 0;
		buf = lisa_mem_calloc(1, DOWNLOAD_SIZE);
		do
		{
			if (HTTPC_read(http_param, buf, DOWNLOAD_SIZE, (void *)&received) != 0) {
				if (received > 0) {
					http_data.buf = buf;
					http_data.len = received;
					http_data.user = ins->user;
					if (ins->inter_on_data) {
						ins->inter_on_data(&http_data);
					}
				}
				break;
			} else {
				http_data.buf = buf;
				http_data.len = received;
				http_data.user = ins->user;
				if (ins->inter_on_data) {
					ins->inter_on_data(&http_data);
				}
			}
			
		} while (1);
		ret = 0;
		
	} else {
		ret = -1;
	}

ERR:
	HTTPC_close(http_param);
	lisa_mem_free(http_param);
ERR_HTTP:
	lisa_mem_free(buf);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}

lisa_http_err_e lisa_http_cleanup(lisa_http_t *ins)
{
	lisa_http_err_e http_ret = LISA_HTTP_OK;
	if (ins) {
		lisa_mem_free(ins);
		return http_ret;
	}
	return http_ret;
};

lisa_http_err_e lisa_http_perform_chunked(lisa_http_t *ins)
{
	lisa_http_data_t http_data;
	int ret = 0;

	char *buf = NULL;

	HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		goto ERR_HTTP;
	}

	strcpy(http_param->Uri, ins->url);
	if (ins->method == LISA_HTTP_GET) {
		http_param->HttpVerb = VerbGet;
	} else if (ins->method == LISA_HTTP_POST) {
		http_param->HttpVerb = VerbPost;
	}

	http_param->nTimeout = ins->timeout;
	http_param->pData = ins->body;
	http_param->pLength = ins->body_len;

	HTTP_CLIENT http_client = {0};
	if ((ret = HTTPC_open(http_param)) != 0) {
		LISA_LOGE(TAG, "http open err..");
		goto ERR;
	}
	if ((ret = HTTPC_request(http_param, ins->header_addr)) != 0) {
		LISA_LOGE(TAG, "http request err..  ret: %d\r\n", ret);
		goto ERR;
	}
	if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
		LISA_LOGE(TAG, "http get request info err.. %d\r\n", ret);
		goto ERR;
	}
	if (http_client.TotalResponseBodyLength != 0) {
		LISA_LOGE(TAG, "not http chunked");
		goto ERR;
	}

	unsigned int received = 0;

	buf = lisa_mem_calloc(1, RESP_BUF_SIZE + 1);
	if (buf == NULL) {
		goto ERR;
	}

	while (HTTPC_read(http_param, buf, RESP_BUF_SIZE, (void *)&received) == 0) {
		http_data.buf = buf;
		http_data.len = received;
		http_data.user = ins->user;

		buf[received] = 0;

		if (ins->inter_on_data) {
			ins->inter_on_data(&http_data);
		}

		received = 0;
	}

	if (received) {
		http_data.buf = buf;
		http_data.len = received;
		http_data.user = ins->user;
		if (ins->inter_on_data) {
			ins->inter_on_data(&http_data);
		}
		received = 0;
	}

ERR:
	HTTPC_close(http_param);
	lisa_mem_free(http_param);
ERR_HTTP:
	lisa_mem_free(buf);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}

lisa_http_err_e lisa_http_perform_chunked_with_cb(lisa_http_t *ins,
						  int (*on_chunk)(lisa_http_data_t *))
{
	lisa_http_data_t http_data;
	int ret = 0;

	char *buf = NULL;

	HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
	if (http_param == NULL) {
		LISA_LOGE(TAG, "malloc http params error");
		goto ERR_HTTP;
	}

	strcpy(http_param->Uri, ins->url);
	if (ins->method == LISA_HTTP_GET) {
		http_param->HttpVerb = VerbGet;
	} else if (ins->method == LISA_HTTP_POST) {
		http_param->HttpVerb = VerbPost;
	}

	http_param->nTimeout = ins->timeout;
	http_param->pData = ins->body;
	http_param->pLength = ins->body_len;

	HTTP_CLIENT http_client = {0};
	if ((ret = HTTPC_open(http_param)) != 0) {
		LISA_LOGE(TAG, "http open err..");
		goto ERR;
	}
	if ((ret = HTTPC_request(http_param, ins->header_addr)) != 0) {
		LISA_LOGE(TAG, "http request err..  ret: %d\r\n", ret);
		goto ERR;
	}
	if ((ret = HTTPC_get_request_info(http_param, &http_client)) != 0) {
		LISA_LOGE(TAG, "http get request info err.. %d\r\n", ret);
		goto ERR;
	}
	if (http_client.TotalResponseBodyLength != 0) {
		LISA_LOGE(TAG, "not http chunked");
		goto ERR;
	}

	UINT32 received = 0;

	buf = lisa_mem_calloc(1, RESP_BUF_SIZE + 1);
	if (buf == NULL) {
		goto ERR;
	}

	while (HTTPC_read(http_param, buf, RESP_BUF_SIZE, (void *)&received) == 0) {
		http_data.buf = buf;
		http_data.len = received;
		http_data.user = ins->user;

		buf[received] = 0;

		if (on_chunk) {
			if (on_chunk(&http_data)) {
				ret = -1;
				goto ERR;
			}
		}

		received = 0;
	}

	if (received) {
		http_data.buf = buf;
		http_data.len = received;
		http_data.user = ins->user;
		if (on_chunk) {
			on_chunk(&http_data);
		}
		received = 0;
	}

ERR:
	HTTPC_close(http_param);
	lisa_mem_free(http_param);
ERR_HTTP:
	lisa_mem_free(buf);

	return (ret == 0 ? LISA_HTTP_OK : LISA_HTTP_COMMON_ERR);
}
