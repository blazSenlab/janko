#include <stdio.h>
#include "include/esp_2_nrf.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <rom/ets_sys.h>
#include "esp_timer.h"
#include "freertos/task.h"
#include "string.h"

// CONFIGURE
#define P1 26
#define P2 25
#define COM_BUFFER_SIZE 1260

/*
    100ms+1.8ms+5ms per packet + approx 30us per bit
*/
// same for both
#define START_SIGNAL_LENGTH 40000      // unit in us
#define MSG_TIMEOUT_START_SIGNAL 50000 // unit is us
#define PAUSE_BEFORE_START 180         // unit in us
#define MSG_END_TIME 15000             // unit in us
#define MSG_TIMEOUT 8000000            // unit is us
#define TRANSMIT_MSG_PAUSE_US 30       // unit in us

#define DELAY_LISTENING 10 // unit in ms

#define PAUSE_BEFORE_REPLY 4000 // unit in ms
#define LAST_VALID_TYPE REBOOT

#define POLY 0x82f63b78

TaskHandle_t com_task_handle = NULL;

uint8_t com_buf[COM_BUFFER_SIZE];
uint8_t last_data[COM_BUFFER_SIZE];
bool last_data_null = false;
ReplyType last_reply;
size_t last_size = 0;

UpdateReadyCb update_ready_cb = NULL;
NewDataCb new_data_cb = NULL;
FirmwarePartCb firmware_part_cb = NULL;
RebootCb reboot_cb = NULL;

int64_t reply_timeout_timer = 0;

uint32_t masks[] = {
    0x00000001U, 0x00000002U, 0x00000004U, 0x00000008U,
    0x00000010U, 0x00000020U, 0x00000040U, 0x00000080U,
    0x00000100U, 0x00000200U, 0x00000400U, 0x00000800U,
    0x00001000U, 0x00002000U, 0x00004000U, 0x00008000U,
    0x00010000U, 0x00020000U, 0x00040000U, 0x00080000U,
    0x00100000U, 0x00200000U, 0x00400000U, 0x00800000U,
    0x01000000U, 0x02000000U, 0x04000000U, 0x08000000U,
    0x10000000U, 0x20000000U, 0x40000000U, 0x80000000U};
uint8_t masks_inverted[] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F};

uint8_t btest[] = {
    62, 171, 73, 207, 113, 75, 206, 58, 117, 167, 79, 118,
    234, 126, 100, 255, 129, 235, 97, 253, 254, 195, 155, 103,
    191, 13, 233, 140, 126, 78, 50, 189, 249, 124, 140, 106,
    199, 91, 164, 60, 2, 244, 178, 237, 114, 22, 236, 243,
    1, 77, 240, 0, 16, 139, 103, 207, 153, 80, 91, 23,
    159, 142, 212, 152, 10, 97, 3, 209, 188, 167, 13, 190};

esp_err_t send_to_nrf(ReplyType t, uint8_t *data, uint16_t data_size);

void set_update_ready_cb(UpdateReadyCb cb)
{
    update_ready_cb = cb;
}

void set_new_data_cb(NewDataCb cb)
{
    new_data_cb = cb;
}

void set_fimrware_part_cb(FirmwarePartCb cb)
{
    firmware_part_cb = cb;
}

void set_reboot_cb(RebootCb cb)
{
    reboot_cb = cb;
}

