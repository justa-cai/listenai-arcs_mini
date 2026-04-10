#include <stdio.h>
#include <stdlib.h>
#include "benchmark_log.h"
#include "benchmark_utils.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// 字体数据
extern const unsigned char data_As_I_Lay_Dying_ttf[];
extern const unsigned long data_As_I_Lay_Dying_ttf_len;

void run_single_char_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t configs[] = {
        {12, 300, FT_LOAD_DEFAULT},
        {16, 300, FT_LOAD_DEFAULT},
        {24, 300, FT_LOAD_DEFAULT},
        {32, 300, FT_LOAD_DEFAULT}
    };
    
    char test_chars[] = {'A', 'B', 'C', 'D', '1', '2', '3', '4'};
    
    benchmark_collector_add(collector, "=== Single Character Loading Benchmarks ===", &(benchmark_result_t){0});
    
    for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++) {
        for (size_t j = 0; j < sizeof(test_chars); j++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Single char load (size %d, char '%c')", 
                    configs[i].char_size, test_chars[j]);
            
            benchmark_result_t result = benchmark_single_char_load(face, &configs[i], test_chars[j]);
            benchmark_collector_add(collector, buf, &result);
        }
    }
}

void run_batch_char_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t config = {16, 300, FT_LOAD_DEFAULT};
    const char* test_strings[] = {
        "Hello",
        "Hello, World!",
        "The quick brown fox jumps over the lazy dog"
    };
    
    benchmark_collector_add(collector, "=== Batch Character Loading Benchmarks ===", &(benchmark_result_t){0});
    
    for (size_t i = 0; i < sizeof(test_strings)/sizeof(test_strings[0]); i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Batch load (%zu chars)", strlen(test_strings[i]));
        
        benchmark_result_t result = benchmark_batch_chars_load(face, &config, 
                                                             test_strings[i], strlen(test_strings[i]));
        benchmark_collector_add(collector, buf, &result);
    }
}

void run_rendering_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t configs[] = {
        {16, 300, FT_LOAD_DEFAULT},
        {16, 300, FT_LOAD_NO_HINTING},
        {16, 300, FT_LOAD_FORCE_AUTOHINT},
        {16, 300, FT_LOAD_TARGET_LIGHT}
    };
    
    const char* config_names[] = {
        "Default",
        "No Hinting",
        "Force Autohint",
        "Light Target"
    };
    
    benchmark_collector_add(collector, "=== Rendering Performance Benchmarks ===", &(benchmark_result_t){0});
    
    for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Render with %s", config_names[i]);
        
        benchmark_result_t result = benchmark_glyph_render(face, &configs[i], 'A');
        benchmark_collector_add(collector, buf, &result);
    }
}

void run_cache_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t config = {16, 300, FT_LOAD_DEFAULT};
    const char* test_strings[] = {
        "Hello",
        "Hello, World!",
        "The quick brown fox jumps over the lazy dog"
    };
    int iterations[] = {1, 10, 100};
    
    benchmark_collector_add(collector, "=== Glyph Cache Performance Benchmarks ===", &(benchmark_result_t){0});
    
    for (size_t i = 0; i < sizeof(test_strings)/sizeof(test_strings[0]); i++) {
        for (size_t j = 0; j < sizeof(iterations)/sizeof(iterations[0]); j++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Cache test (%zu chars, %d iterations)", 
                    strlen(test_strings[i]), iterations[j]);
            
            benchmark_result_t result = benchmark_glyph_cache(face, &config, 
                                                            test_strings[i], strlen(test_strings[i]),
                                                            iterations[j]);
            benchmark_collector_add(collector, buf, &result);
        }
    }
}

