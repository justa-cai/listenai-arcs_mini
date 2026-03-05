#include <stdio.h>
#include <string.h>
#include <quirc.h>

#define LOG_TAG "quirc_sample"

#include "lisa_log.h"
#include "lisa_time.h"

/* Uncomment the line below to use pre-generated QR code test data */
/* To generate test data, run: tools/generate_qr.sh */
#define USE_QR_TEST_DATA

#ifdef USE_QR_TEST_DATA
#include "qr_test_data.h"
#endif

/* 简单的 QR 码测试数据 (模拟灰度图像数据)
 * 在实际应用中，这应该从相机或图像文件获取
 */
static void test_quirc_basic(void)
{
    struct quirc *qr;
    int width = 100;
    int height = 100;

    LISA_LOGI(LOG_TAG, "=== quirc QR-code recognition library test ===");
    LISA_LOGI(LOG_TAG, "Library version: %s", quirc_version());

    /* 创建 quirc 对象 */
    qr = quirc_new();
    if (!qr) {
        LISA_LOGE(LOG_TAG, "Error: Failed to allocate quirc object");
        return;
    }
    LISA_LOGI(LOG_TAG, "✓ Successfully created quirc object");

    /* 设置图像尺寸 */
    if (quirc_resize(qr, width, height) < 0) {
        LISA_LOGE(LOG_TAG, "Error: Failed to resize quirc object");
        quirc_destroy(qr);
        return;
    }
    LISA_LOGI(LOG_TAG, "✓ Successfully resized to %dx%d", width, height);

    /* 获取图像缓冲区 */
    int w, h;
    uint8_t *image = quirc_begin(qr, &w, &h);
    LISA_LOGI(LOG_TAG, "✓ Got image buffer: %dx%d", w, h);

    /* 填充测试图像数据 (全0，不包含实际的QR码)
     * 在实际应用中，这里应该填充真实的图像数据
     */
    memset(image, 0, w * h);
    LISA_LOGI(LOG_TAG, "✓ Filled image buffer with test data");

    /* 结束图像输入，开始识别 */
    quirc_end(qr);

    /* 获取识别到的 QR 码数量 */
    int count = quirc_count(qr);
    LISA_LOGI(LOG_TAG, "✓ Recognition completed, found %d QR code(s)", count);

    /* 遍历识别到的 QR 码 */
    if (count > 0) {
        LISA_LOGI(LOG_TAG, "Processing %d QR code(s)...", count);
        for (int i = 0; i < count; i++) {
            struct quirc_code code;
            struct quirc_data data;
            quirc_decode_error_t err;

            LISA_LOGI(LOG_TAG, "Extracting QR code %d...", i);
            quirc_extract(qr, i, &code);
            LISA_LOGI(LOG_TAG, "Extracted QR code %d, size: %d", i, code.size);

            /* 解码 QR 码 */
            LISA_LOGI(LOG_TAG, "Decoding QR code %d...", i);
            err = quirc_decode(&code, &data);
            LISA_LOGI(LOG_TAG, "Decode result for QR code %d: %d", i, err);

            if (err) {
                LISA_LOGW(LOG_TAG, "  QR code %d: DECODE FAILED - %s", i, quirc_strerror(err));
            } else {
                LISA_LOGI(LOG_TAG, "  QR code %d: Data: %s", i, data.payload);
            }
        }
    } else {
        LISA_LOGI(LOG_TAG, "No QR codes found (this is expected for test data)");
    }

    /* 清理资源 */
    LISA_LOGI(LOG_TAG, "Destroying quirc object...");
    quirc_destroy(qr);
    LISA_LOGI(LOG_TAG, "✓ Cleaned up quirc object");

    LISA_LOGI(LOG_TAG, "=== quirc basic test completed ===");
}

/* 测试错误码字符串 */
static void test_error_strings(void)
{
    LISA_LOGI(LOG_TAG, "=== Testing error code strings ===");

    quirc_decode_error_t errors[] = {
        QUIRC_SUCCESS,
        QUIRC_ERROR_INVALID_GRID_SIZE,
        QUIRC_ERROR_INVALID_VERSION,
        QUIRC_ERROR_FORMAT_ECC,
        QUIRC_ERROR_DATA_ECC,
        QUIRC_ERROR_UNKNOWN_DATA_TYPE,
        QUIRC_ERROR_DATA_OVERFLOW,
        QUIRC_ERROR_DATA_UNDERFLOW
    };

    for (int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        LISA_LOGI(LOG_TAG, "  Error %d: %s", errors[i], quirc_strerror(errors[i]));
    }

    LISA_LOGI(LOG_TAG, "=== Error string test completed ===");
}

