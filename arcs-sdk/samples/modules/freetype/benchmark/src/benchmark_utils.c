#include "benchmark_utils.h"
#include "benchmark_log.h"
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define INITIAL_COLLECTOR_CAPACITY 50

static void print_table_header(void) {
    // 打印表头上边框
    printf(MAKE_HORIZONTAL_LINE(TABLE_TOP_LEFT, TABLE_T_DOWN, TABLE_TOP_RIGHT));
    
    // 打印表头
    printf(TABLE_HEADER_FORMAT,
           TABLE_VERTICAL,
           TEST_NAME_WIDTH, "Test Name",
           "",
           "",
           DURATION_WIDTH, "Duration (ms)",
           "",
           "",
           MEMORY_WIDTH, "Memory (bytes)",
           "");
    
    // 打印表头下边框
    printf(MAKE_HORIZONTAL_LINE(TABLE_T_RIGHT, TABLE_CROSS, TABLE_T_LEFT));
}

static void print_table_footer(void) {
    // 打印表格底部边框
    printf(MAKE_HORIZONTAL_LINE(TABLE_BOTTOM_LEFT, TABLE_T_UP, TABLE_BOTTOM_RIGHT));
}

void benchmark_collector_init(benchmark_collector_t* collector) {
    collector->capacity = INITIAL_COLLECTOR_CAPACITY;
    collector->count = 0;
    collector->items = (benchmark_result_item_t*)malloc(
        collector->capacity * sizeof(benchmark_result_item_t));
}

void benchmark_collector_add(benchmark_collector_t* collector, const char* test_name, const benchmark_result_t* result) {
    if (collector->count >= collector->capacity) {
        collector->capacity *= 2;
        collector->items = (benchmark_result_item_t*)realloc(
            collector->items, collector->capacity * sizeof(benchmark_result_item_t));
    }
    
    strncpy(collector->items[collector->count].test_name, test_name, sizeof(collector->items[0].test_name) - 1);
    collector->items[collector->count].test_name[sizeof(collector->items[0].test_name) - 1] = '\0';
    collector->items[collector->count].result = *result;
    collector->count++;
}

void benchmark_collector_print_all(benchmark_collector_t* collector) {
    BENCHMARK_LOG_INFO(BENCHMARK_LOG_CATEGORY_START, "Benchmark Results Summary");
    
    const char* current_category = NULL;
    int first_category = 1;
    int table_started = 0;
    
    for (size_t i = 0; i < collector->count; i++) {
        // 检查是否是新的测试类别
        const char* category_start = strstr(collector->items[i].test_name, "===");
        const char* testing_size = strstr(collector->items[i].test_name, "Testing size:");
        
        if (category_start) {
            // 如果表格已经开始，先打印表格底部
            if (table_started) {
                print_table_footer();
                table_started = 0;
            }
            
            current_category = collector->items[i].test_name;
            BENCHMARK_LOG_INFO("\n%s\n", current_category);
            
            // 打印新表格的表头
            print_table_header();
            table_started = 1;
            
            first_category = 0;
            continue;
        }
        
        if (testing_size) {
            // 如果表格已经开始，先结束当前表格
            if (table_started) {
                print_table_footer();
            }
            
            // 打印大小信息作为子标题
            BENCHMARK_LOG_INFO("\n%s\n", testing_size);
            
            // 开始新表格
            print_table_header();
            table_started = 1;
            continue;
        }
        
        // 打印数据行
        printf(TABLE_ROW_FORMAT,
               TABLE_VERTICAL,
               TEST_NAME_WIDTH, collector->items[i].test_name,
               "",
               "",
               DURATION_WIDTH, collector->items[i].result.duration_ms,
               "",
               "",
               MEMORY_WIDTH, collector->items[i].result.memory_used,
               "");
    }
    
    // 打印最后一个表格的底部
    if (table_started) {
        print_table_footer();
    }
    
    BENCHMARK_LOG_INFO(BENCHMARK_LOG_CATEGORY_END, "Benchmark Results Summary");
}

void benchmark_collector_free(benchmark_collector_t* collector) {
    free(collector->items);
    collector->items = NULL;
    collector->count = 0;
    collector->capacity = 0;
}

void benchmark_start(benchmark_result_t* result) {
    result->start_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    result->memory_used = get_current_memory_usage();
}

void benchmark_end(benchmark_result_t* result) {
    result->end_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    result->duration_ms = result->end_time_ms - result->start_time_ms;
    result->memory_used = get_current_memory_usage() - result->memory_used;
}

benchmark_result_t benchmark_single_char_load(FT_Face face, const render_config_t* config, char c) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    FT_Error error = FT_Load_Char(face, c, config->load_flags);
    benchmark_end(&result);
    
    if (error) {
        BENCHMARK_LOG_ERROR("Error loading char '%c': %d\n", c, error);
    }
    
    return result;
}

