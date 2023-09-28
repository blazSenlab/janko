#ifndef _ESP_2_NRF
#define _ESP_2_NRF
#include "stdint.h"
#include "stdbool.h"
#include "esp_err.h"

void init_esp2nrf();
/*
 * Message content [length_0][length_1][type][data_0][data_1]...[data_n][crc_byte0][crc_byte1][crc_byte2][crc_byte4]
 * length applies only to data and type bytes so max data size is COM_BUFFER_SIZE - 7
 */

typedef enum
{
    // from esp
    CRC_FAILED,
    UPDATE_STATUS,
    UPDATE_PART,
    UPDATE_LAST_PART,
    DATA_SEND_STATUS,
    REBOOT_READY
} ReplyType;

typedef enum
{
    // from nrf
    NRF_CRC_FAILED,
    IS_UPDATE_READY,
    NEW_DATA,
    NEXT_UPDATE_PART,
    REBOOT,
    // from errors *data is NULL, data_size is 0
    TIMEOUT_START_SIGNAL,
    TIMEOUT_MSG,
    ERROR_MESSAGE_SIZE,
    ERROR_CRC,
    ERROR_INVALID_TYPE

} MessageType;

/**
 * @brief return 0 if there isnt new update, 1 if there is new update
 *        and 2 if couldnt get info (no wifi for example)
 *
 */
typedef int (*UpdateReadyCb)(void);

/**
 * @brief send the received data to the server, return true if operstion was successfull, false otherwise
 *
 */
typedef bool (*NewDataCb)(uint8_t*, size_t);

/**
 * @brief malloc uint8_t** and fill it with data, set size_t to the data size, return true if it is the last part false otherwise
 *
 */
typedef bool (*FirmwarePartCb)(uint8_t**, size_t*);

/**
 * @brief cleanup before reboot if necessary
 *
 */
typedef void (*RebootCb)(void);

void set_update_ready_cb(UpdateReadyCb cb);

void set_new_data_cb(NewDataCb cb);

void set_fimrware_part_cb(FirmwarePartCb cb);

void set_reboot_cb(RebootCb cb);


#endif
