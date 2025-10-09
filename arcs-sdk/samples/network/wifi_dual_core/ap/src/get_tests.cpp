#include <cpr/cpr.h>
#include <iostream>
#include <curl/curl.h>
#include <curl/easy.h>

extern "C" {

extern const char testtls_ca_pem[];

using namespace std;
using namespace cpr;

// 加载CA根证书
SslOptions LoadCertificatesFromMemory()
{
    printf("Load TLS Certificates From Memory (blob)\n");
    SslOptions options;
    string all_certs(testtls_ca_pem);
    options.ca_buffer = move(all_certs);
    return options;
}

int http_test_init(void)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        printf("Failed to initialize CURL global\n");
        return -1;
    }
    printf("CURL global init success\n");
    return 0;
}

int cpr_test(void)
{
    printf("Starting CPR HTTPS test...\n");

    // HTTPS设置 - 配置SSL选项
    cpr::SslOptions ssl_options = LoadCertificatesFromMemory();

    ssl_options.ssl_version = CURL_SSLVERSION_TLSv1_2;

    cpr::Response response = cpr::Get(cpr::Url{"https://testtls.com"}, // 使用证书中的域名
                                      cpr::VerifySsl{true},            // 开启SSL验证
                                      ssl_options,                     // 包含我们的证书数据
                                      cpr::Timeout{30000},             // 30秒超时
                                      cpr::ConnectTimeout{10000}       // 10秒连接超时
    );

    // 检查请求是否成功
    if (response.status_code == 200) {
        printf("HTTPS Request successful!\n");
        printf("Status code: %d\n", response.status_code);
        printf("Response length: %zu bytes\n", response.text.length());
        printf("Response preview: %.100s...\n", response.text.c_str());
    } else {
        printf("HTTPS Request failed, status code: %d\n", response.status_code);
        printf("Error message: %s\n", response.error.message.c_str());
    }
    return 0;
}

// 只统计接收数据大小的回调
static size_t write_callback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    const size_t total = size * nmemb;
    printf("[%.2f] Received %zu bytes\n", (float)clock() / CLOCKS_PER_SEC, total);
    return total;
}

// 调试回调函数
static int debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userp)
{
    if (type == CURLINFO_TEXT) {
        printf("[CURL] %.*s\n", (int)size, data);
    }
    return 0;
}

void curl_test(void)
{
    CURL *curl;
    CURLcode res;
    char error_buffer[CURL_ERROR_SIZE];

    curl = curl_easy_init();
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, "https://testtls.com");

        struct curl_blob blob;
        blob.data = (unsigned char *)testtls_ca_pem;
        blob.len = strlen(testtls_ca_pem);
        blob.flags = CURL_BLOB_COPY;
        if (curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob) != CURLE_OK) {
            printf("Failed to set CA cert\n");
            return;
        }

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L); // 验证对端证书
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L); // 严格主机名验证
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback); // 回调
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);                  // 10秒总超时
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);           // 5秒连接超时

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);

        // Set error buffer
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            if (error_buffer[0] != '\0') {
                printf("Error details: %s\n", error_buffer);
            }

            long verify_result = 0;
            curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verify_result);
            printf("SSL verify result: %ld\n", verify_result);

            const char *ip = NULL;
            curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &ip);
            printf("Connected to IP: %s\n", ip ? ip : "unknown");
        } else {

            long verify_result = 0;
            curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &verify_result);
            printf("SSL verify result: %ld\n", verify_result);

            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            printf("HTTP response code: %ld\n", http_code);
            printf("Request successful!\n");
        }

        // Clean up
        curl_easy_cleanup(curl);
    } else {
        printf("Failed to initialize curl\n");
    }
}
}