#ifdef USE_QR_TEST_DATA
/* 测试真实的 QR 码数据 */
static void test_real_qr_codes(void)
{
    LISA_LOGI(LOG_TAG, "");
    LISA_LOGI(LOG_TAG, "=== Testing Real QR Codes ===");
    LISA_LOGI(LOG_TAG, "Total test cases: %d", QR_TEST_COUNT);
    LISA_LOGI(LOG_TAG, "");

    int passed = 0;
    int failed = 0;
    uint64_t total_decode_time = 0;
    uint64_t total_identify_time = 0;
    uint64_t min_decode_time = UINT64_MAX;
    uint64_t max_decode_time = 0;

    for (int test_idx = 0; test_idx < QR_TEST_COUNT; test_idx++) {
        const qr_test_data_t *test = &qr_test_cases[test_idx];
        struct quirc *qr;

        LISA_LOGI(LOG_TAG, "--- Test %d/%d: %s ---", test_idx + 1, QR_TEST_COUNT, test->name);
        LISA_LOGI(LOG_TAG, "Image size: %dx%d", test->width, test->height);
        LISA_LOGI(LOG_TAG, "Expected data: \"%s\"", test->expected_data);

        uint64_t start_time, end_time, identify_time, decode_time;

        /* 创建 quirc 对象 */
        qr = quirc_new();
        if (!qr) {
            LISA_LOGE(LOG_TAG, "Failed to create quirc object");
            failed++;
            continue;
        }

        /* 设置图像尺寸 */
        if (quirc_resize(qr, test->width, test->height) < 0) {
            LISA_LOGE(LOG_TAG, "Failed to resize quirc object");
            quirc_destroy(qr);
            failed++;
            continue;
        }

        /* 获取图像缓冲区并复制数据 */
        int w, h;
        uint8_t *image = quirc_begin(qr, &w, &h);
        memcpy(image, test->data, w * h);

        /* 开始识别 */
        start_time = lisa_os_get_tick_ms();
        quirc_end(qr);
        end_time = lisa_os_get_tick_ms();
        identify_time = end_time - start_time;
        total_identify_time += identify_time;

        /* 获取识别到的 QR 码数量 */
        int count = quirc_count(qr);
        LISA_LOGI(LOG_TAG, "Found %d QR code(s) (identify time: %llu ms)", count, identify_time);

        int test_passed = 0;

        if (count > 0) {
            for (int i = 0; i < count; i++) {
                struct quirc_code code;
                struct quirc_data data;
                quirc_decode_error_t err;

                quirc_extract(qr, i, &code);

                /* 解码 QR 码 */
                start_time = lisa_os_get_tick_ms();
                err = quirc_decode(&code, &data);
                end_time = lisa_os_get_tick_ms();
                decode_time = end_time - start_time;

                if (err) {
                    LISA_LOGW(LOG_TAG, "QR code %d decode failed: %s (decode time: %llu ms)", i, quirc_strerror(err), decode_time);
                    
                    /* Try flipping for mirrored codes */
                    if (err == QUIRC_ERROR_DATA_ECC) {
                        quirc_flip(&code);
                        err = quirc_decode(&code, &data);
                        if (err == QUIRC_SUCCESS) {
                            LISA_LOGI(LOG_TAG, "Decoded after flip!");
                        }
                    }
                }

                if (err == QUIRC_SUCCESS) {
                    total_decode_time += decode_time;
                    if (decode_time < min_decode_time) min_decode_time = decode_time;
                    if (decode_time > max_decode_time) max_decode_time = decode_time;

                    LISA_LOGI(LOG_TAG, "Decoded data: \"%s\" (decode time: %llu ms)", data.payload, decode_time);
                    LISA_LOGI(LOG_TAG, "Data type: %d, ECC level: %d", data.data_type, data.ecc_level);
                    LISA_LOGI(LOG_TAG, "Version: %d, Payload length: %d", data.version, data.payload_len);

                    /* 验证解码结果 */
                    if (strcmp((char *)data.payload, test->expected_data) == 0) {
                        LISA_LOGI(LOG_TAG, "✓ TEST PASSED: Data matches expected!");
                        test_passed = 1;
                    } else {
                        LISA_LOGE(LOG_TAG, "✗ TEST FAILED: Data mismatch!");
                        LISA_LOGE(LOG_TAG, "  Expected: \"%s\"", test->expected_data);
                        LISA_LOGE(LOG_TAG, "  Got:      \"%s\"", data.payload);
                    }
                } else {
                    LISA_LOGE(LOG_TAG, "✗ Decode failed: %s", quirc_strerror(err));
                }
            }
        } else {
            LISA_LOGE(LOG_TAG, "✗ TEST FAILED: No QR code detected!");
        }

        if (test_passed) {
            passed++;
        } else {
            failed++;
        }

        quirc_destroy(qr);
        LISA_LOGI(LOG_TAG, "");
    }

    LISA_LOGI(LOG_TAG, "=== Test Summary ===");
    LISA_LOGI(LOG_TAG, "Total: %d, Passed: %d, Failed: %d", QR_TEST_COUNT, passed, failed);
    LISA_LOGI(LOG_TAG, "Success rate: %d%%", (passed * 100) / QR_TEST_COUNT);
    LISA_LOGI(LOG_TAG, "");
    
    LISA_LOGI(LOG_TAG, "=== Performance Statistics ===");
    LISA_LOGI(LOG_TAG, "Total identify time: %llu ms", total_identify_time);
    LISA_LOGI(LOG_TAG, "Average identify time: %llu ms", total_identify_time / QR_TEST_COUNT);
    if (passed > 0) {
        LISA_LOGI(LOG_TAG, "Total decode time: %llu ms", total_decode_time);
        LISA_LOGI(LOG_TAG, "Average decode time: %llu ms", total_decode_time / passed);
        LISA_LOGI(LOG_TAG, "Min decode time: %llu ms", min_decode_time);
        LISA_LOGI(LOG_TAG, "Max decode time: %llu ms", max_decode_time);
    }
    LISA_LOGI(LOG_TAG, "");
}
#endif /* USE_QR_TEST_DATA */

