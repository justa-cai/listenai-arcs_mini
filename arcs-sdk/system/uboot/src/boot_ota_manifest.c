#include "boot_ota_manifest.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "syslog.h"

#define OTA_COPY_TO_NOR "nor"
#define OTA_MD5_DIGEST_LEN 16
#define OTA_MD5_HEX_LEN 32

static int boot_ota_manifest_hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}

static int boot_ota_manifest_parse_u32(const cJSON *item, uint32_t *value)
{
    const char *text;
    unsigned long parsed;
    char *end = NULL;

    if (item == NULL || value == NULL) {
        return -1;
    }

    if (cJSON_IsString(item) && item->valuestring != NULL) {
        text = item->valuestring;
        if (text[0] == '\0' || text[0] == '-') {
            return -1;
        }

        parsed = strtoul(text, &end, 0);
        if (end == NULL || *end != '\0' || parsed > UINT32_MAX) {
            return -1;
        }

        *value = (uint32_t)parsed;
        return 0;
    }

    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0 ||
        item->valuedouble > (double)UINT32_MAX) {
        return -1;
    }

    parsed = (unsigned long)item->valuedouble;
    if ((double)parsed != item->valuedouble) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int boot_ota_manifest_parse_md5(const cJSON *item, uint8_t digest[OTA_MD5_DIGEST_LEN])
{
    const char *text;
    size_t i;

    if (!cJSON_IsString(item) || item->valuestring == NULL || digest == NULL) {
        return -1;
    }

    text = item->valuestring;
    if (strlen(text) != OTA_MD5_HEX_LEN) {
        return -1;
    }

    for (i = 0; i < OTA_MD5_DIGEST_LEN; ++i) {
        int high = boot_ota_manifest_hex_nibble(text[i * 2U]);
        int low = boot_ota_manifest_hex_nibble(text[i * 2U + 1U]);

        if (high < 0 || low < 0) {
            return -1;
        }

        digest[i] = (uint8_t)((high << 4) | low);
    }

    return 0;
}

static int boot_ota_manifest_add_file(ota_param_t *param, const cJSON *image_item)
{
    const cJSON *addr_item;
    const cJSON *copy_to_item;
    const cJSON *file_item;
    const cJSON *md5_item;
    const cJSON *size_item;
    const cJSON *type_item;
    ota_file_t *file;
    uint32_t addr;
    uint32_t size;
    uint32_t i;

    if (param == NULL || image_item == NULL) {
        return -1;
    }

    if (param->file_count >= MAX_OTA_FILES) {
        printk("ota: too many manifest images\n");
        return -1;
    }

    file_item = cJSON_GetObjectItemCaseSensitive(image_item, "file");
    md5_item = cJSON_GetObjectItemCaseSensitive(image_item, "md5");
    copy_to_item = cJSON_GetObjectItemCaseSensitive(image_item, "copy_to");
    if (!cJSON_IsString(file_item) || file_item->valuestring == NULL || copy_to_item == NULL) {
        return -1;
    }

    type_item = cJSON_GetObjectItemCaseSensitive(copy_to_item, "type");
    addr_item = cJSON_GetObjectItemCaseSensitive(copy_to_item, "addr");
    size_item = cJSON_GetObjectItemCaseSensitive(copy_to_item, "size");
    if (!cJSON_IsString(type_item) || type_item->valuestring == NULL ||
        strcmp(type_item->valuestring, OTA_COPY_TO_NOR) != 0) {
        printk("ota: unsupported copy_to.type\n");
        return -1;
    }
    if (boot_ota_manifest_parse_u32(addr_item, &addr) != 0 ||
        boot_ota_manifest_parse_u32(size_item, &size) != 0) {
        return -1;
    }
    if (addr < CONFIG_MEM_FLASH_BASE || size == 0U || addr > UINT32_MAX - size) {
        return -1;
    }

    for (i = 0; i < param->file_count; i++) {
        if (strcmp(file_item->valuestring, param->files[i].path_match) == 0) {
            printk("ota: duplicate manifest image %s\n", file_item->valuestring);
            return -1;
        }
    }

    file = &param->files[param->file_count];
    memset(file, 0, sizeof(*file));
    strncpy(file->path_match, file_item->valuestring, sizeof(file->path_match) - 1U);
    file->path_match[sizeof(file->path_match) - 1U] = '\0';
    if (boot_ota_manifest_parse_md5(md5_item, file->expected_md5) != 0) {
        printk("ota: invalid md5 for %s\n", file_item->valuestring);
        return -1;
    }
    file->part_base = addr;
    file->part_size = size;
    param->file_count++;
    return 0;
}

int boot_ota_manifest_parse_json(const char *json_text, ota_param_t *param)
{
    cJSON *root;
    const cJSON *image_item;
    const cJSON *images;

    if (json_text == NULL || param == NULL) {
        return -1;
    }

    root = cJSON_Parse(json_text);
    if (root == NULL) {
        return -1;
    }

    param->file_count = 0;
    memset(param->files, 0, sizeof(param->files));

    images = cJSON_GetObjectItemCaseSensitive(root, "image");
    if (!cJSON_IsArray(images)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON_ArrayForEach(image_item, images) {
        if (boot_ota_manifest_add_file(param, image_item) != 0) {
            cJSON_Delete(root);
            return -1;
        }
    }

    cJSON_Delete(root);
    return param->file_count > 0U ? 0 : -1;
}