uint32_t crc32c(const uint8_t *buf, size_t len)
{
    int k;
    uint32_t crc = 0;
    while (len--)
    {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return crc;
}

void handle_data(MessageType t, uint8_t *d, size_t s)
{
    reply_timeout_timer = esp_timer_get_time();
    xTaskResumeAll();
    switch (t)
    {
    case TIMEOUT_START_SIGNAL:
        send_to_nrf(CRC_FAILED, NULL, 0);
        break;
    case TIMEOUT_MSG:
        send_to_nrf(CRC_FAILED, NULL, 0);
        break;
    case ERROR_MESSAGE_SIZE:
        send_to_nrf(CRC_FAILED, NULL, 0);
        break;
    case ERROR_CRC:
        send_to_nrf(CRC_FAILED, NULL, 0);
        break;
    case ERROR_INVALID_TYPE:
        send_to_nrf(CRC_FAILED, NULL, 0);
        break;
    case IS_UPDATE_READY:
        uint8_t data = update_ready_cb();
        send_to_nrf(UPDATE_STATUS, &data, 1);
        break;
    case NEW_DATA:
        uint8_t res = 1;
        if (new_data_cb(d, s))
        {
            res = 0;
        }
        send_to_nrf(DATA_SEND_STATUS, &res, 1);
        break;
    case NEXT_UPDATE_PART:
        uint8_t *firmware_part;
        size_t part_size;
        ReplyType pt = UPDATE_PART;
        if (firmware_part_cb(&firmware_part, &part_size))
        {
            pt = UPDATE_LAST_PART;
        }
        send_to_nrf(pt, firmware_part, part_size);
        free(firmware_part);
        break;
    case REBOOT:
        if (reboot_cb != NULL)
        {
            reboot_cb();
        }
        send_to_nrf(REBOOT_READY, NULL, 0);
        esp_restart();
        break;
    case NRF_CRC_FAILED:
        if (!last_data_null)
        {
            send_to_nrf(last_reply, last_data, last_size);
        }
        else
        {
            send_to_nrf(last_reply, last_data, last_size);
        }
        break;
    default:
        break;
    }
}

void listen_to_msg()
{
    while (1)
    {
listen_loop_start:
        printf("listening\n");
        vTaskDelay(pdMS_TO_TICKS(5));
        while (gpio_get_level(P2) != 1)
        {
            vTaskDelay(pdMS_TO_TICKS(DELAY_LISTENING));
        }
        vTaskSuspendAll();
        int64_t timeout_timer = esp_timer_get_time();
        while (gpio_get_level(P2))
        {
            if (esp_timer_get_time() - timeout_timer >= MSG_TIMEOUT_START_SIGNAL)
            {
                handle_data(TIMEOUT_START_SIGNAL, NULL, 0);
                goto listen_loop_start;
            }
        }
        bool msg_running = true;
        int step_bit = 0;
        int step_byte = 0;
        int buf_size = 0;
        bool bit_got = false;
        timeout_timer = esp_timer_get_time();
        int64_t msg_end_time = esp_timer_get_time();
        while (msg_running)
        {
            if (!bit_got)
            {
                if (gpio_get_level(P2))
                {
                    if (gpio_get_level(P1))
                    {
                        com_buf[step_byte] |= masks[step_bit];
                    }
                    else
                    {
                        com_buf[step_byte] &= masks_inverted[step_bit];
                    }
                    step_bit++;
                    if (step_bit > 7)
                    {
                        step_byte++;
                        step_bit = 0;
                    }
                    bit_got = true;
                    timeout_timer = esp_timer_get_time();
                }
                else if (esp_timer_get_time() - msg_end_time >= MSG_END_TIME)
                {
                    msg_running = false;
                    buf_size = step_byte;
                }
            }
            else
            {
                if (!gpio_get_level(P2))
                {
                    bit_got = false;
                    timeout_timer = esp_timer_get_time();
                    msg_end_time = esp_timer_get_time();
                }
            }
            if (esp_timer_get_time() - timeout_timer > MSG_TIMEOUT)
            {
                handle_data(TIMEOUT_MSG, NULL, 0);
                goto listen_loop_start;
            }
        }
        uint32_t result = 0;
        result |= com_buf[buf_size - 4] << 0;
        result |= com_buf[buf_size - 3] << 8;
        result |= com_buf[buf_size - 2] << 16;
        result |= com_buf[buf_size - 1] << 24;
        uint16_t data_size = 0;
        data_size |= com_buf[0] << 0;
        data_size |= com_buf[1] << 8;
        if (data_size + 7 != buf_size)
        {
            // printf("err msg size %d shjould be %d\n", data_size, buf_size);
            handle_data(ERROR_MESSAGE_SIZE, NULL, 0);
            continue;
        }
        if (crc32c(&com_buf[3], data_size) != result)
        {
            handle_data(ERROR_CRC, NULL, 0);
            continue;
        }
        if (com_buf[2] > LAST_VALID_TYPE)
        {
            handle_data(ERROR_CRC, NULL, 0);
            continue;
        }
        handle_data(com_buf[2], &com_buf[3], data_size);
    }
}

esp_err_t send_to_nrf(ReplyType t, uint8_t *data, uint16_t data_size)
{
    if (data_size > COM_BUFFER_SIZE - 7)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t err = gpio_set_direction(P1, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
    {
        return err;
    }
    err = gpio_set_direction(P2, GPIO_MODE_OUTPUT);
    if (err != ESP_OK)
    {
        return err;
    }
    if (data != NULL)
    {
        memcpy(last_data, data, data_size * sizeof(uint8_t));
        last_data_null = false;
    }
    else
    {
        last_data_null = true;
    }
    last_reply = t;
    last_size = data_size;
    while (esp_timer_get_time() - reply_timeout_timer < PAUSE_BEFORE_REPLY * 1000)
    {
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    com_buf[0] = data_size & 0xFF;
    com_buf[1] = (data_size >> 8) & 0xFF;
    com_buf[2] = t;
    memcpy(&com_buf[3], data, data_size);
    uint32_t crc_check = crc32c(data, data_size);
    com_buf[3 + data_size] = (crc_check >> 0) & 0xFF;
    com_buf[4 + data_size] = (crc_check >> 8) & 0xFF;
    com_buf[data_size + 5] = (crc_check >> 16) & 0xFF;
    com_buf[data_size + 6] = (crc_check >> 24) & 0xFF;
    // printf("crc buf %d %d %d %d\n", com_buf[data_size + 6], com_buf[data_size + 5], com_buf[data_size + 4], com_buf[data_size + 3]);
    vTaskSuspendAll();
    gpio_set_level(P2, 1);
    ets_delay_us(START_SIGNAL_LENGTH);
    gpio_set_level(P2, 0);
    ets_delay_us(PAUSE_BEFORE_START);
    for (size_t i = 0; i < data_size + 7; i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            gpio_set_level(P1, (com_buf[i] & masks[j]));
            gpio_set_level(P2, 1);
            ets_delay_us(TRANSMIT_MSG_PAUSE_US);
            gpio_set_level(P2, 0);
            ets_delay_us(TRANSMIT_MSG_PAUSE_US);
        }
    }
    gpio_set_level(P2, 0);
    gpio_set_level(P1, 0);
    xTaskResumeAll();
    printf("sent to nrf\n");
    err = gpio_set_direction(P1, GPIO_MODE_INPUT);
    if (err != ESP_OK)
    {
        return ESP_ERR_INVALID_STATE;
    }
    err = gpio_set_direction(P2, GPIO_MODE_INPUT);
    if (err != ESP_OK)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

/*void check_rcv_task(void *pvParameters)
{
    while (1)
    {
        // printf("com task\n");
        if (gpio_get_level(P2) == 1)
        {
            printf("in com task\n");
            // vTaskSuspendAll();
            // int64_t st_time = esp_timer_get_time();
            // bool msg_ready = false;
            while (gpio_get_level(P2))
            {
                if (esp_timer_get_time() - st_time >= START_SIGNAL_LENGTH - ((DELAY_ON_FALSE_SINGAL + DELAY_ON_RX_TASK + 30) * 1000))
                {
                    msg_ready = true;
                }
                if (esp_timer_get_time() - st_time >= MSG_TIMEOUT_START_SIGNAL)
                {
                    printf("MSG timeout\n");
                    msg_ready = false;
                    break;
                }
            }
            if (!msg_ready)
            {
                vTaskDelay(pdMS_TO_TICKS(DELAY_ON_FALSE_SINGAL));
                continue;
            }
            if(listen_to_msg()){
                //xTaskResumeAll();
            }
            listen_to_msg();
        }
        vTaskDelay(pdMS_TO_TICKS(DELAY_ON_RX_TASK));
    }
}*/

void init_esp2nrf()
{
    esp_err_t err;
    assert(MSG_TIMEOUT_START_SIGNAL > START_SIGNAL_LENGTH);
    assert(MSG_END_TIME < MSG_TIMEOUT);
    assert(PAUSE_BEFORE_START < MSG_END_TIME);
    err = gpio_set_direction(P1, GPIO_MODE_INPUT);
    if (err != ESP_OK)
    {
        printf("ERR P1 cfg %d\n", err);
    }
    err = gpio_set_direction(P2, GPIO_MODE_INPUT);
    if (err != ESP_OK)
    {
        printf("ERR P2 cfg %d\n", err);
    }
    printf("gpios set\n");

    err = xTaskCreate(listen_to_msg, "nrf_com_task", 4096, NULL, configMAX_PRIORITIES - 1, &com_task_handle);
    if (err != pdPASS)
    {
        printf("failed to create recv task %d\n", err);
    }
}