int main(int argc, char **argv)
{
    LISA_LOGI(LOG_TAG, "quirc Sample Application");
    LISA_LOGI(LOG_TAG, "========================");
    LISA_LOGI(LOG_TAG, "Library version: %s", quirc_version());
    LISA_LOGI(LOG_TAG, "");

#ifdef USE_QR_TEST_DATA
    /* 测试真实的 QR 码数据 */
    LISA_LOGI(LOG_TAG, "Running QR code decode tests with pre-generated data...");
    test_real_qr_codes();
#else
    /* 测试基本功能 */
    LISA_LOGI(LOG_TAG, "Running basic API test...");
    test_quirc_basic();
    LISA_LOGI(LOG_TAG, "");

    /* 测试错误字符串 */
    test_error_strings();

    LISA_LOGI(LOG_TAG, "");
    LISA_LOGI(LOG_TAG, "Note: This is a basic API test without real QR code images.");
    LISA_LOGI(LOG_TAG, "");
    LISA_LOGI(LOG_TAG, "To test with real QR codes:");
    LISA_LOGI(LOG_TAG, "  1. Run: tools/generate_qr.sh");
    LISA_LOGI(LOG_TAG, "  2. Uncomment '#define USE_QR_TEST_DATA' in main.c");
    LISA_LOGI(LOG_TAG, "  3. Rebuild and run");
    LISA_LOGI(LOG_TAG, "");
    LISA_LOGI(LOG_TAG, "In real applications, you need to:");
    LISA_LOGI(LOG_TAG, "  - Capture image from camera or load from file");
    LISA_LOGI(LOG_TAG, "  - Convert to grayscale if necessary");
    LISA_LOGI(LOG_TAG, "  - Fill the quirc image buffer with actual data");
    LISA_LOGI(LOG_TAG, "  - Process and decode QR codes");
#endif

    LISA_LOGI(LOG_TAG, "");
    LISA_LOGI(LOG_TAG, "Program completed successfully!");

    return 0;
}
