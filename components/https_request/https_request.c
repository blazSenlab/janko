#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"   
#include "esp_sntp.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_tls.h"
#include "sdkconfig.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "time_sync.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "ota.senlab.io"
#define WEB_PORT "443"
#define WEB_URL "https://ota.senlab.io/ota/tracccivil/app_update.bin"
#define WEB_URL_1 "https://ota.senlab.io/ota/tracccivil/test.txt"

#define SERVER_URL_MAX_SZ 256

static const char *TAG = "example";

#define RESPONSE_BUFFER_SIZE 2048
static char response_buffer[RESPONSE_BUFFER_SIZE];

/* Timer interval once every day (24 Hours) */
#define TIME_PERIOD (86400000000ULL)

// static const char HOWSMYSSL_REQUEST[] = "GET " WEB_URL " HTTP/1.1\r\n"
//                              "Host: "WEB_SERVER"\r\n"
//                              "User-Agent: esp-idf/1.0 esp32\r\n"
//                              "\r\n";

static const char RANGED_REQUEST_TEMPLATE[] = "GET " WEB_URL " HTTP/1.1\r\n"
                            "Host: "WEB_SERVER"\r\n"
                            "User-Agent: esp-idf/1.0 esp32\r\n"
                            "Range: bytes=%d-%d\r\n"
                            "\r\n";

static const char HEAD_REQUEST[] = "HEAD " WEB_URL " HTTP/1.1\r\n"
                            "Host: "WEB_SERVER"\r\n"
                            "User-Agent: esp-idf/1.0 esp32\r\n"
                            "\r\n";

static const char GET_REQUEST1[] = "GET " WEB_URL_1 " HTTP/1.1\r\n"
                            "Host: "WEB_SERVER"\r\n"
                            "User-Agent: esp-idf/1.0 esp32\r\n"
                            "\r\n";

#ifdef CONFIG_EXAMPLE_CLIENT_SESSION_TICKETS
static const char LOCAL_SRV_REQUEST[] = "GET " CONFIG_EXAMPLE_LOCAL_SERVER_URL " HTTP/1.1\r\n"
                             "Host: "WEB_SERVER"\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "\r\n";
#endif

typedef enum {
    REQUEST_GET,
    REQUEST_HEAD_1,
} request_type_t;

// Constants and extern declarations...
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

extern const uint8_t local_server_cert_pem_start[] asm("_binary_local_server_cert_pem_start");
extern const uint8_t local_server_cert_pem_end[]   asm("_binary_local_server_cert_pem_end");

esp_tls_t *tls = NULL;

static int https_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST, request_type_t reqType)
{
    char buf[1024];
    char request[1024];
    int ret, len;
    char *content_length_ptr;
    int content_length_value = -1;

    if (tls == NULL){
        tls = esp_tls_init();
    }

    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return -1;
    }

    // establish connection, with specified URL, cfg is TLS configuration

    if (esp_tls_conn_http_new_sync(WEB_SERVER_URL, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        esp_tls_conn_destroy(tls);
        tls = NULL;
        return -1;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                REQUEST + written_bytes,
                                strlen(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            esp_tls_conn_destroy(tls);
            tls = NULL;
            return -1;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");

    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            for (int countdown = 1; countdown >= 0; countdown--) {
                ESP_LOGI(TAG, "%d...", countdown);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            ESP_LOGI(TAG, "connection closed");
            esp_tls_conn_destroy(tls);
            tls = NULL;
            break;
        }

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);

        strncat(response_buffer, buf, len);

        // Check for Content-Length in the buffer
        content_length_ptr = strstr(buf, "Content-Length: ");
        if (content_length_ptr) {
            sscanf(content_length_ptr, "Content-Length: %d", &content_length_value);
            ESP_LOGI(TAG, "Content-Length is %d", content_length_value);
        }

        if (reqType == REQUEST_HEAD_1 && strstr(buf, "\r\n\r\n")) {
            break;
        }

    } while (1);

    esp_tls_conn_destroy(tls);
    tls = NULL;

    return content_length_value;
}

void download_file_in_ranges(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL) {
    int total_size = https_request(cfg, WEB_SERVER_URL, HEAD_REQUEST, REQUEST_HEAD_1);
    int buffer_size = 1024;  
    int start_byte = 0;

    char ranged_request[1024];

    while (start_byte < total_size) {
        int end_byte = start_byte + buffer_size - 1;
        if (end_byte >= total_size) {
            end_byte = total_size - 1;
        }

        // Construct the ranged request
        sprintf(ranged_request, RANGED_REQUEST_TEMPLATE, start_byte, end_byte);

        (void)printf("Ranged Request:\n%s\n", ranged_request);

        https_request(cfg, WEB_SERVER_URL, ranged_request, REQUEST_GET);

        start_byte = end_byte + 1;
    }
}

int compare_version(){
    ESP_LOGI(TAG, "compare version of firmware");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

    const char *web_url = "https://ota.senlab.io/ota/tracccivil/test.txt";

    https_request(cfg, web_url, GET_REQUEST1, REQUEST_GET);
    ESP_LOGI(TAG, "Response: %s", response_buffer);
    
    if(strcmp(response_buffer, "hello world") != 0){
        (void)printf("nova datoteka");
        int i = https_request(cfg, WEB_URL, HEAD_REQUEST, REQUEST_HEAD_1);
        (void)printf(".bin file size: %d", i);
        return i;
    } else {
        (void)printf("stara datoteka");
        return 1;
    }
}

static void https_get_request_using_cacert_buf(void)
{
    ESP_LOGI(TAG, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };
    download_file_in_ranges(cfg, WEB_URL);
}

static void https_request_task(void *pvparameters)
{
    ESP_LOGI(TAG, "Start https_request example");

    ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
    // https_get_request_using_cacert_buf();
    compare_version();
    ESP_LOGI(TAG, "Finish https_request example");
    vTaskDelete(NULL);
}