benchmark_result_t benchmark_batch_chars_load(FT_Face face, const render_config_t* config,
                                            const char* chars, int char_count) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    for (int i = 0; i < char_count; i++) {
        FT_Error error = FT_Load_Char(face, chars[i], config->load_flags);
        if (error) {
            BENCHMARK_LOG_ERROR("Error loading char '%c': %d\n", chars[i], error);
        }
    }
    benchmark_end(&result);
    
    return result;
}

benchmark_result_t benchmark_glyph_render(FT_Face face, const render_config_t* config, char c) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    FT_Error error = FT_Load_Char(face, c, FT_LOAD_RENDER | config->load_flags);
    benchmark_end(&result);
    
    if (error) {
        BENCHMARK_LOG_ERROR("Error rendering char '%c': %d\n", c, error);
    }
    
    return result;
}

benchmark_result_t benchmark_multiple_glyphs_render(FT_Face face, const render_config_t* config,
                                                  const char* text, int text_len) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    for (int i = 0; i < text_len; i++) {
        FT_Error error = FT_Load_Char(face, text[i], FT_LOAD_RENDER | config->load_flags);
        if (error) {
            BENCHMARK_LOG_ERROR("Error rendering char '%c': %d\n", text[i], error);
        }
    }
    benchmark_end(&result);
    
    return result;
}

size_t get_current_memory_usage(void) {
    // Note: This is a simplified implementation
    // In a real system, you would want to use proper memory tracking
    return 0;
}

void print_benchmark_result(const char* test_name, const benchmark_result_t* result) {
    BENCHMARK_LOG_INFO("\nBenchmark: %s\n", test_name);
    BENCHMARK_LOG_INFO("Duration: %u ms\n", result->duration_ms);
    BENCHMARK_LOG_INFO("Memory used: %zu bytes\n", result->memory_used);
}

benchmark_result_t benchmark_glyph_cache(FT_Face face, const render_config_t* config,
                                       const char* text, int text_len, int iterations) {
    benchmark_result_t result = {0};
    FT_UInt glyph_indices[256];  // 缓存字形索引
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    
    // 首先获取所有字形索引
    for (int i = 0; i < text_len; i++) {
        glyph_indices[i] = FT_Get_Char_Index(face, text[i]);
    }
    
    // 多次重复加载字形以测试缓存性能
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < text_len; i++) {
            FT_Load_Glyph(face, glyph_indices[i], config->load_flags);
        }
    }
    
    benchmark_end(&result);
    return result;
}

benchmark_result_t benchmark_glyph_transform(FT_Face face, const render_config_t* config,
                                           char c, FT_Matrix* matrix) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    
    // 设置变换矩阵
    FT_Set_Transform(face, matrix, NULL);
    
    // 加载并渲染字形
    FT_Error error = FT_Load_Char(face, c, FT_LOAD_RENDER | config->load_flags);
    if (error) {
        BENCHMARK_LOG_ERROR("Error transforming char '%c': %d\n", c, error);
    }
    
    // 重置变换
    FT_Set_Transform(face, NULL, NULL);
    
    benchmark_end(&result);
    return result;
}

benchmark_result_t benchmark_glyph_outline(FT_Face face, const render_config_t* config,
                                         char c) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    
    // 加载字形但不渲染
    FT_Error error = FT_Load_Char(face, c, FT_LOAD_NO_BITMAP | config->load_flags);
    if (error) {
        BENCHMARK_LOG_ERROR("Error loading char '%c' for outline: %d\n", c, error);
    } else {
        // 获取字形轮廓
        FT_Outline* outline = &face->glyph->outline;
        // 这里可以添加轮廓处理代码
    }
    
    benchmark_end(&result);
    return result;
}

benchmark_result_t benchmark_render_mode(FT_Face face, const render_config_t* config,
                                       char c, FT_Render_Mode render_mode) {
    benchmark_result_t result = {0};
    
    // 设置字符大小
    FT_Set_Char_Size(face, 0, config->char_size * 64, config->dpi, config->dpi);
    
    benchmark_start(&result);
    
    // 加载字形
    FT_Error error = FT_Load_Char(face, c, FT_LOAD_DEFAULT | config->load_flags);
    if (error) {
        BENCHMARK_LOG_ERROR("Error loading char '%c' for rendering: %d\n", c, error);
    } else {
        // 使用指定的渲染模式渲染字形
        error = FT_Render_Glyph(face->glyph, render_mode);
        if (error) {
            BENCHMARK_LOG_ERROR("Error rendering char '%c' with mode %d: %d\n", c, render_mode, error);
        }
    }
    
    benchmark_end(&result);
    return result;
}
