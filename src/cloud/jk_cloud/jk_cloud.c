#define TAG "jk_cloud"

#include "jk_cloud.h"
#include "jk_asr.h"
#include "jk_llm.h"
#include "jk_tts.h"
#include "jk_pcm_player.h"
#include "app_client.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "assistant_controller.h"
#include "tone.h"
#include "sound_player.h"
#include "cJSON.h"
#include "FreeRTOS.h"
#include "task.h"
#include "aiui_mcp.h"
#include "recognizer.h"
#include "proc_mgr.h"
#include "audio_player.h"
#include <stdio.h>
#include <string.h>

#ifndef CONFIG_MY_CLOUD_HOST
#define CONFIG_MY_CLOUD_HOST "192.168.1.169"
#endif

// AP 每次过来 16ms 数据, 过滤 52 帧 (与 recognizer.c 保持一致)
#define JK_CLOUD_DROP_AUDIO_FRAME_MAX (52)

static jk_cloud_t *s_cloud = NULL;

static int _reconnect_runnable(void *arg);
static void jk_cloud_check_all_connected(void);

static void execute_mcp_tool(const char *tool_name, cJSON *arguments) {
    if (!tool_name) {
        LISA_LOGW(TAG, "Tool call without tool_name");
        return;
    }

    LISA_LOGI(TAG, "Executing MCP tool: %s", tool_name);

    const mcp_tool_def_t *tool = mcp_find_tool_from_section(tool_name);
    if (!tool) {
        LISA_LOGW(TAG, "Tool not found: %s", tool_name);
        return;
    }

    mcp_param_t *params = NULL;
    uint32_t param_count = 0;

    if (arguments && cJSON_IsObject(arguments)) {
        cJSON *item = arguments->child;
        while (item) {
            param_count++;
            item = item->next;
        }

        if (param_count > 0) {
            params = lisa_mem_calloc(param_count, sizeof(mcp_param_t));
            if (!params) {
                LISA_LOGE(TAG, "Failed to allocate params");
                return;
            }

            item = arguments->child;
            uint32_t i = 0;
            while (item && i < param_count) {
                params[i].name = item->string;
                params[i].value = cJSON_Duplicate(item, 1);
                item = item->next;
                i++;
            }
        }
    }

    mcp_response_t response = {0};
    mcp_result_t result = mcp_call_tool_sync(tool_name, params, param_count, &response);

    if (result == MCP_RESULT_SUCCESS) {
        LISA_LOGI(TAG, "Tool %s executed successfully", tool_name);
        if (response.content) {
            char *result_str = cJSON_PrintUnformatted(response.content);
            if (result_str) {
                LISA_LOGI(TAG, "Tool result: %s", result_str);
                lisa_mem_free(result_str);
            }
        }
    } else {
        LISA_LOGE(TAG, "Tool %s execution failed: %d", tool_name, result);
    }

    if (params) {
        for (uint32_t i = 0; i < param_count; i++) {
            if (params[i].value) {
                cJSON_Delete(params[i].value);
            }
        }
        lisa_mem_free(params);
    }

    if (response.content) {
        cJSON_Delete(response.content);
    }
}

static void jk_cloud_check_all_connected(void) {
    if (!s_cloud) return;
    
    if (s_cloud->asr_connected && s_cloud->llm_connected && s_cloud->tts_connected) {
        s_cloud->state = JK_CLOUD_STATE_CONNECTED;
        LISA_LOGI(TAG, "All services connected");
        if (s_cloud->m_client && s_cloud->m_client->sound_player) {
            listen_soundplayer_play(s_cloud->m_client->sound_player, TONE_ID_59, 0);
        }
    }
}

static void on_asr_text_result(jk_asr_t *asr, const char *text, bool is_final) {
    if (!s_cloud || !text) {
        LISA_LOGW(TAG, "ASR result callback: s_cloud=%p, text=%p", s_cloud, text);
        return;
    }
    
    LISA_LOGI(TAG, "ASR result: [%s] (is_final: %d, len=%zu)", text, is_final, strlen(text));
    
    if (is_final && strlen(text) > 0) {
        LISA_LOGI(TAG, "Sending text to LLM: %s", text);
        assist_controller_trigger_event(CONTROLLER_EVENT_STATE_CLOUD_UPDATE_IAT_TEXT, 
                                         (void *)text, strlen(text) + 1);
        jk_llm_send_text(s_cloud->llm, text);
    } else {
        LISA_LOGW(TAG, "Text not sent to LLM: is_final=%d, len=%zu", is_final, strlen(text));
    }
}

static void on_asr_connected(jk_asr_t *asr) {
    LISA_LOGI(TAG, "ASR connected");
    if (s_cloud) {
        s_cloud->asr_connected = true;
        jk_cloud_check_all_connected();
    }
}

static void on_asr_disconnected(jk_asr_t *asr) {
    LISA_LOGW(TAG, "ASR disconnected");
    if (s_cloud) {
        s_cloud->asr_connected = false;
        if (s_cloud->m_wifi_conn && s_cloud->m_ntp_conn) {
            LISA_LOGI(TAG, "Scheduling ASR reconnect in 500ms");
            evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
        }
    }
}

