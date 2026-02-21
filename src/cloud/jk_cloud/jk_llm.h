#ifndef __JK_LLM_H__
#define __JK_LLM_H__

#include <stdbool.h>
#include <stdint.h>

#define JK_LLM_DEFAULT_HOST "ws://"
#define JK_LLM_DEFAULT_PORT "9400"
#define JK_LLM_SESSION_ID_LEN 64

typedef enum {
    JK_LLM_STATE_DISCONNECTED = 0,
    JK_LLM_STATE_CONNECTING,
    JK_LLM_STATE_CONNECTED,
} jk_llm_state_e;

typedef enum {
    JK_LLM_MSG_TYPE_STATUS,
    JK_LLM_MSG_TYPE_LLM_RESPONSE,
    JK_LLM_MSG_TYPE_TOOL_CALL,
    JK_LLM_MSG_TYPE_TOOL_CALLBACK,      /* 服务端回调客户端工具 (v1.2.0) */
    JK_LLM_MSG_TYPE_TOOLS_REGISTERED,   /* 工具注册确认 (v1.2.0) */
    JK_LLM_MSG_TYPE_ERROR,
    JK_LLM_MSG_TYPE_PONG,
} jk_llm_msg_type_e;

struct jk_llm;
typedef struct jk_llm jk_llm_t;

typedef struct {
    jk_llm_msg_type_e type;
    const char *session_id;
    const char *content;
    const char *tool_name;
    char *arguments;
    char *result;
    const char *error_code;
    const char *error_message;
    /* v1.2.0 新增字段 (for tool_callback) */
    const char *call_id;           /* 工具调用 ID */
} jk_llm_message_t;

/* v1.2.0 客户端工具定义 */
typedef struct {
    const char *name;            /* 工具名称 */
    const char *description;     /* 工具描述 */
    const char *parameters;      /* JSON Schema 格式的参数定义 */
} jk_llm_tool_def_t;

typedef struct {
    void (*on_connected)(jk_llm_t *llm);
    void (*on_disconnected)(jk_llm_t *llm);
    void (*on_message)(jk_llm_t *llm, jk_llm_message_t *msg);
    void (*on_error)(jk_llm_t *llm, const char *error_msg);
    /* v1.2.0 新增回调 */
    void (*on_tool_callback)(jk_llm_t *llm, const char *call_id, const char *tool_name,
                             const char *arguments);
    void (*on_tools_registered)(jk_llm_t *llm, int count);
} jk_llm_callbacks_t;

struct jk_llm {
    void *ws;
    jk_llm_state_e state;
    jk_llm_callbacks_t cbs;
    char *host;
    char *port;
    char session_id[JK_LLM_SESSION_ID_LEN];
    void *user_data;
};

jk_llm_t *jk_llm_create(const char *host, const char *port, jk_llm_callbacks_t *cbs);
void jk_llm_destroy(jk_llm_t *llm);

int jk_llm_connect(jk_llm_t *llm);
int jk_llm_disconnect(jk_llm_t *llm);

int jk_llm_send_text(jk_llm_t *llm, const char *text);
int jk_llm_send_ping(jk_llm_t *llm);

/* v1.2.0 客户端工具注册 API */
int jk_llm_register_tools(jk_llm_t *llm, const jk_llm_tool_def_t *tools, int count);
int jk_llm_send_tool_result(jk_llm_t *llm, const char *call_id,
                            const char *result, bool success, const char *error);

bool jk_llm_is_connected(jk_llm_t *llm);

const char *jk_llm_get_session_id(jk_llm_t *llm);

#endif
