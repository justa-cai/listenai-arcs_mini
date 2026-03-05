#include "config_parser.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysheap.h"

// Helper function to duplicate a string
static char *strdup_safe(const char *str)
{
    if (!str) {
        return NULL;
    }
    char *new_str = psram_malloc(strlen(str) + 1);
    if (new_str) {
        strcpy(new_str, str);
    }
    return new_str;
}

// Helper function to parse a string array
static StringArray parse_string_array(const cJSON *array_json)
{
    StringArray result = {0};
    if (!cJSON_IsArray(array_json)) {
        return result;
    }

    size_t count = cJSON_GetArraySize(array_json);
    if (count == 0) {
        return result;
    }

    char **items = psram_malloc(count * sizeof(char *));
    if (!items) {
        return result;
    }

    size_t valid_count = 0;
    for (size_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array_json, i);
        if (cJSON_IsString(item)) {
            items[valid_count] = strdup_safe(item->valuestring);
            if (items[valid_count]) {
                valid_count++;
            }
        }
    }

    result.items = items;
    result.count = valid_count;
    return result;
}

// Helper function to free a string array
static void free_string_array(StringArray *array)
{
    if (!array || !array->items) {
        return;
    }

    for (size_t i = 0; i < array->count; i++) {
        if (array->items[i]) {
            psram_free(array->items[i]);
        }
    }
    psram_free(array->items);
    array->items = NULL;
    array->count = 0;
}