static void on_asr_error(jk_asr_t *asr, const char *error_msg) {
    LISA_LOGE(TAG, "ASR error: %s", error_msg);
    if (s_cloud) {
        s_cloud->asr_connected = false;
        if (s_cloud->m_wifi_conn && s_cloud->m_ntp_conn) {
            LISA_LOGI(TAG, "Scheduling ASR reconnect in 500ms");
            evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
        }
    }
}

static void on_llm_connected(jk_llm_t *llm) {
    LISA_LOGI(TAG, "on_llm_connected: called, llm=%p, s_cloud=%p", llm, s_cloud);
    if (s_cloud) {
        LISA_LOGI(TAG, "LLM connected");
        s_cloud->llm_connected = true;
        jk_cloud_check_all_connected();

        /* v1.2.0 连接建立后注册本地 MCP 工具到云端 */
        LISA_LOGI(TAG, "Starting client tools registration...");

        mcp_tool_def_t **tools = NULL;
        uint32_t tool_count = 0;

        mcp_result_t list_result = mcp_list_tools(&tools, &tool_count);
        LISA_LOGI(TAG, "mcp_list_tools result: %d, tool_count=%d, tools=%p",
                  list_result, tool_count, tools);

        if (list_result == MCP_RESULT_SUCCESS && tools && tool_count > 0) {
            /* 转换为 jk_llm_tool_def_t 格式 */
            jk_llm_tool_def_t *llm_tools = lisa_mem_calloc(tool_count, sizeof(jk_llm_tool_def_t));
            if (llm_tools) {
                int valid_count = 0;
                LISA_LOGI(TAG, "Starting tool processing loop, tool_count=%d", tool_count);
                for (uint32_t i = 0; i < tool_count; i++) {
                    LISA_LOGI(TAG, "Tool[%d]: name=%s, desc=%s",
                              i, tools[i]->name, tools[i]->description ? tools[i]->description : "null");

                    llm_tools[i].name = tools[i]->name;
                    llm_tools[i].description = tools[i]->description;

                    /* 生成 JSON Schema */
                    LISA_LOGI(TAG, "Tool[%d]: getting input_schema...", i);
                    if (tools[i]->input_schema) {
                        cJSON *schema = tools[i]->input_schema();
                        LISA_LOGI(TAG, "Tool[%d]: input_schema returned=%p", i, schema);
                        if (schema) {
                            LISA_LOGI(TAG, "Tool[%d]: calling cJSON_PrintUnformatted...", i);
                            llm_tools[i].parameters = cJSON_PrintUnformatted(schema);
                            LISA_LOGI(TAG, "Tool[%d]: schema generated, len=%zu",
                                      i, llm_tools[i].parameters ? strlen(llm_tools[i].parameters) : 0);
                            cJSON_Delete(schema);
                        } else {
                            LISA_LOGW(TAG, "Tool[%d]: schema is NULL", i);
                        }
                    } else {
                        LISA_LOGW(TAG, "Tool[%d]: input_schema function is NULL", i);
                    }
                    valid_count++;
                    LISA_LOGI(TAG, "Tool[%d]: completed, valid_count=%d", i, valid_count);
                }

                LISA_LOGI(TAG, "Tool processing completed, calling jk_llm_register_tools...");
                /* 发送注册请求 */
                int ret = jk_llm_register_tools(llm, llm_tools, valid_count);
                LISA_LOGI(TAG, "Registering %d tools to cloud: ret=%d", valid_count, ret);

                /* 清理 */
                for (uint32_t i = 0; i < tool_count; i++) {
                    if (llm_tools[i].parameters) {
                        lisa_mem_free((void *)llm_tools[i].parameters);
                    }
                }
                lisa_mem_free(llm_tools);
            }

            lisa_mem_free(tools);
        } else {
            LISA_LOGE(TAG, "Failed to get tools list: result=%d, count=%d, tools=%p",
                      list_result, tool_count, tools);
        }
    }
}

static void on_llm_disconnected(jk_llm_t *llm) {
    LISA_LOGW(TAG, "LLM disconnected");
    if (s_cloud) {
        s_cloud->llm_connected = false;
        if (s_cloud->m_wifi_conn && s_cloud->m_ntp_conn) {
            LISA_LOGI(TAG, "Scheduling LLM reconnect in 500ms");
            evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
        }
    }
}

