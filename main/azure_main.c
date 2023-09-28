/* esp-azure example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_system.h"
#include "esp_wifi.h"
#ifdef CONFIG_IDF_TARGET_ESP8266 || (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0))
#include "esp_event_loop.h"
#else
#include "esp_event.h"
#endif
#include "esp_log.h"

#include "nvs_flash.h"

#include "../components/azure_connection/azure_connection.h"
// #include "azure_connection.h"
#include "wifi_connection.h"
#include "../components/https_request/https_request.c"

#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

/* FreeRTOS event group to signal when we are connected & ready to make a request */
// static EventGroupHandle_t wifi_event_group;

#ifndef BIT0
#define BIT0 (0x1 << 0)
#endif
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
// const int CONNECTED_BIT = BIT0;

// static const char *TAG = "azure";

SECURE_DEVICE_TYPE hsm_type;
PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport;
IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;
IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;
CLIENT_SAMPLE_INFO user_ctx;
IOTHUB_CLIENT_SAMPLE_INFO iothub_info;
PROV_DEVICE_LL_HANDLE handle;
bool traceOn;


void azure_task(void *pvParameter)
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP success!");

    // Initialize IoT Hub
    init_iot_hub(&hsm_type, &traceOn, &prov_transport, &user_ctx);

    if (!provisioning(&handle, &traceOn, &user_ctx, &prov_transport)) {
        ESP_LOGE(TAG, "Provisioning failed!");
        vTaskDelete(NULL);
        return;
    }

    // Create IoT device handle
    if (!create_iot_device_handle(&traceOn, &iothub_transport, &device_ll_handle, &user_ctx)) {
        ESP_LOGE(TAG, "Failed to create IoT device handle!");
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay for 1 second
        vTaskDelete(NULL);
        return;
    }
    
    const unsigned char buffer[] = "Your message content here"; // Replace with your actual message content
    size_t buffer_length = sizeof(buffer) - 1; // Subtract 1 to exclude the null terminator

    int i = 0;

    // Repeatedly send the message every 1 second
    while (i < 10) {
        sendMessage(&device_ll_handle, &user_ctx, buffer, buffer_length);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
        i = i + 1;
        (void)printf("i: %d", i);
    }

    disconnect_and_deinit(&device_ll_handle, &user_ctx);

    // This line will never be reached due to the infinite loop above
    // vTaskDelete(NULL);

    vTaskDelete(NULL);
}

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialise_wifi();

    if (esp_reset_reason() == ESP_RST_POWERON) {
        ESP_LOGI(TAG, "Updating time from NVS");
        ESP_ERROR_CHECK(update_time_from_nvs());
    }

    xTaskCreate(&https_request_task, "https_get_task", 8192, NULL, 5, NULL);
}
