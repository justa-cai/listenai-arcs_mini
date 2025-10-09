// #include "cJSON.h"
// #include "lisa_http.h"
// #include "lisa_semaphore.h"

// #include "stdio.h"
// #include "lisa_log.h"
// #define TAG "haoxueduo"

// #include "shell.h"
// #include "app_cloud.h"
// #include "lisa_aiui.h"

// #include "app_client.h"
// #include "assistant_controller.h"
// #include "pa_manager.h"
// #include "evs_utils.h"
// #include "haoxueduo.h"

// #include "lisa_thread.h"
// #include "tone.h"
// #include "app_player.h"
// #include "app_tone.h"

// #define ROLES_GET_RETRY_MAX   (3)
// #define ROLSE_GET_RETRY_INTERVAL_MS  (1000)
// const app_cloud_t *app_cloud_get();

// static char *g_speaker_name = NULL;
// static uint32_t g_speaker_id = 0;
// static struct haoxueduo_role *g_roles = NULL;
// static uint32_t g_roles_cnt = 0;
// static bool is_has_get_roles = false;
// static uint32_t roles_get_retry_count = 0;
// static char *g_token = NULL;
// extern void app_token_fresh(bool re_fresh);
// static struct haoxueduo_role *role_last = NULL;
// static bool is_communication = false;

// const char *haoxueduo_speaker_name_get()
// {
//     return g_speaker_name;
// }

// const uint32_t haoxueduo_speaker_id_get()
// {
//     return g_speaker_id;
// }

// const char *role_get_url = "http://staging-api.listenai.com/external/assistant/hxd/get_roles";

// struct response_data {
//     char *buf;
//     uint32_t len;
// };

// static void haoxueduo_roles_info_request_on_data(lisa_http_data_t *data)
// {
//     struct response_data *resp_data = (struct response_data *)data->user;
//     resp_data->buf = lisa_mem_calloc(1, data->len + 1);
//     if (resp_data->buf == NULL) {
//         return;
//     }

//     memcpy(resp_data->buf, data->buf, data->len);
//     resp_data->len = data->len;
// }

// static void *http_client_get_headers(void)
// {
// #define HTTP_REQ_HEADER "Content-Type: application/json"
//     return HTTP_REQ_HEADER;
// }

// static char *_strdup_(const char *str)
// {
//     if (str == NULL) {
//         return NULL;
//     }
//     char *new_str = lisa_mem_calloc(1, strlen(str) + 1);
//     if (new_str == NULL) {
//         return NULL;
//     }
//     strcpy(new_str, str);
//     return new_str;
// }

// static int haoxueduo_role_info_parse(cJSON *info, struct haoxueduo_role_info *role_info)
// {
//     cJSON *desc = cJSON_GetObjectItem(info, "desc");
//     if (desc == NULL) {
//         return -1;
//     }
//     role_info->desc = _strdup_(cJSON_GetStringValue(desc));

//     cJSON *voice_model = cJSON_GetObjectItem(info, "voiceModel");
//     if (voice_model == NULL) {
//         return -1;
//     }

//     role_info->voice_model = _strdup_(cJSON_GetStringValue(voice_model));
//     cJSON *start_text = cJSON_GetObjectItem(info, "startText");
//     if (start_text == NULL) {
//         return -1;
//     }
//     role_info->start_text = _strdup_(cJSON_GetStringValue(start_text));

//     return 0;
// }

// static int haoxueduo_role_parse(cJSON *item, struct haoxueduo_role *role)
// {
//     cJSON *id = cJSON_GetObjectItem(item, "id");
//     if (id == NULL) {
//         return -1;
//     }
//     role->id = cJSON_GetNumberValue(id);

//     cJSON *name = cJSON_GetObjectItem(item, "name");
//     if (name == NULL) {
//         return -1;
//     }

//     role->name = _strdup_(cJSON_GetStringValue(name));