/* v1.2.0 工具回调处理 */
static void on_llm_tool_callback(jk_llm_t *llm, const char *call_id,
                                  const char *tool_name, const char *arguments) {
    if (!s_cloud || !call_id || !tool_name) return;

    LISA_LOGI(TAG, "Tool callback: %s (call_id=%s)", tool_name, call_id);

    /* 解析参数并执行工具 */
    cJSON *args = NULL;
    if (arguments) {
        args = cJSON_Parse(arguments);
    }

    /* 执行工具并获取结果 */
    mcp_param_t *params = NULL;
    uint32_t param_count = 0;

    if (args && cJSON_IsObject(args)) {
        /* 从 JSON 对象转换为参数数组 */
        cJSON *item;
        int count = 0;
        cJSON_ArrayForEach(item, args) {
            count++;
        }

        if (count > 0) {
            params = lisa_mem_calloc(count, sizeof(mcp_param_t));
            if (params) {
                int i = 0;
                cJSON_ArrayForEach(item, args) {
                    params[i].name = item->string;
                    params[i].value = cJSON_Duplicate(item, 1);
                    i++;
                }
                param_count = count;
            }
        }
    }

    /* 调用工具 */
    mcp_response_t response = {0};
    mcp_result_t result = MCP_RESULT_ERROR;

    const mcp_tool_def_t *tool = mcp_find_tool_from_section(tool_name);
    if (tool) {
        mcp_set_next_call_id(call_id);
        result = mcp_call_tool_sync(tool_name, params, param_count, &response);
    }

    /* 发送结果回服务端 */
    if (result == MCP_RESULT_SUCCESS && response.content) {
        char *result_str = cJSON_PrintUnformatted(response.content);
        jk_llm_send_tool_result(llm, call_id, result_str, true, NULL);
        lisa_mem_free(result_str);
    } else {
        jk_llm_send_tool_result(llm, call_id, NULL, false, "Tool execution failed");
    }

    /* 清理 */
    if (params) {
        for (uint32_t i = 0; i < param_count; i++) {
            if (params[i].value) {
                cJSON_Delete(params[i].value);
            }
        }
        lisa_mem_free(params);
    }

    if (response.content) {
        cJSON_Delete(response.content);
    }

    if (args) {
        cJSON_Delete(args);
    }
}

/* v1.2.0 工具注册确认 */
static void on_llm_tools_registered(jk_llm_t *llm, int count) {
    LISA_LOGI(TAG, "Tools registered: %d tools confirmed by server", count);
}

static void on_llm_message(jk_llm_t *llm, jk_llm_message_t *msg) {
    if (!s_cloud || !msg) return;

    switch (msg->type) {
    case JK_LLM_MSG_TYPE_STATUS:
        LISA_LOGI(TAG, "LLM status: session_id=%s", msg->session_id ? msg->session_id : "null");
        break;

    case JK_LLM_MSG_TYPE_LLM_RESPONSE:
        if (msg->content && strlen(msg->content) > 0) {
            LISA_LOGI(TAG, "LLM response: %s", msg->content);
            assist_controller_trigger_event(CONTROLLER_EVENT_STATE_CLOUD_UPDATE_IAT_TEXT,
                                            (void *)msg->content, strlen(msg->content) +1);

            // Check if any audio playback is active (using new API for better state tracking)
            bool pcm_active = jk_pcm_player_is_playing(s_cloud->pcm_player) ||
                             jk_pcm_player_is_preparing(s_cloud->pcm_player);
            bool tts_active = jk_tts_is_playing(s_cloud->tts);

            if (pcm_active || tts_active) {
                LISA_LOGI(TAG, "Previous TTS still playing (pcm=%d tts=%d), stopping it",
                          pcm_active, tts_active);
                jk_pcm_player_stop(s_cloud->pcm_player);

                // Reset TTS state to CONNECTED to allow new request
                // Old session data will be discarded by request_id filtering
                s_cloud->tts->data_complete = true;
                s_cloud->tts->state = JK_TTS_STATE_CONNECTED;  // Reset to CONNECTED
                s_cloud->tts->has_pending = false;  // Clear any pending
                LISA_LOGI(TAG, "Old TTS stopped and state reset to CONNECTED");
            }

            // Check if music player is active (preparing, prepared, playing, or paused)
            // If so, skip TTS to avoid interrupting music playback
            audioplayer_t *audio_player = get_audio_player();
            bool music_active = false;
            if (audio_player) {
                PlayerEvt music_state = listen_audioplayer_get_state(audio_player);
                // APP_PLAYER_PREPARING = 101, check for active/preparing states
                if (music_state == PLAYER_EVT_PREPARED ||
                    music_state == PLAYER_EVT_PLAYING ||
                    music_state == PLAYER_EVT_PAUSED ||
                    music_state == 101) {  // APP_PLAYER_PREPARING
                    music_active = true;
                    LISA_LOGI(TAG, "Music player is active (state=%d), skipping TTS to avoid interrupting playback", music_state);
                }
            }

            if (!music_active) {
                // Release CONTENT channel to allow TTS to get focus
                LISA_LOGI(TAG, "Releasing CONTENT channel before TTS request");
                listen_audiomgr_release_channel(s_cloud->m_client->audio_mgr, CONTENT);

                // Acquire TTS channel focus before sending TTS request
                LISA_LOGI(TAG, "Acquiring TTS channel focus before TTS request");
                listen_audiomgr_acquire_channel(s_cloud->m_client->audio_mgr, TTS);

                // Check if TTS is connected, if not, trigger reconnect
                if (!s_cloud->tts_connected || s_cloud->tts->state == JK_TTS_STATE_DISCONNECTED) {
                    LISA_LOGW(TAG, "TTS disconnected (tts_connected=%d, state=%d), triggering reconnect",
                              s_cloud->tts_connected, s_cloud->tts->state);
                    evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 100);
                    // Don't send TTS request - wait for reconnect to complete
                    // The text will be lost, but user can ask again after reconnect
                    break;
                }

                s_cloud->drop_frame_count = 0;
                jk_tts_request(s_cloud->tts, msg->content, NULL);
                assist_controller_trigger_event(CONTROLLER_EVENT_STATE_TTS_PLAY_START, NULL, 0);
            } else {
                LISA_LOGI(TAG, "Skipping TTS request due to active music playback");
            }
        }
        break;

    case JK_LLM_MSG_TYPE_TOOL_CALL:
        /* tool_call 带有 result 字段表示服务端已执行过工具，无需再执行 */
        if (msg->result) {
            LISA_LOGI(TAG, "Tool call: %s (server executed, skipping)", msg->tool_name ? msg->tool_name : "null");
        } else {
            LISA_LOGI(TAG, "Tool call: %s", msg->tool_name ? msg->tool_name : "null");
            cJSON *args = NULL;
            if (msg->arguments) {
                args = cJSON_Parse(msg->arguments);
            }
            execute_mcp_tool(msg->tool_name, args);
            if (args) {
                cJSON_Delete(args);
            }
        }
        break;

    case JK_LLM_MSG_TYPE_ERROR:
        LISA_LOGE(TAG, "LLM error: %s - %s", 
                  msg->error_code ? msg->error_code : "unknown",
                  msg->error_message ? msg->error_message : "no message");
        break;

    case JK_LLM_MSG_TYPE_PONG:
        LISA_LOGD(TAG, "LLM pong received");
        break;

    /* TOOL_CALLBACK 和 TOOLS_REGISTERED 由专用回调处理，无需在此处理 */
    }
}

