#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "lisa_log.h"
#include "mcp.h"
#include "service_camera.h"
#include "sysheap.h"
#include "voice_msg.h"

static cJSON *take_photo_list(const char *name)
{
    cJSON *tool = mcp_tool_list_info_create_default(name, "拍照");
    if (!tool) {
        return NULL;
    }

    return tool;
}

static cJSON *take_photo_call(const char *id, const char *name, cJSON *args)
{
    LOGI("Taking photo...");

    voice_msg_pub(VOICE_MSG_CLOUD_MCP_IMAGE_RECOGNITION, (void *)id, strlen(id) + 1);

    return NULL;
}

MCP_TOOL_DEFINE(ls.built_in.take_photo, take_photo_list, take_photo_call);
