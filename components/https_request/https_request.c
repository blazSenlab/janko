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

#define BUFFER_SIZE 1024

char *version = NULL;

static int https_request(esp_tls_cfg_t cfg, const char *REQUEST, request_type_t reqType, uint8_t *buffer)
{
    int ret;
    char *content_length_ptr;
    int content_length_value = -1;

    if (tls == NULL){
        tls = esp_tls_init();
    }

    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return -1;
    }

    if(*REQUEST == GET_REQUEST1){
        if (esp_tls_conn_http_new_sync(WEB_URL_1, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            esp_tls_conn_destroy(tls);
            tls = NULL;
            return -1;
        }
    } else {
        if (esp_tls_conn_http_new_sync(WEB_URL, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
        } else {
            ESP_LOGE(TAG, "Connection failed...");
            esp_tls_conn_destroy(tls);
            tls = NULL;
            return -1;
        }
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
        ret = esp_tls_conn_read(tls, buffer, BUFFER_SIZE);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            esp_tls_conn_destroy(tls);
            tls = NULL;
            break;
        }

        content_length_ptr = strstr((char*)buffer, "Content-Length: ");
        if (content_length_ptr) {
            sscanf(content_length_ptr, "Content-Length: %d", &content_length_value);
            (void)printf("Content-Length is %d", content_length_value);
        }

        if (reqType == REQUEST_HEAD_1 && strstr((char*)buffer, "\r\n\r\n")) {
            break;
        }

        int no1, no2, no3;

        if(REQUEST == GET_REQUEST1){
            version = strstr((char*)buffer, "\r\n\r\n");
            (void)printf("\n\nret: %d \n\n", ret);
            if (version != NULL){
                sscanf(version, "\r\n\r\n%d.%d.%d", &no1, &no2, &no3);
                version += 4;
                (void)printf("version: %s\n", version);

                (void)printf("%d, %d, %d", no1, no2, no3);
            }
        }

    } while (1);

    esp_tls_conn_destroy(tls);
    tls = NULL;

    return content_length_value;
}

int compare_version(){
    ESP_LOGI(TAG, "compare version of firmware");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

    uint8_t *buffer = NULL;
    size_t actual_buffer_size = 1024;
    size_t *buffer_size = &actual_buffer_size;
    buffer = (uint8_t*)malloc(*buffer_size);
    if(buffer == NULL){
        (void)printf("malloc failed");
        return false;
    }

    https_request(cfg, GET_REQUEST1, REQUEST_GET, buffer);
    
    if(strcmp(response_buffer, "hello world") != 0){
        (void)printf("nova datoteka");
        int i = https_request(cfg, HEAD_REQUEST, REQUEST_HEAD_1, buffer);
        (void)printf(".bin file size: %d", i);
        return i;
    } else {
        (void)printf("stara datoteka");
        return 1;
    }
}

int start_byte = 0;
int end_byte = 0;

bool ranged_https_request(uint8_t **buffer, size_t *buffer_size)
{
    ESP_LOGI(TAG, "making ranged request");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_root_cert_pem_start,
        .cacert_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };

    char request_string[1024];

    *buffer = (uint8_t*)malloc(*buffer_size);
    if(*buffer == NULL){
        (void)printf("malloc failed");
        return false;
    }

    (void)printf("\n\nbuffer size: %d\n\n", *buffer_size);

    int total_size = https_request(cfg, HEAD_REQUEST, REQUEST_HEAD_1, *buffer); 

    end_byte = start_byte + BUFFER_SIZE - 1;

    if (end_byte >= total_size){
        end_byte = total_size - 1;
    }

    // while (start_byte < total_size) {
    //     int end_byte = start_byte + BUFFER_SIZE - 1;
    //     if (end_byte >= total_size) {
    //         end_byte = total_size - 1;
    //     }

    // Construct the ranged request
    sprintf(request_string, RANGED_REQUEST_TEMPLATE, start_byte, end_byte);

    (void)printf("Ranged Request:\n%s\n", request_string);

    // (void)printf("\n\n Making https request again \n\n");

    https_request(cfg, request_string, REQUEST_GET, *buffer);

    start_byte = end_byte + 1;
    // }
    if(end_byte < total_size){
        return true;
    } else {
        return false;
    }
    
}

// static void https_request_task(void *pvparameters)
// {
//     ESP_LOGI(TAG, "Start https_request example");

//     uint8_t *actual_buffer = NULL;
//     uint8_t **buffer = &actual_buffer;
//     size_t actual_buffer_size = 1024;
//     size_t *buffer_size = &actual_buffer_size;

//     ESP_LOGI(TAG, "Minimum free heap size: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
//     // ranged_https_request(buffer, buffer_size);
//     compare_version();
//     ESP_LOGI(TAG, "Finish https_request example");
//     vTaskDelete(NULL);
// }