//     cJSON *icon_url = cJSON_GetObjectItem(item, "icon");
//     if (icon_url == NULL) {
//         return -1;
//     }
//     role->icon_url = _strdup_(cJSON_GetStringValue(icon_url));

//     cJSON *info = cJSON_GetObjectItem(item, "info");
//     if (info == NULL) {
//         return -1;
//     }

//     cJSON *token = cJSON_GetObjectItem(item, "token");
//     if (token != NULL) {
//         printf("token: %s\r\n", cJSON_GetStringValue(token));
//         role->token = _strdup_(cJSON_GetStringValue(token));
//     } else {
//         printf("token: NULL\r\n");
//         role->token = NULL;
//     }

//     haoxueduo_role_info_parse(info, &role->info);

//     return 0;
// }

// static int haoxueduo_roles_parse(const char *json_str, struct haoxueduo_role **roles, uint32_t *cnt)
// {
//     cJSON *root = cJSON_Parse(json_str);
//     if (root == NULL) {
//         return -1;
//     }

//     cJSON *rc = cJSON_GetObjectItem(root, "rc");
//     if (rc == NULL) {
//         return -1;
//     }

//     if (strcmp(rc->valuestring, "0") != 0) {
//         return -1;
//     }

//     cJSON *array = cJSON_GetObjectItem(root, "data");
//     if (array == NULL) {
//         return -1;
//     }

//     *cnt = cJSON_GetArraySize(array);
//     if (*cnt == 0) {
//         return -1;
//     }

//     *roles = (struct haoxueduo_role *)lisa_mem_calloc(*cnt, sizeof(struct haoxueduo_role));
//     if (*roles == NULL) {
//         return -1;
//     }

//     for (int i = 0; i < *cnt; i++) {
//         cJSON *item = cJSON_GetArrayItem(array, i);
//         if (item == NULL) {
//             continue;
//         }

//         haoxueduo_role_parse(item, (*roles) + i);
//     }

//     return 0;
// }

// static void haoxueduo_role_info_free(struct haoxueduo_role_info *role_info)
// {
//     if (role_info == NULL) {
//         return;
//     }

//     lisa_mem_free(role_info->desc);
//     lisa_mem_free(role_info->voice_model);
//     lisa_mem_free(role_info->start_text);
// }

// static void haoxueduo_role_free(struct haoxueduo_role *roles)
// {
//     if (roles == NULL) {
//         return;
//     }

//     haoxueduo_role_info_free(&roles->info);

//     lisa_mem_free(roles->icon_url);
//     lisa_mem_free(roles->name);
// }

// void haoxueduo_roles_free(struct haoxueduo_role *roles, uint32_t cnt)
// {
//     for (int i = 0; i < cnt; i++) {
//         haoxueduo_role_free(roles + i);
//     }
//     lisa_mem_free(roles);
// }

// int haoxueduo_roles_get(struct haoxueduo_role **roles, uint32_t *cnt)
// {
//     if (g_roles == NULL) {
//         return -1;
//     }

//     *roles = g_roles;
//     *cnt = g_roles_cnt;

//     return 0;
// }

// static int haoxueduo_roles_request(struct haoxueduo_role **roles, uint32_t *cnt)
// {
//     cJSON *body_root = cJSON_CreateObject();
//     cJSON_AddNumberToObject(body_root, "agent_type", 1);
//     extern const char *get_device_id_str(void);
//     cJSON_AddStringToObject(body_root, "device_id", get_device_id_str());
//     struct response_data resp_data = {0};
//     lisa_http_request_t req;
//     int err;

//     memset(&req, 0, sizeof(lisa_http_request_t));
//     req.method = LISA_HTTP_POST;
//     req.url = (char *)role_get_url;
//     req.timeout = 10;
//     req.on_data = haoxueduo_roles_info_request_on_data;
//     char *req_body = cJSON_Print(body_root);
//     req.headers = (uint8_t *)http_client_get_headers;
//     req.body = req_body;
//     req.body_len = strlen(req_body);
//     req.user = &resp_data;