static void on_llm_error(jk_llm_t *llm, const char *error_msg) {
    LISA_LOGE(TAG, "LLM error: %s", error_msg);
    if (s_cloud) {
        s_cloud->llm_connected = false;
    }
}

static void on_tts_connected(jk_tts_t *tts) {
    LISA_LOGI(TAG, "TTS connected");
    if (s_cloud) {
        s_cloud->tts_connected = true;
        jk_cloud_check_all_connected();
    }
}

static void on_tts_disconnected(jk_tts_t *tts) {
    LISA_LOGW(TAG, "TTS disconnected");
    if (s_cloud) {
        s_cloud->tts_connected = false;
        // Trigger auto-reconnect when TTS disconnects unexpectedly
        if (s_cloud->m_wifi_conn && s_cloud->m_ntp_conn) {
            LISA_LOGI(TAG, "Triggering TTS auto-reconnect");
            evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
        }
    }
}

static void on_tts_audio_data(jk_tts_t *tts, int16_t *samples, uint32_t count, jk_tts_metadata_t *meta) {
    if (!s_cloud || !samples || !s_cloud->pcm_player) {
        LISA_LOGW(TAG, "TTS audio data: invalid params s_cloud=%p, samples=%p, pcm_player=%p",
                  s_cloud, samples, s_cloud ? s_cloud->pcm_player : NULL);
        return;
    }

    if (!jk_tts_is_playing(tts)) {
        LISA_LOGW(TAG, "TTS not playing (state=%d), discarding audio data", tts->state);
        return;
    }

    // IMPORTANT: Start PCM player when first audio data arrives
    // This sets is_playing=true in the PCM player, allowing data to be written
    // Use new API to check if player is already active
    if (!jk_pcm_player_is_playing(s_cloud->pcm_player)) {
        LISA_LOGI(TAG, "Starting PCM player for TTS audio (prepared=%d, preparing=%d)",
                  jk_pcm_player_is_prepared(s_cloud->pcm_player),
                  jk_pcm_player_is_preparing(s_cloud->pcm_player));
        jk_pcm_player_start(s_cloud->pcm_player);
    }

    static int audio_count = 0;
    if (audio_count++ < 5) {
        LISA_LOGI(TAG, "TTS audio: %u samples, sample_rate: %d, is_final: %d (count=%d)",
                  count, meta->sample_rate, meta->is_final, audio_count);
    }

    // Retry mechanism for player handle recovery
    // ret = -2 means recovery happened and retry is recommended
    int ret = jk_pcm_player_write(s_cloud->pcm_player, samples, count);
    if (ret == -2 && jk_tts_is_playing(tts)) {
        // Recovery occurred, retry the write once
        LISA_LOGI(TAG, "Retrying PCM write after recovery");
        ret = jk_pcm_player_write(s_cloud->pcm_player, samples, count);
    }

    if (ret != 0) {
        // Only log error if TTS is still in playing state
        // Late-arriving frames after playback completes are expected and should be silently discarded
        if (jk_tts_is_playing(tts)) {
            LISA_LOGE(TAG, "Failed to write PCM data to player: ret=%d", ret);
        } else {
            LISA_LOGD(TAG, "Discarding late TTS audio after playback complete");
        }
    }
}

