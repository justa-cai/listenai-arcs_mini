#ifndef BENCHMARK_UTILS_H
#define BENCHMARK_UTILS_H

#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "systick.h"

// Benchmark结果结构
typedef struct {
    uint32_t start_time_ms;
    uint32_t end_time_ms;
    uint32_t duration_ms;
    size_t memory_used;
} benchmark_result_t;

// 测试结果项结构
typedef struct {
    char test_name[128];
    benchmark_result_t result;
} benchmark_result_item_t;

// 结果收集器结构
typedef struct {
    benchmark_result_item_t* items;
    size_t count;
    size_t capacity;
} benchmark_collector_t;

// 字符渲染配置
typedef struct {
    int char_size;
    int dpi;
    FT_Int32 load_flags;
} render_config_t;

// 结果收集器函数
void benchmark_collector_init(benchmark_collector_t* collector);
void benchmark_collector_add(benchmark_collector_t* collector, const char* test_name, const benchmark_result_t* result);
void benchmark_collector_print_all(benchmark_collector_t* collector);
void benchmark_collector_free(benchmark_collector_t* collector);

// 基准测试函数
void benchmark_start(benchmark_result_t* result);
void benchmark_end(benchmark_result_t* result);

// 单字符加载性能测试
benchmark_result_t benchmark_single_char_load(FT_Face face, const render_config_t* config, char c);

// 批量字符加载性能测试
benchmark_result_t benchmark_batch_chars_load(FT_Face face, const render_config_t* config, 
                                            const char* chars, int char_count);

// 渲染性能测试
benchmark_result_t benchmark_glyph_render(FT_Face face, const render_config_t* config, char c);
benchmark_result_t benchmark_multiple_glyphs_render(FT_Face face, const render_config_t* config,
                                                  const char* text, int text_len);

// 测试字形缓存性能
benchmark_result_t benchmark_glyph_cache(FT_Face face, const render_config_t* config,
                                       const char* text, int text_len, int iterations);

// 测试字形变换性能
benchmark_result_t benchmark_glyph_transform(FT_Face face, const render_config_t* config,
                                           char c, FT_Matrix* matrix);

// 测试字形轮廓提取性能
benchmark_result_t benchmark_glyph_outline(FT_Face face, const render_config_t* config,
                                         char c);

// 测试不同渲染模式性能
benchmark_result_t benchmark_render_mode(FT_Face face, const render_config_t* config,
                                       char c, FT_Render_Mode render_mode);

// 内存使用测试
size_t get_current_memory_usage(void);

#endif // BENCHMARK_UTILS_H