void run_transform_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t config = {32, 300, FT_LOAD_DEFAULT};
    const char test_char = 'A';
    
    benchmark_collector_add(collector, "=== Glyph Transform Benchmarks ===", &(benchmark_result_t){0});
    
    // 测试不同的变换矩阵
    FT_Matrix matrices[] = {
        // 旋转45度
        {0x0000B505,  0x000B505,  -0x000B505,  0x0000B505},
        // 水平拉伸2倍
        {0x00020000,  0x0000000,   0x0000000,  0x00010000},
        // 垂直拉伸2倍
        {0x00010000,  0x0000000,   0x0000000,  0x00020000},
        // 倾斜
        {0x00010000,  0x0005000,   0x0000000,  0x00010000}
    };
    
    const char* transform_names[] = {
        "Rotate 45 degrees",
        "Scale 2x horizontal",
        "Scale 2x vertical",
        "Shear"
    };
    
    for (size_t i = 0; i < sizeof(matrices)/sizeof(matrices[0]); i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Transform (%s)", transform_names[i]);
        
        benchmark_result_t result = benchmark_glyph_transform(face, &config, test_char, &matrices[i]);
        benchmark_collector_add(collector, buf, &result);
    }
}

void run_render_mode_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t config = {32, 300, FT_LOAD_DEFAULT};
    const char test_char = 'A';
    
    benchmark_collector_add(collector, "=== Render Mode Benchmarks ===", &(benchmark_result_t){0});
    
    FT_Render_Mode modes[] = {
        FT_RENDER_MODE_NORMAL,
        FT_RENDER_MODE_LIGHT,
        FT_RENDER_MODE_MONO,
        FT_RENDER_MODE_LCD,
        FT_RENDER_MODE_LCD_V
    };
    
    const char* mode_names[] = {
        "Normal (anti-aliased)",
        "Light",
        "Monochrome",
        "LCD (horizontal RGB)",
        "LCD (vertical RGB)"
    };
    
    for (size_t i = 0; i < sizeof(modes)/sizeof(modes[0]); i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Render mode (%s)", mode_names[i]);
        
        benchmark_result_t result = benchmark_render_mode(face, &config, test_char, modes[i]);
        benchmark_collector_add(collector, buf, &result);
    }
}

void run_outline_benchmarks(FT_Face face, benchmark_collector_t* collector) {
    render_config_t configs[] = {
        {12, 300, FT_LOAD_DEFAULT},
        {24, 300, FT_LOAD_DEFAULT},
        {48, 300, FT_LOAD_DEFAULT},
        {72, 300, FT_LOAD_DEFAULT}
    };
    const char test_chars[] = {'A', 'B', '@', '8'};
    
    benchmark_collector_add(collector, "=== Outline Extraction Benchmarks ===", &(benchmark_result_t){0});
    
    for (size_t i = 0; i < sizeof(configs)/sizeof(configs[0]); i++) {
        for (size_t j = 0; j < sizeof(test_chars)/sizeof(test_chars[0]); j++) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Outline (size %d, char '%c')", 
                    configs[i].char_size, test_chars[j]);
            
            benchmark_result_t result = benchmark_glyph_outline(face, &configs[i], test_chars[j]);
            benchmark_collector_add(collector, buf, &result);
        }
    }
}

int main(int argc, char **argv) {
    FT_Library library;
    FT_Face face;
    FT_Error error;
    benchmark_collector_t collector;

    BENCHMARK_LOG_INFO("\nFreeType Benchmark Started\n");

    benchmark_collector_init(&collector);

    error = FT_Init_FreeType(&library);
    if (error) {
        BENCHMARK_LOG_ERROR("Error: Could not init FreeType library\n");
        benchmark_collector_free(&collector);
        return -1;
    }

    error = FT_New_Memory_Face(library,
                              data_As_I_Lay_Dying_ttf,
                              data_As_I_Lay_Dying_ttf_len,
                              0,
                              &face);
    if (error) {
        BENCHMARK_LOG_ERROR("Error: Could not open font\n");
        FT_Done_FreeType(library);
        benchmark_collector_free(&collector);
        return -1;
    }

    // 运行所有测试场景
    run_single_char_benchmarks(face, &collector);
    run_batch_char_benchmarks(face, &collector);
    run_rendering_benchmarks(face, &collector);
    run_cache_benchmarks(face, &collector);
    run_transform_benchmarks(face, &collector);
    run_render_mode_benchmarks(face, &collector);
    run_outline_benchmarks(face, &collector);

    // 打印所有测试结果
    benchmark_collector_print_all(&collector);

    // 清理
    benchmark_collector_free(&collector);
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    BENCHMARK_LOG_INFO("\nFreeType Benchmark Completed\n");
    return 0;
}