static void on_tts_complete(jk_tts_t *tts) {
    LISA_LOGI(TAG, "TTS data complete, waiting for playback to finish");
    
    if (s_cloud->pcm_player) {
        jk_pcm_player_end_stream(s_cloud->pcm_player);
    }
}

static void on_tts_error(jk_tts_t *tts, const char *error_msg) {
    LISA_LOGE(TAG, "TTS error: %s", error_msg);
    if (s_cloud) {
        s_cloud->tts_connected = false;
        // Reset TTS state on error
        s_cloud->tts->state = JK_TTS_STATE_DISCONNECTED;
        s_cloud->tts->data_complete = false;
        if (s_cloud->pcm_player) {
            jk_pcm_player_stop(s_cloud->pcm_player);
        }
    }
}

static void on_pcm_play_start(jk_pcm_player_t *player) {
    LISA_LOGI(TAG, "PCM playback started");
}

static void on_pcm_play_complete(jk_pcm_player_t *player) {
    uint32_t total_written = jk_pcm_player_get_buffered(player);
    LISA_LOGI(TAG, "PCM playback completed, total=%u bytes", total_written);

    // Reset TTS state to CONNECTED when playback naturally completes
    // This callback is NOT called for manual stops (PLAYER_EVT_STOPPED)
    if (s_cloud && s_cloud->tts && s_cloud->tts->state == JK_TTS_STATE_PLAYING) {
        s_cloud->tts->state = JK_TTS_STATE_CONNECTED;
        LISA_LOGI(TAG, "TTS state reset to CONNECTED after playback complete");

        // Check if there's a pending request to send
        if (s_cloud->tts->has_pending) {
            LISA_LOGI(TAG, "Found pending TTS request, sending it now");
            jk_tts_check_pending(s_cloud->tts);
        }
    }

    // Auto stop recording after TTS playback if enabled
    bool auto_stop = recognizer_get_auto_stop_record();
    LISA_LOGI(TAG, "Auto stop record check: enabled=%d, is_recording=%d",
              auto_stop, s_cloud ? s_cloud->is_recording : false);
    if (s_cloud && auto_stop && s_cloud->is_recording) {
        LISA_LOGI(TAG, "Auto stop: setting is_recording=false");
        s_cloud->is_recording = false;
    }

    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_TTS_PLAY_FINISH, NULL, 0);
}

static void on_pcm_play_error(jk_pcm_player_t *player, const char *error) {
    LISA_LOGE(TAG, "PCM playback error: %s", error);

    // Reset TTS state on error as well
    if (s_cloud && s_cloud->tts) {
        s_cloud->tts->state = JK_TTS_STATE_CONNECTED;
        LISA_LOGI(TAG, "TTS state reset to CONNECTED after playback error");

        // Check if there's a pending request to send
        if (s_cloud->tts->has_pending) {
            LISA_LOGI(TAG, "Found pending TTS request after error, sending it now");
            jk_tts_check_pending(s_cloud->tts);
        }
    }
}

