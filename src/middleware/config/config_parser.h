#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char** items;
    size_t count;
} StringArray;

typedef struct {
    char* name;
    char* url;
} AuthConfig;

typedef struct {
    char* host;
    char* port;
    char* path;
    char* scheme;
} WebSocketConfig;

typedef struct {
    char* name;
    char* pid;
    char* sid;
    char* app_id;
    char* app_key;
    AuthConfig auth;
    WebSocketConfig websocket;
} CloudConfig;

typedef struct {
    bool enable;
    bool keywords_filter;
    StringArray keywords;
} VoiceWakeUpConfig;

typedef struct {
    bool enable;
    char* mode;
} ButtonWakeUpConfig;

typedef struct {
    VoiceWakeUpConfig voice;
    ButtonWakeUpConfig button;
} WakeUpConfig;

typedef struct {
    char* text;
    int tone_id;
} HelloConfig;

typedef struct {
    char* name;
    char* prompt;
    HelloConfig hello;
} RoleConfig;

typedef struct {
    StringArray servers;
    uint32_t timeout_ms;
    uint32_t retry_interval_ms;
    uint32_t retry_times;
} DnsConfig;

typedef struct {
    StringArray servers;
    char* timezone;
    uint32_t timeout_ms;
    uint32_t retry_interval_ms;
    uint32_t retry_times;
} SntpConfig;

typedef struct {
    DnsConfig dns;
    SntpConfig sntp;
} NetworkConfig;

typedef struct {
    char* name;
    bool check;
    uint32_t address;
    uint32_t size;
    uint32_t crc32;
} ResourceConfig;

typedef struct {
    ResourceConfig* items;
    size_t count;
} ResourceArray;

typedef struct {
    struct {
        CloudConfig cloud;
    } chat;
} AbilitiesConfig;

typedef struct {
    char* name;
    char* version;
    AbilitiesConfig abilities;
    WakeUpConfig wake_up;
    RoleConfig role;
    NetworkConfig network;
    ResourceArray resources;
} Config;

// Parse JSON string into Config structure
const Config* config_parse(const char* json_str);

// Free memory allocated by config_parse
void config_free(Config* config);

// Print configuration for debugging
void config_print(const Config* config);

// Get global configuration instance
const Config* config_get(void);

// Initialize global configuration
const Config* config_init(const char* config_json_str);
const ResourceConfig *config_get_resource_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_PARSER_H