//     printf("req_body: %s\r\n", req_body);

//     lisa_http_t *http = lisa_http_init(&req);
//     if (http == NULL) {
//         goto exit;
//     }

//     err = lisa_http_perform(http);
//     lisa_http_cleanup(http);
//     if (err != LISA_HTTP_OK) {
//         goto exit;
//     }

//     haoxueduo_roles_parse(resp_data.buf, roles, cnt);

// exit:
//     cJSON_Delete(body_root);
//     cJSON_free(req_body);
//     if (resp_data.buf) {
//         lisa_mem_free(resp_data.buf);
//     }

//     return 0;
// }

// void haoxueduo_role_print(const struct haoxueduo_role *role)
// {
//     printf("name: %s\n", role->name);
//     printf("icon_url: %s\n", role->icon_url);
//     printf("desc: %s\n", role->info.desc);
//     printf("voice_model: %s\n", role->info.voice_model);
//     printf("start_text: %s\n", role->info.start_text);
//     printf("token: %s\n", role->token);
// }

// static struct haoxueduo_role *haoxueduo_role_get(const char *name)
// {
//     if (g_roles == NULL) {
//         return NULL;
//     }

//     for (int i = 0; i < g_roles_cnt; i++) {
//         if (strcmp(g_roles[i].name, name) == 0) {
//             return g_roles + i;
//         }
//     }

//     return NULL;
// }

// void haoxueduo_chat_start_communication(void)
// {
//     app_client_t *client = app_client_get_instance();
//     /* 打开全双工链路 */
//     lisa_aiui_set_interactive_mode(INTER_CONTINUE);
//     assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
//     pa_manager_refresh(PA_MGR_ON, LS_PA_BASE_TIME, "wakeup");
//     app_cloud_wakeup(client->cloud);
// }

// static int haoxueduo_chat_runnable(void *arg)
// {
//     extern bool app_cloud_is_connected();
//     if (!app_cloud_is_connected()) {
//         extern void recongizer_play_audio_id(uint8_t id);
//         recongizer_play_audio_id(TONE_ID_85);
//         LISA_LOGI(TAG, "cloud is not connected");
//         return -1;
//     }

//     char *name = (char *)arg;
//     if (name == NULL) {
//         LISA_LOGE(TAG, "haoxueduo_chat_runnable name is null");
//         return -1;
//     }

//     struct haoxueduo_role *role = haoxueduo_role_get(name);

//     if (role == NULL) {
//         lisa_mem_free(name);
//         LISA_LOGE(TAG, "haoxueduo_chat_start role:%s not found", name);
//         return -1;
//     }
//     lisa_mem_free(name);

//     g_speaker_name = role->info.voice_model;
//     g_speaker_id = role->id;

//     is_communication = true;
//     if (role_last != NULL) {
//         if (role_last->token != NULL && role->token == NULL) {
//             /* disconnect websocket */
//             extern void app_cloud_disconnect();
//             app_token_fresh(false);
//             LISA_LOGI(TAG, "switch role, need to disconnect ws");
//             app_cloud_disconnect();
//             g_token = NULL;
//         } else if (role_last->token == NULL || role->token != NULL) {
//             /* disconnect websocket */
//             extern void app_cloud_disconnect();
//             app_token_fresh(true);
//             LISA_LOGI(TAG, "need to disconnect ws");
//             app_cloud_disconnect();
//             g_token = role->token;
//         }
//     } else {
//         role_last = role;
//         if (role->token != NULL) {
//             app_token_fresh(true);
//             app_cloud_disconnect();
//             g_token = role->token;
//             LISA_LOGI(TAG, "first time need to disconnect ws");
//         } else {
//             LISA_LOGI(TAG, "no need to disconnect ws");
//             haoxueduo_chat_start_communication();
//         }
//     }

//     role_last = role;
// }