jk_cloud_t *jk_cloud_create(app_client_t *client) {
    if (!client) return NULL;

    jk_cloud_t *cloud = lisa_mem_calloc(1, sizeof(jk_cloud_t));
    if (!cloud) {
        LISA_LOGE(TAG, "Failed to allocate");
        return NULL;
    }

    cloud->m_client = client;
    cloud->host = strdup(CONFIG_MY_CLOUD_HOST);
    cloud->state = JK_CLOUD_STATE_DISCONNECTED;
    cloud->asr_connected = false;
    cloud->llm_connected = false;
    cloud->tts_connected = false;
    cloud->is_recording = false;
    cloud->drop_frame_count = 0;
    s_cloud = cloud;

    /* 初始化 MCP 框架并注册静态工具 */
    mcp_init();
    mcp_init_static_tools();
    LISA_LOGI(TAG, "MCP tools initialized");

    cloud->m_rec = recognizer_create(client->short_player, client->tts_player, client->audio_mgr, NULL);

    jk_asr_callbacks_t asr_cbs = {
        .on_connected = on_asr_connected,
        .on_disconnected = on_asr_disconnected,
        .on_text_result = on_asr_text_result,
        .on_error = on_asr_error,
    };
    cloud->asr = jk_asr_create(cloud->host, "9200", &asr_cbs);
    if (!cloud->asr) {
        LISA_LOGE(TAG, "Failed to create ASR");
        goto cleanup;
    }

    jk_llm_callbacks_t llm_cbs = {
        .on_connected = on_llm_connected,
        .on_disconnected = on_llm_disconnected,
        .on_message = on_llm_message,
        .on_error = on_llm_error,
        .on_tool_callback = on_llm_tool_callback,
        .on_tools_registered = on_llm_tools_registered,
    };
    cloud->llm = jk_llm_create(cloud->host, "9400", &llm_cbs);
    if (!cloud->llm) {
        LISA_LOGE(TAG, "Failed to create LLM");
        goto cleanup;
    }

    jk_tts_callbacks_t tts_cbs = {
        .on_connected = on_tts_connected,
        .on_disconnected = on_tts_disconnected,
        .on_audio_data = on_tts_audio_data,
        .on_complete = on_tts_complete,
        .on_error = on_tts_error,
    };
    cloud->tts = jk_tts_create(cloud->host, "9300", &tts_cbs);
    if (!cloud->tts) {
        LISA_LOGE(TAG, "Failed to create TTS");
        goto cleanup;
    }

    jk_pcm_player_callbacks_t pcm_cbs = {
        .on_play_start = on_pcm_play_start,
        .on_play_complete = on_pcm_play_complete,
        .on_error = on_pcm_play_error,
    };
    cloud->pcm_player = jk_pcm_player_create(client->audio_mgr, &pcm_cbs);
    if (!cloud->pcm_player) {
        LISA_LOGE(TAG, "Failed to create PCM player");
        goto cleanup;
    }

    LISA_LOGI(TAG, "jk_cloud created [host=%s]", cloud->host);
    return cloud;

cleanup:
    if (cloud->asr) jk_asr_destroy(cloud->asr);
    if (cloud->llm) jk_llm_destroy(cloud->llm);
    if (cloud->tts) jk_tts_destroy(cloud->tts);
    if (cloud->pcm_player) jk_pcm_player_destroy(cloud->pcm_player);
    if (cloud->host) lisa_mem_free(cloud->host);
    lisa_mem_free(cloud);
    s_cloud = NULL;
    return NULL;
}

void jk_cloud_destroy(jk_cloud_t *cloud) {
    if (!cloud) return;

    if (cloud->pcm_player) jk_pcm_player_destroy(cloud->pcm_player);
    jk_asr_destroy(cloud->asr);
    jk_llm_destroy(cloud->llm);
    jk_tts_destroy(cloud->tts);
    if (cloud->host) lisa_mem_free(cloud->host);
    lisa_mem_free(cloud);
    s_cloud = NULL;
}

static int _connect_all_runnable(void *arg) {
    LISA_LOGI(TAG, "_connect_all_runnable: called, s_cloud=%p", s_cloud);
    if (!s_cloud) {
        LISA_LOGE(TAG, "_connect_all_runnable: s_cloud is NULL!");
        return -1;
    }

    LISA_LOGI(TAG, "Connecting to all services [host=%s]...", s_cloud->host);

    s_cloud->asr_connected = false;
    s_cloud->llm_connected = false;
    s_cloud->tts_connected = false;
    s_cloud->state = JK_CLOUD_STATE_CONNECTING;

    int ret = 0;

    LISA_LOGI(TAG, "_connect_all_runnable: calling jk_asr_connect, asr=%p", s_cloud->asr);
    if (jk_asr_connect(s_cloud->asr) != 0) {
        LISA_LOGE(TAG, "Failed to start ASR connection");
        ret = -1;
    } else {
        LISA_LOGI(TAG, "_connect_all_runnable: jk_asr_connect returned successfully");
    }

    LISA_LOGI(TAG, "_connect_all_runnable: calling jk_llm_connect, llm=%p", s_cloud->llm);
    if (jk_llm_connect(s_cloud->llm) != 0) {
        LISA_LOGE(TAG, "Failed to start LLM connection");
        ret = -1;
    } else {
        LISA_LOGI(TAG, "_connect_all_runnable: jk_llm_connect returned successfully");
    }

    LISA_LOGI(TAG, "_connect_all_runnable: calling jk_tts_connect, tts=%p", s_cloud->tts);
    if (jk_tts_connect(s_cloud->tts) != 0) {
        LISA_LOGE(TAG, "Failed to start TTS connection");
        ret = -1;
    } else {
        LISA_LOGI(TAG, "_connect_all_runnable: jk_tts_connect returned successfully");
    }

    if (ret != 0) {
        LISA_LOGW(TAG, "_connect_all_runnable: some connections failed, scheduling reconnect in 500ms");
        evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
    } else {
        LISA_LOGI(TAG, "_connect_all_runnable: all connect calls completed");
    }

    return ret;
}

