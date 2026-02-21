#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "jk_websocket.h"

static volatile int running = 1;

static void signal_handler(int sig) {
    running = 0;
}

static void on_asr_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    const char *name = (const char *)user;
    switch (event) {
        case JK_WS_EVENT_CONNECTED:
            printf("[%s] CONNECTED\n", name);
            break;
        case JK_WS_EVENT_DISCONNECTED:
            printf("[%s] DISCONNECTED\n", name);
            break;
        case JK_WS_EVENT_ERROR:
            printf("[%s] ERROR\n", name);
            break;
    }
}

static void on_asr_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    const char *name = (const char *)user;
    printf("[%s] Received %s data (%u bytes): %.*s\n", 
           name, type == JK_WS_DATA_TEXT ? "text" : "binary", len, len, (const char *)data);
}

static void on_llm_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    on_asr_event(ws, event, user);
}

static void on_llm_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    on_asr_data(ws, type, data, len, user);
}

static void on_tts_event(jk_websocket_t *ws, jk_ws_event_type_e event, void *user) {
    on_asr_event(ws, event, user);
}

static void on_tts_data(jk_websocket_t *ws, jk_ws_data_type_e type,
                        const void *data, uint32_t len, void *user) {
    const char *name = (const char *)user;
    printf("[%s] Received %s data (%u bytes)\n", 
           name, type == JK_WS_DATA_TEXT ? "text" : "binary", len);
}

int main(int argc, char *argv[]) {
    const char *host = "192.168.1.169";
    
    if (argc > 1) {
        host = argv[1];
    }
    
    printf("=== WebSocket Connection Test ===\n");
    printf("Server: %s\n", host);
    printf("ASR:  ws://%s:9200/\n", host);
    printf("LLM:  ws://%s:9400/\n", host);
    printf("TTS:  ws://%s:9300/tts\n", host);
    printf("=================================\n\n");
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    jk_ws_config_t asr_config = {
        .host = host,
        .port = "9200",
        .path = "/",
        .timeout_ms = 30000,
        .user = "ASR",
        .on_event = on_asr_event,
        .on_data = on_asr_data,
    };
    
    jk_ws_config_t llm_config = {
        .host = host,
        .port = "9400",
        .path = "/",
        .timeout_ms = 60000,
        .user = "LLM",
        .on_event = on_llm_event,
        .on_data = on_llm_data,
    };
    
    jk_ws_config_t tts_config = {
        .host = host,
        .port = "9300",
        .path = "/tts",
        .timeout_ms = 60000,
        .user = "TTS",
        .on_event = on_tts_event,
        .on_data = on_tts_data,
    };
    
    printf("Creating WebSocket clients...\n");
    
    jk_websocket_t *asr = jk_ws_create(&asr_config);
    jk_websocket_t *llm = jk_ws_create(&llm_config);
    jk_websocket_t *tts = jk_ws_create(&tts_config);
    
    if (!asr || !llm || !tts) {
        printf("Failed to create WebSocket clients\n");
        return 1;
    }
    
    printf("\n--- Testing ASR Connection ---\n");
    if (jk_ws_connect(asr) == 0) {
        printf("ASR connection test: PASSED\n");
    } else {
        printf("ASR connection test: FAILED\n");
    }
    
    printf("\n--- Testing LLM Connection ---\n");
    if (jk_ws_connect(llm) == 0) {
        printf("LLM connection test: PASSED\n");
    } else {
        printf("LLM connection test: FAILED\n");
    }
    
    printf("\n--- Testing TTS Connection ---\n");
    if (jk_ws_connect(tts) == 0) {
        printf("TTS connection test: PASSED\n");
    } else {
        printf("TTS connection test: FAILED\n");
    }
    
    if (jk_ws_is_connected(llm)) {
        printf("\n--- Testing LLM Text Send ---\n");
        const char *test_msg = "{\"type\":\"ping\"}";
        if (jk_ws_send_text(llm, test_msg) == 0) {
            printf("Sent: %s\n", test_msg);
            printf("Waiting for response...\n");
            for (int i = 0; i < 10 && running; i++) {
                jk_ws_run_loop(llm, 1000);
            }
        }
    }
    
    printf("\nCleaning up...\n");
    jk_ws_destroy(asr);
    jk_ws_destroy(llm);
    jk_ws_destroy(tts);
    
    printf("Test completed.\n");
    return 0;
}