// int aiui_get_custom_token(char **token)
// {
//     if (g_token == NULL) {
//         return -1;
//     }
//     *token = lisa_mem_alloc(strlen(g_token) + 1);
//     if (*token == NULL) {
//         return -1;
//     }
//     strcpy(*token, g_token);

//     return 0;
// }

// uint8_t haoxueduo_role_start_voice_id_get(const char *name)
// {
//     struct start_voice {
//         char *name;
//         uint8_t id;
//     };

//     const struct start_voice start_voices[] = {
//         {"Teeni", TONE_ID_95},
//         {"小仙", TONE_ID_102},
//         {"小鹦鹉波力", TONE_ID_97},
//         {"科学家牛顿", TONE_ID_98},
//         {"哪吒", TONE_ID_99},
//         {"孙悟空", TONE_ID_100},
//         {"绘本企鹅佩吉", TONE_ID_101},
//     };

//     for (int i = 0; i < sizeof(start_voices)/ sizeof(start_voices[0]); i++) {
//         if (strcmp(start_voices[i].name, name) == 0) {
//             return start_voices[i].id;
//         }
//     }
// }

// void haoxueduo_role_start_text_play()
// {
//     if (!is_communication) {
//         LISA_LOGI(TAG, "not in communication, do nothing");
//         return;
//     }

//     struct haoxueduo_role *role = role_last;
//     if (role == NULL) {
//         return;
//     }

// #if 1
//     uint8_t id = haoxueduo_role_start_voice_id_get(role->name);
//     extern void recongizer_play_audio_id(uint8_t id);
//     recongizer_play_audio_id(id);
// #else
//     lisa_ui_tts_send_haoxueduo(app_cloud_get()->aiui, role->info.voice_model, role->info.start_text);
// #endif
// }

// int haoxueduo_role_start_runable(void *arg)
// {
//     haoxueduo_chat_start_communication();

//     return 0;
// }

// void haoxueduo_role_start()
// {
//     struct haoxueduo_role *role = role_last;
//     if (role == NULL) {
//         LISA_LOGE(TAG, "haoxueduo_role_start role is null");
//         return;
//     }

//     evs_handler_post_runnable_delay(haoxueduo_role_start_runable, NULL, 0);
// }

// int haoxueduo_chat_start(const char *name)
// {
//     char *name_dup = lisa_mem_alloc(strlen(name) + 1);
//     if (name_dup == NULL) {
//         LISA_LOGE(TAG, "haoxueduo_chat_start name_dup alloc failed");
//         return -1;
//     }
//     strcpy(name_dup, name);
//     evs_handler_post_runnable_delay(haoxueduo_chat_runnable, name_dup, 0);

//     return 0;
// }

// static int haoxueduo_chat_start_shell(int argc, char *argv[])
// {
//     if (argc < 2) {
//         return -1;
//     }

//     haoxueduo_chat_start(argv[1]);

//     return 0;
// }
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
//                  haoxueduo_chat_start, haoxueduo_chat_start_shell, haoxueduo_chat_start);

// int haoxueduo_chat_stop_runable(void *arg)
// {
//     if (!is_communication) {
//         LISA_LOGI(TAG, "not in communication, do nothing");
//         return 0;
//     }

//     is_communication = false;
//     LISA_LOGI(TAG, "haoxueduo_chat_stop_runable, is_communication: %d", is_communication);

//     extern void recognizer_stop_haoxueduo(void);
//     recognizer_stop_haoxueduo();

//     return 0;
// }

// int haoxueduo_chat_stop()
// {
//     evs_handler_post_runnable_delay(haoxueduo_chat_stop_runable, NULL, 0);
//     return 0;
// }

// static int haoxueduo_chat_stop_shell(int argc, char *argv[])
// {
//     haoxueduo_chat_stop();

//     return 0;
// }
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
//                  haoxueduo_chat_stop, haoxueduo_chat_stop_shell, haoxueduo_chat_stop);

// struct haoxueduo_roles_info_request_runnable_arg_t {
//     haoxueduo_roles_info_request_cb_t cb;
//     void *user_data;
// };