static int _reconnect_runnable(void *arg) {
    if (!s_cloud) return -1;

    // Check if all services are already connected
    if (s_cloud->asr_connected && s_cloud->llm_connected && s_cloud->tts_connected) {
        return 0;  // All connected, no need to reconnect
    }

    if (!s_cloud->m_wifi_conn || !s_cloud->m_ntp_conn) {
        LISA_LOGI(TAG, "Network not ready, waiting...");
        evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
        return 0;
    }

    // Only reconnect services that are not connected
    LISA_LOGI(TAG, "Reconnecting disconnected services (asr=%d, llm=%d, tts=%d)",
              s_cloud->asr_connected, s_cloud->llm_connected, s_cloud->tts_connected);

    int ret = 0;
    if (!s_cloud->asr_connected) {
        LISA_LOGI(TAG, "Reconnecting ASR...");
        s_cloud->state = JK_CLOUD_STATE_CONNECTING;
        if (jk_asr_connect(s_cloud->asr) != 0) {
            LISA_LOGE(TAG, "Failed to reconnect ASR");
            ret = -1;
        }
    }

    if (!s_cloud->llm_connected) {
        LISA_LOGI(TAG, "Reconnecting LLM...");
        if (s_cloud->state == JK_CLOUD_STATE_CONNECTED) {
            s_cloud->state = JK_CLOUD_STATE_CONNECTING;
        }
        if (jk_llm_connect(s_cloud->llm) != 0) {
            LISA_LOGE(TAG, "Failed to reconnect LLM");
            ret = -1;
        }
    }

    if (!s_cloud->tts_connected) {
        LISA_LOGI(TAG, "Reconnecting TTS...");
        if (s_cloud->state == JK_CLOUD_STATE_CONNECTED) {
            s_cloud->state = JK_CLOUD_STATE_CONNECTING;
        }
        if (jk_tts_connect(s_cloud->tts) != 0) {
            LISA_LOGE(TAG, "Failed to reconnect TTS");
            ret = -1;
        }
    }

    if (ret != 0) {
        // Schedule another retry if any reconnection failed
        evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 500);
    }

    return ret;
}

void jk_cloud_process_wifi_connected(jk_cloud_t *cloud) {
    LISA_LOGI(TAG, "jk_cloud_process_wifi_connected: called, cloud=%p", cloud);
    if (!cloud) {
        LISA_LOGE(TAG, "jk_cloud_process_wifi_connected: cloud is NULL!");
        return;
    }

    LISA_LOGI(TAG, "jk_cloud_process_wifi_connected: setting m_wifi_conn=true");
    cloud->m_wifi_conn = true;
    LISA_LOGI(TAG, "WiFi connected, attempting to connect cloud");

    LISA_LOGI(TAG, "jk_cloud_process_wifi_connected: posting _connect_all_runnable to event handler");
    evs_handler_post_runnable(_connect_all_runnable, NULL);
    LISA_LOGI(TAG, "jk_cloud_process_wifi_connected: _connect_all_runnable posted");
}

void jk_cloud_process_wifi_disconnected(jk_cloud_t *cloud) {
    if (!cloud) return;

    cloud->m_wifi_conn = false;
    cloud->state = JK_CLOUD_STATE_DISCONNECTED;
    cloud->asr_connected = false;
    cloud->llm_connected = false;
    cloud->tts_connected = false;
    LISA_LOGI(TAG, "WiFi disconnected");

    if (cloud->pcm_player) {
        jk_pcm_player_stop(cloud->pcm_player);
    }
    jk_asr_disconnect(cloud->asr);
    jk_llm_disconnect(cloud->llm);
    jk_tts_disconnect(cloud->tts);
}

bool jk_cloud_is_connected(void) {
    if (!s_cloud) {
        LISA_LOGW(TAG, "is_connected: s_cloud is NULL");
        return false;
    }
    LISA_LOGI(TAG, "is_connected: state=%d, asr=%d, llm=%d, tts=%d, wifi=%d",
              s_cloud->state, s_cloud->asr_connected, s_cloud->llm_connected,
              s_cloud->tts_connected, s_cloud->m_wifi_conn);
    return s_cloud->state == JK_CLOUD_STATE_CONNECTED;
}

bool jk_cloud_is_wifi_connected(void) {
    if (!s_cloud) return false;
    return s_cloud->m_wifi_conn;
}

void jk_cloud_audio(jk_cloud_t *cloud, const char *audio, uint32_t len) {
    if (!cloud || !audio || len == 0) {
        LISA_LOGE(TAG, "jk_cloud_audio: invalid params cloud=%p audio=%p len=%u", cloud, audio, len);
        return;
    }
    if (!cloud->is_recording) {
        LISA_LOGD(TAG, "jk_cloud_audio: not recording");
        return;
    }
    if (cloud->state != JK_CLOUD_STATE_CONNECTED) {
        LISA_LOGD(TAG, "jk_cloud_audio: not connected, state=%d", cloud->state);
        return;
    }
    if (!cloud->asr_connected) {
        LISA_LOGE(TAG, "jk_cloud_audio: ASR not connected");
        return;
    }

    // Drop first few frames to avoid unstable audio at start
    cloud->drop_frame_count++;
    if (cloud->drop_frame_count < JK_CLOUD_DROP_AUDIO_FRAME_MAX) {
        if (cloud->drop_frame_count == 1) {
            LISA_LOGI(TAG, "jk_cloud_audio: dropping first %d frames (frame %d/%d)",
                      JK_CLOUD_DROP_AUDIO_FRAME_MAX, cloud->drop_frame_count, JK_CLOUD_DROP_AUDIO_FRAME_MAX);
        } else if (cloud->drop_frame_count == JK_CLOUD_DROP_AUDIO_FRAME_MAX - 1) {
            LISA_LOGI(TAG, "jk_cloud_audio: about to send first frame (drop count: %d/%d)",
                      cloud->drop_frame_count, JK_CLOUD_DROP_AUDIO_FRAME_MAX);
        }
        return;
    }

    if (cloud->drop_frame_count == JK_CLOUD_DROP_AUDIO_FRAME_MAX) {
        LISA_LOGI(TAG, "jk_cloud_audio: first frame sending (threshold reached: %d/%d), len=%u",
                  cloud->drop_frame_count, JK_CLOUD_DROP_AUDIO_FRAME_MAX, len);
    } else {
        static int send_count = 0;
        if (send_count++ < 5) {
            LISA_LOGI(TAG, "jk_cloud_audio: sending audio len=%u (count=%d)", len, send_count);
        }
    }
    jk_asr_send_audio(cloud->asr, (const int16_t *)audio, len / sizeof(int16_t));
}