// Parse a single resource configuration
static ResourceConfig parse_resource_config(const cJSON *resource_json)
{
    ResourceConfig resource = {0};
    if (!resource_json) {
        return resource;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(resource_json, "name");
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(resource_json, "check");
    const cJSON *address = cJSON_GetObjectItemCaseSensitive(resource_json, "address");
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(resource_json, "size");
    const cJSON *crc32 = cJSON_GetObjectItemCaseSensitive(resource_json, "crc32");

    if (cJSON_IsString(name)) {
        resource.name = strdup_safe(name->valuestring);
    }

    if (cJSON_IsBool(check)) {
        resource.check = cJSON_IsTrue(check);
    } else {
        // Default to false if not specified
        resource.check = false;
    }

    if (cJSON_IsNumber(address)) {
        resource.address = (uint32_t)address->valuedouble;
    }

    if (cJSON_IsNumber(size)) {
        resource.size = (uint32_t)size->valuedouble;
    }

    if (cJSON_IsNumber(crc32)) {
        resource.crc32 = (uint32_t)crc32->valuedouble;
    }

    return resource;
}

// Parse resources array
static ResourceArray parse_resources(const cJSON *resources_json)
{
    ResourceArray result = {0};
    if (!cJSON_IsArray(resources_json)) {
        return result;
    }

    size_t count = cJSON_GetArraySize(resources_json);
    if (count == 0) {
        return result;
    }

    ResourceConfig *items = psram_malloc(count * sizeof(ResourceConfig));
    if (!items) {
        return result;
    }

    size_t valid_count = 0;
    for (size_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(resources_json, i);
        if (item) {
            items[valid_count] = parse_resource_config(item);
            valid_count++;
        }
    }

    result.items = items;
    result.count = valid_count;
    return result;
}

// Parse network configuration
static NetworkConfig parse_network_config(const cJSON *network_json)
{
    NetworkConfig config = {0};
    if (!network_json) {
        return config;
    }

    // Parse DNS configuration
    const cJSON *dns_json = cJSON_GetObjectItemCaseSensitive(network_json, "dns");
    if (dns_json) {
        const cJSON *servers = cJSON_GetObjectItemCaseSensitive(dns_json, "servers");
        if (servers) {
            config.dns.servers = parse_string_array(servers);
        }

        const cJSON *timeout = cJSON_GetObjectItemCaseSensitive(dns_json, "timeout-ms");
        if (cJSON_IsNumber(timeout)) {
            config.dns.timeout_ms = timeout->valueint;
        }

        const cJSON *retry_interval = cJSON_GetObjectItemCaseSensitive(dns_json, "retry-interval-ms");
        if (cJSON_IsNumber(retry_interval)) {
            config.dns.retry_interval_ms = retry_interval->valueint;
        }

        const cJSON *retry_times = cJSON_GetObjectItemCaseSensitive(dns_json, "retry-times");
        if (cJSON_IsNumber(retry_times)) {
            config.dns.retry_times = retry_times->valueint;
        }
    }

    // Parse SNTP configuration
    const cJSON *sntp_json = cJSON_GetObjectItemCaseSensitive(network_json, "sntp");
    if (sntp_json) {
        const cJSON *servers = cJSON_GetObjectItemCaseSensitive(sntp_json, "servers");
        if (servers) {
            config.sntp.servers = parse_string_array(servers);
        }

        const cJSON *timezone = cJSON_GetObjectItemCaseSensitive(sntp_json, "timezone");
        if (cJSON_IsString(timezone)) {
            config.sntp.timezone = strdup_safe(timezone->valuestring);
        }

        const cJSON *timeout = cJSON_GetObjectItemCaseSensitive(sntp_json, "timeout-ms");
        if (cJSON_IsNumber(timeout)) {
            config.sntp.timeout_ms = timeout->valueint;
        }

        const cJSON *retry_interval = cJSON_GetObjectItemCaseSensitive(sntp_json, "retry-interval-ms");
        if (cJSON_IsNumber(retry_interval)) {
            config.sntp.retry_interval_ms = retry_interval->valueint;
        }

        const cJSON *retry_times = cJSON_GetObjectItemCaseSensitive(sntp_json, "retry-times");
        if (cJSON_IsNumber(retry_times)) {
            config.sntp.retry_times = retry_times->valueint;
        }
    }

    return config;
}

// Parse wake-up configuration
static WakeUpConfig parse_wake_up_config(const cJSON *wake_up_json)
{
    WakeUpConfig config = {0};
    if (!wake_up_json) {
        return config;
    }

    // Parse voice wake-up configuration
    const cJSON *voice_json = cJSON_GetObjectItemCaseSensitive(wake_up_json, "voice");
    if (voice_json) {
        const cJSON *enable = cJSON_GetObjectItemCaseSensitive(voice_json, "enable");
        if (cJSON_IsBool(enable)) {
            config.voice.enable = cJSON_IsTrue(enable);
        }

        const cJSON *keywords_filter = cJSON_GetObjectItemCaseSensitive(voice_json, "keywords_filter");
        if (cJSON_IsBool(keywords_filter)) {
            config.voice.keywords_filter = cJSON_IsTrue(keywords_filter);
        }

        const cJSON *keywords = cJSON_GetObjectItemCaseSensitive(voice_json, "keywords");
        if (keywords) {
            config.voice.keywords = parse_string_array(keywords);
        }
    }

    // Parse button wake-up configuration
    const cJSON *button_json = cJSON_GetObjectItemCaseSensitive(wake_up_json, "button");
    if (button_json) {
        const cJSON *enable = cJSON_GetObjectItemCaseSensitive(button_json, "enable");
        if (cJSON_IsBool(enable)) {
            config.button.enable = cJSON_IsTrue(enable);
        }

        const cJSON *mode = cJSON_GetObjectItemCaseSensitive(button_json, "mode");
        if (cJSON_IsString(mode)) {
            config.button.mode = strdup_safe(mode->valuestring);
        }
    }

    return config;
}

// Parse role configuration
static RoleConfig parse_role_config(const cJSON *role_json)
{
    RoleConfig config = {0};
    if (!role_json) {
        return config;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(role_json, "name");
    if (cJSON_IsString(name)) {
        config.name = strdup_safe(name->valuestring);
    }

    const cJSON *prompt = cJSON_GetObjectItemCaseSensitive(role_json, "prompt");
    if (cJSON_IsString(prompt)) {
        config.prompt = strdup_safe(prompt->valuestring);
    }

    const cJSON *hello_json = cJSON_GetObjectItemCaseSensitive(role_json, "hello");
    if (hello_json) {
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(hello_json, "text");
        if (cJSON_IsString(text)) {
            config.hello.text = strdup_safe(text->valuestring);
        }

        const cJSON *tone_id = cJSON_GetObjectItemCaseSensitive(hello_json, "tone-id");
        if (cJSON_IsNumber(tone_id)) {
            config.hello.tone_id = tone_id->valueint;
        }
    }

    return config;
}

// Parse cloud configuration
static void parse_cloud_config(CloudConfig *config, const cJSON *cloud_json)
{
    if (!config || !cloud_json) {
        return;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(cloud_json, "name");
    if (cJSON_IsString(name)) {
        config->name = strdup_safe(name->valuestring);
    }

    const cJSON *config_json = cJSON_GetObjectItemCaseSensitive(cloud_json, "config");
    if (config_json) {
        const cJSON *pid = cJSON_GetObjectItemCaseSensitive(config_json, "pid");
        if (cJSON_IsString(pid)) {
            config->pid = strdup_safe(pid->valuestring);
        }

        const cJSON *sid = cJSON_GetObjectItemCaseSensitive(config_json, "sid");
        if (cJSON_IsString(sid)) {
            config->sid = strdup_safe(sid->valuestring);
        }

        const cJSON *app_id = cJSON_GetObjectItemCaseSensitive(config_json, "app-id");
        if (cJSON_IsString(app_id)) {
            config->app_id = strdup_safe(app_id->valuestring);
        }

        const cJSON *app_key = cJSON_GetObjectItemCaseSensitive(config_json, "app-key");
        if (cJSON_IsString(app_key)) {
            config->app_key = strdup_safe(app_key->valuestring);
        }
    }

    // Parse auth configuration
    const cJSON *auth_json = cJSON_GetObjectItemCaseSensitive(cloud_json, "auth");
    if (auth_json) {
        const cJSON *url = cJSON_GetObjectItemCaseSensitive(auth_json, "url");
        if (cJSON_IsString(url)) {
            config->auth.url = strdup_safe(url->valuestring);
        }
    }

    // Parse WebSocket configuration
    const cJSON *ws_json = cJSON_GetObjectItemCaseSensitive(cloud_json, "websocket");
    if (ws_json) {
        const cJSON *host = cJSON_GetObjectItemCaseSensitive(ws_json, "host");
        if (cJSON_IsString(host)) {
            config->websocket.host = strdup_safe(host->valuestring);
        }

        const cJSON *port = cJSON_GetObjectItemCaseSensitive(ws_json, "port");
        if (cJSON_IsString(port)) {
            config->websocket.port = strdup_safe(port->valuestring);
        }

        const cJSON *path = cJSON_GetObjectItemCaseSensitive(ws_json, "path");
        if (cJSON_IsString(path)) {
            config->websocket.path = strdup_safe(path->valuestring);
        }

        const cJSON *scheme = cJSON_GetObjectItemCaseSensitive(ws_json, "scheme");
        if (cJSON_IsString(scheme)) {
            config->websocket.scheme = strdup_safe(scheme->valuestring);
        }
    }
}

// Main configuration parsing function
const Config *config_parse(const char *json_str)
{
    if (!json_str) {
        return NULL;
    }

    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            printf("JSON parse error before: %s\n", error_ptr);
        }
        return NULL;
    }

    Config *config = psram_calloc(1, sizeof(Config));
    if (!config) {
        cJSON_Delete(json);
        return NULL;
    }

    // Parse application info
    const cJSON *app = cJSON_GetObjectItemCaseSensitive(json, "application");
    if (!app) {
        config_free(config);
        cJSON_Delete(json);
        return NULL;
    }

    // Parse basic application info
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(app, "name");
    if (cJSON_IsString(name)) {
        config->name = strdup_safe(name->valuestring);
    }

    const cJSON *version = cJSON_GetObjectItemCaseSensitive(app, "version");
    if (cJSON_IsString(version)) {
        config->version = strdup_safe(version->valuestring);
    }

    // Parse abilities
    const cJSON *abilities = cJSON_GetObjectItemCaseSensitive(app, "abilities");
    if (abilities) {
        const cJSON *chat = cJSON_GetObjectItemCaseSensitive(abilities, "chat");
        if (chat) {
            const cJSON *cloud = cJSON_GetObjectItemCaseSensitive(chat, "cloud");
            if (cloud) {
                parse_cloud_config(&config->abilities.chat.cloud, cloud);
            }
        }
    }

    // Parse config section
    const cJSON *config_json = cJSON_GetObjectItemCaseSensitive(app, "config");
    if (config_json) {
        // Parse wake-up configuration
        const cJSON *wake_up_json = cJSON_GetObjectItemCaseSensitive(config_json, "wake-up");
        if (wake_up_json) {
            config->wake_up = parse_wake_up_config(wake_up_json);
        }

        // Parse role configuration
        const cJSON *role_json = cJSON_GetObjectItemCaseSensitive(config_json, "role");
        if (role_json) {
            config->role = parse_role_config(role_json);
        }

        // Parse network configuration
        const cJSON *network_json = cJSON_GetObjectItemCaseSensitive(config_json, "network");
        if (network_json) {
            config->network = parse_network_config(network_json);
        }

        // Parse resources
        const cJSON *resources_json = cJSON_GetObjectItemCaseSensitive(config_json, "resources");
        if (resources_json) {
            config->resources = parse_resources(resources_json);
        }
    }

    cJSON_Delete(json);
    return config;
}

// Free configuration memory
void config_free(Config *config)
{
    if (!config) {
        return;
    }

    // Free basic application info
    if (config->name) {
        psram_free(config->name);
    }
    if (config->version) {
        psram_free(config->version);
    }

    // Free cloud configuration
    if (config->abilities.chat.cloud.name) {
        psram_free(config->abilities.chat.cloud.name);
    }
    if (config->abilities.chat.cloud.pid) {
        psram_free(config->abilities.chat.cloud.pid);
    }
    if (config->abilities.chat.cloud.sid) {
        psram_free(config->abilities.chat.cloud.sid);
    }
    if (config->abilities.chat.cloud.app_id) {
        psram_free(config->abilities.chat.cloud.app_id);
    }
    if (config->abilities.chat.cloud.app_key) {
        psram_free(config->abilities.chat.cloud.app_key);
    }
    if (config->abilities.chat.cloud.auth.url) {
        psram_free(config->abilities.chat.cloud.auth.url);
    }
    if (config->abilities.chat.cloud.websocket.host) {
        psram_free(config->abilities.chat.cloud.websocket.host);
    }
    if (config->abilities.chat.cloud.websocket.port) {
        psram_free(config->abilities.chat.cloud.websocket.port);
    }
    if (config->abilities.chat.cloud.websocket.path) {
        psram_free(config->abilities.chat.cloud.websocket.path);
    }
    if (config->abilities.chat.cloud.websocket.scheme) {
        psram_free(config->abilities.chat.cloud.websocket.scheme);
    }

    // Free wake-up configuration
    free_string_array(&config->wake_up.voice.keywords);
    if (config->wake_up.button.mode) {
        psram_free(config->wake_up.button.mode);
    }

    // Free role configuration
    if (config->role.name) {
        psram_free(config->role.name);
    }
    if (config->role.prompt) {
        psram_free(config->role.prompt);
    }
    if (config->role.hello.text) {
        psram_free(config->role.hello.text);
    }

    // Free network configuration
    free_string_array(&config->network.dns.servers);
    free_string_array(&config->network.sntp.servers);
    if (config->network.sntp.timezone) {
        psram_free(config->network.sntp.timezone);
    }

    // Free resources
    if (config->resources.items) {
        for (size_t i = 0; i < config->resources.count; i++) {
            if (config->resources.items[i].name) {
                psram_free(config->resources.items[i].name);
            }
        }
        psram_free(config->resources.items);
    }

    psram_free(config);
}

// Print configuration for debugging
void config_print(const Config *config)
{
    if (!config) {
        printf("Configuration is NULL\n");
        return;
    }

    printf("=== Application Configuration ===\n");
    printf("Name: %s\n", config->name ? config->name : "");
    printf("Version: %s\n\n", config->version ? config->version : "");

    printf("=== Cloud Configuration ===\n");
    printf("Name: %s\n", config->abilities.chat.cloud.name ? config->abilities.chat.cloud.name : "");
    printf("PID: %s\n", config->abilities.chat.cloud.pid ? config->abilities.chat.cloud.pid : "");
    printf("SID: %s\n", config->abilities.chat.cloud.sid ? config->abilities.chat.cloud.sid : "");
    printf("App ID: %s\n", config->abilities.chat.cloud.app_id ? config->abilities.chat.cloud.app_id : "");
    printf("App Key: %s\n", config->abilities.chat.cloud.app_key ? "[HIDDEN]" : "");
    printf("Auth URL: %s\n", config->abilities.chat.cloud.auth.url ? config->abilities.chat.cloud.auth.url : "");
    printf("WebSocket: %s://%s:%s%s\n\n",
           config->abilities.chat.cloud.websocket.scheme ? config->abilities.chat.cloud.websocket.scheme : "",
           config->abilities.chat.cloud.websocket.host ? config->abilities.chat.cloud.websocket.host : "",
           config->abilities.chat.cloud.websocket.port ? config->abilities.chat.cloud.websocket.port : "",
           config->abilities.chat.cloud.websocket.path ? config->abilities.chat.cloud.websocket.path : "");

    printf("=== Wake-up Configuration ===\n");
    printf("Voice Wake-up: %s\n", config->wake_up.voice.enable ? "Enabled" : "Disabled");
    printf("Wake-up Keywords:\n");
    for (size_t i = 0; i < config->wake_up.voice.keywords.count; i++) {
        printf("  - %s\n", config->wake_up.voice.keywords.items[i]);
    }
    printf("Button Wake-up: %s\n", config->wake_up.button.enable ? "Enabled" : "Disabled");
    printf("Button Mode: %s\n\n", config->wake_up.button.mode ? config->wake_up.button.mode : "");

    printf("=== Role Configuration ===\n");
    printf("Name: %s\n", config->role.name ? config->role.name : "");
    printf("Prompt: %s\n", config->role.prompt ? config->role.prompt : "");
    printf("Hello Text: %s\n", config->role.hello.text ? config->role.hello.text : "");
    printf("Hello Tone ID: %d\n\n", config->role.hello.tone_id);

    printf("=== Network Configuration ===\n");
    printf("DNS Servers:\n");
    for (size_t i = 0; i < config->network.dns.servers.count; i++) {
        printf("  - %s\n", config->network.dns.servers.items[i]);
    }
    printf("DNS Timeout: %ums, Retry: %dx every %ums\n", config->network.dns.timeout_ms,
           config->network.dns.retry_times, config->network.dns.retry_interval_ms);

    printf("\nSNTP Servers:\n");
    for (size_t i = 0; i < config->network.sntp.servers.count; i++) {
        printf("  - %s\n", config->network.sntp.servers.items[i]);
    }
    printf("SNTP Timezone: %s\n", config->network.sntp.timezone ? config->network.sntp.timezone : "");
    printf("SNTP Timeout: %ums, Retry: %dx every %ums\n", config->network.sntp.timeout_ms,
           config->network.sntp.retry_times, config->network.sntp.retry_interval_ms);

    printf("\n=== Resources ===\n");
    for (size_t i = 0; i < config->resources.count; i++) {
        const ResourceConfig *res = &config->resources.items[i];
        printf("Resource %zu:\n", i + 1);
        printf("  Name: %s\n", res->name ? res->name : "");
        printf("  Check: %s\n", res->check ? "Yes" : "No");
        printf("  Address: 0x%08X\n", res->address);
        printf("  Size: %u bytes\n", res->size);
        printf("  CRC32: 0x%08X\n", res->crc32);
        printf("\n");
    }
}

// Global configuration instance
static const Config *g_config = NULL;

// Get the global configuration instance
const Config *config_get(void)
{
    return g_config;
}

// Initialize the global configuration
const Config *config_init(const char *config_json_str)
{
    if (g_config) {
        return g_config;
    }

    g_config = config_parse(config_json_str);
    return g_config;
}

const ResourceConfig *config_get_resource_by_name(const char *name)
{
    for (size_t i = 0; i < g_config->resources.count; i++) {
        if (strcmp(g_config->resources.items[i].name, name) == 0) {
            return &g_config->resources.items[i];
        }
    }
    return NULL;
}