// static int haoxueduo_roles_info_request_runnable(void *p)
// {
//     struct haoxueduo_roles_info_request_runnable_arg_t *arg = (struct haoxueduo_roles_info_request_runnable_arg_t *)p;
//     int r = haoxueduo_roles_request(&g_roles, &g_roles_cnt);
//     arg->cb(r, (const struct haoxueduo_role *)g_roles, g_roles_cnt, arg->user_data);
//     lisa_mem_free(arg);

//     return r;
// }

// void haoxueduo_roles_info_request(haoxueduo_roles_info_request_cb_t cb, void *user_data)
// {
//     struct haoxueduo_roles_info_request_runnable_arg_t *arg =
//         lisa_mem_alloc(sizeof(struct haoxueduo_roles_info_request_runnable_arg_t));
//     if (arg == NULL) {
//         cb(-1, NULL, 0, user_data);
//         return;
//     }

//     arg->cb = cb;
//     arg->user_data = user_data;
//     evs_handler_post_runnable(haoxueduo_roles_info_request_runnable, arg);
// }

// static void haoxueduo_roles_info_request_test_cb(int state, const struct haoxueduo_role *roles, uint32_t cnt,
//                                                  void *user_data)
// {
//     if (state != 0) {
//         return;
//     }

//     for (int i = 0; i < cnt; i++) {
//         haoxueduo_role_print(roles + i);
//     }

//     for (int i = 0; i < cnt; i++) {
//         const struct haoxueduo_role *role = roles + i;
//         if (role->info.start_text != NULL && strlen(role->info.start_text) > 0) {
//             lisa_ui_tts_send_haoxueduo(app_cloud_get()->aiui, role->info.voice_model, "");
//             break;
//         }
//     }
// }

// static int get_roles_test(int argc, char *argv[])
// {
//     int r = haoxueduo_roles_request(&g_roles, &g_roles_cnt);
//     if (r != 0) {
//         return r;
//     }

//     for (int i = 0; i < g_roles_cnt; i++) {
//         haoxueduo_role_print(g_roles + i);
//     }

//     for (int i = 0; i < g_roles_cnt; i++) {
//         const struct haoxueduo_role *role = g_roles + i;
//         if (role->info.start_text != NULL && strlen(role->info.start_text) > 0) {
//             lisa_ui_tts_send_haoxueduo(app_cloud_get()->aiui, role->info.voice_model, role->info.start_text);
//             break;
//         }
//     }

//     return 0;
// }

// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
//                  get_roles_test, get_roles_test, get_roles_test);

// static int request_role_runnable(void *arg);
// static void haoxueduo_roles_info_request_cb(int state, const struct haoxueduo_role *roles, uint32_t cnt,
//                                             void *user_data)
// {
//     if (state != 0) {
//         if (roles_get_retry_count++ < ROLES_GET_RETRY_MAX) {

//             evs_handler_post_runnable_delay(request_role_runnable, NULL, ROLSE_GET_RETRY_INTERVAL_MS);
//         } else {
//             printf("Get roles failed,retry:%d", roles_get_retry_count);
//             assist_controller_trigger_event(CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_FAILED, NULL, 0);
//         }
//         return;
//     }

//     roles_get_retry_count = 0;
//     is_has_get_roles = true;
//     printf("Get roles success");
//     assist_controller_trigger_event(CONTROLLER_EVENT_STATE_CLOUD_GET_ROLES_SUCCESS, NULL, 0);
// }

// static int request_role_runnable(void *arg)
// {
//     haoxueduo_roles_info_request(haoxueduo_roles_info_request_cb, NULL);
//     return 0;
// }

// int haoxueduo_check_for_get_roles()
// {
//     if (is_has_get_roles) {
//         return 0;
//     }
//     roles_get_retry_count = 0;
//     evs_handler_post_runnable(request_role_runnable, NULL);

//     return 0;
// }