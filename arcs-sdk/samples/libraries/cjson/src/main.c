#include "stdio.h"
#include "cJSON.h"
#include "stdlib.h"

int main(int argc, char **argv)
{
    printf("this is a cjson samples\n");
    // 创建一个 JSON 对象
    cJSON *root = cJSON_CreateObject();
    cJSON *info = cJSON_CreateObject();

    // 添加数据到 JSON 对象
    cJSON_AddStringToObject(root, "name", "John Doe");
    cJSON_AddNumberToObject(root, "age", 30);
    cJSON_AddBoolToObject(root, "is_student", 0);
    cJSON_AddItemToObject(root, "info", info);
    cJSON_AddStringToObject(info, "address", "123 Main St");
    cJSON_AddNumberToObject(info, "zip_code", 12345);

    // 将 JSON 对象转换为字符串
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        fprintf(stderr, "Failed to print JSON.\n");
        cJSON_Delete(root);
        return 1;
    }

    // 输出 JSON 字符串
    printf("Generated JSON:\n%s\n", json_string);

    // 解析 JSON 字符串
    cJSON *parsed_root = cJSON_Parse(json_string);
    if (parsed_root == NULL) {
        fprintf(stderr, "Failed to parse JSON.\n");
        free(json_string);
        cJSON_Delete(root);
        return 1;
    }

    // 提取数据
    cJSON *name = cJSON_GetObjectItem(parsed_root, "name");
    cJSON *age = cJSON_GetObjectItem(parsed_root, "age");
    cJSON *is_student = cJSON_GetObjectItem(parsed_root, "is_student");
    cJSON *parsed_info = cJSON_GetObjectItem(parsed_root, "info");
    cJSON *address = cJSON_GetObjectItem(parsed_info, "address");
    cJSON *zip_code = cJSON_GetObjectItem(parsed_info, "zip_code");

    // 打印提取的数据
    printf("Parsed JSON:\n");
    printf("Name: %s\n", name->valuestring);
    printf("Age: %d\n", age->valueint);
    printf("Is Student: %s\n", is_student->valueint ? "true" : "false");
    printf("Address: %s\n", address->valuestring);
    printf("Zip Code: %d\n", zip_code->valueint);

    // 释放内存
    free(json_string);
    cJSON_Delete(root);
    cJSON_Delete(parsed_root);
}