void jk_cloud_wakeup(jk_cloud_t *cloud) {
    if (!cloud) {
        LISA_LOGW(TAG, "jk_cloud_wakeup: cloud is NULL");
        return;
    }

    LISA_LOGI(TAG, "Wakeup triggered: is_recording=%d->true, state=%d, asr_connected=%d",
              cloud->is_recording, cloud->state, cloud->asr_connected);
    cloud->is_recording = true;
    cloud->drop_frame_count = 0;
    cloud->accumulated_text[0] = '\0';

    if (cloud->m_rec) {
        LISA_LOGI(TAG, "Wakeup: calling recognizer_recognize");
        recognizer_recognize(cloud->m_rec);
    } else {
        LISA_LOGE(TAG, "Wakeup: m_rec is NULL!");
    }

    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
}

void jk_cloud_tts(const char *text) {
    if (!s_cloud || !text) return;
    if (s_cloud->state != JK_CLOUD_STATE_CONNECTED) {
        LISA_LOGW(TAG, "Cloud not connected, cannot TTS");
        return;
    }

    // Check if TTS is connected
    if (!s_cloud->tts_connected || s_cloud->tts->state == JK_TTS_STATE_DISCONNECTED) {
        LISA_LOGW(TAG, "TTS disconnected, triggering reconnect");
        evs_handler_post_runnable_delay(_reconnect_runnable, NULL, 100);
        return;
    }

    // Stop previous TTS if still playing (new TTS has priority)
    // Use new API for better state detection
    bool pcm_active = jk_pcm_player_is_playing(s_cloud->pcm_player) ||
                     jk_pcm_player_is_preparing(s_cloud->pcm_player);
    bool tts_active = jk_tts_is_playing(s_cloud->tts);

    if (pcm_active || tts_active) {
        LISA_LOGI(TAG, "Stopping previous TTS for new request (pcm_active=%d, tts_active=%d)",
                  pcm_active, tts_active);
        jk_pcm_player_stop(s_cloud->pcm_player);
        // Reset state to allow new request
        s_cloud->tts->data_complete = true;
        s_cloud->tts->state = JK_TTS_STATE_CONNECTED;
        s_cloud->tts->has_pending = false;
    }

    s_cloud->drop_frame_count = 0;
    jk_tts_request(s_cloud->tts, text, NULL);
}

void jk_cloud_txt(const char *txt) {
    if (!s_cloud || !txt) return;
    if (s_cloud->state != JK_CLOUD_STATE_CONNECTED) {
        LISA_LOGW(TAG, "Cloud not connected");
        return;
    }

    jk_llm_send_text(s_cloud->llm, txt);
}

void jk_cloud_connect(void) {
    if (!s_cloud) return;
    s_cloud->state = JK_CLOUD_STATE_CONNECTING;
    evs_handler_post_runnable(_connect_all_runnable, NULL);
}

void jk_cloud_disconnect(void) {
    if (!s_cloud) return;

    s_cloud->state = JK_CLOUD_STATE_DISCONNECTED;
    s_cloud->asr_connected = false;
    s_cloud->llm_connected = false;
    s_cloud->tts_connected = false;
    
    if (s_cloud->pcm_player) {
        jk_pcm_player_stop(s_cloud->pcm_player);
    }
    jk_asr_disconnect(s_cloud->asr);
    jk_llm_disconnect(s_cloud->llm);
    jk_tts_disconnect(s_cloud->tts);
}

void jk_cloud_ntp_ok(void *arg) {
    if (s_cloud) {
        s_cloud->m_ntp_conn = true;
        LISA_LOGI(TAG, "NTP synced");
    }
}

jk_cloud_t *jk_cloud_get_instance(void) {
    return s_cloud;
}

int jk_cloud_img_recognition(uint16_t *rgb565_datas, uint32_t width, uint32_t height) {
    return -1;
}
