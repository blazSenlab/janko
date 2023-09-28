// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// CAVEAT: This sample is to demonstrate azure IoT client concepts only and is not a guide design principles or style
// Checking of return codes and error values shall be omitted for brevity.  Please practice sound engineering practices
// when writing production code.
#include "azure_connection.h"

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

//
// The protocol you wish to use should be uncommented
//
#define SAMPLE_MQTT
//#define SAMPLE_MQTT_OVER_WEBSOCKETS
//#define SAMPLE_AMQP
//#define SAMPLE_AMQP_OVER_WEBSOCKETS
//#define SAMPLE_HTTP

#ifdef SAMPLE_MQTT
#include "iothubtransportmqtt.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#endif // SAMPLE_MQTT

// This sample is to demostrate iothub reconnection with provisioning and should not
// be confused as production code

MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(PROV_DEVICE_RESULT, PROV_DEVICE_RESULT_VALUE);
MU_DEFINE_ENUM_STRINGS_WITHOUT_INVALID(PROV_DEVICE_REG_STATUS, PROV_DEVICE_REG_STATUS_VALUES);

static const char* global_prov_uri = "global.azure-devices-provisioning.net";
static const char* id_scope = CONFIG_DPS_ID_SCOPE;

#define PROXY_PORT                  8888
#define MESSAGES_TO_SEND            2
#define TIME_BETWEEN_MESSAGES       2

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context)
{
    (void)message;
    IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
    (void)printf("Stop message recieved from IoTHub\r\n");
    iothub_info->stop_running = 1;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void registration_status_callback(PROV_DEVICE_REG_STATUS reg_status, void* user_context)
{
    (void)user_context;
    (void)printf("Provisioning Status: %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_REG_STATUS, reg_status));
}

static void iothub_connection_status(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    if (user_context == NULL)
    {
        printf("iothub_connection_status user_context is NULL\r\n");
    }
    else
    {
        (void)printf("iothub_connection_status: \r\n");
        IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
        if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
        {
            (void)printf("connected\r\n");
            iothub_info->connected = 1;
        }
        else
        {
            (void)printf("not connected\r\n");
            iothub_info->connected = 0;
            iothub_info->stop_running = 1;
        }
    }
}

static void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri, const char* device_id, void* user_context)
{
    if (user_context == NULL)
    {
        printf("user_context is NULL\r\n");
    }
    else
    {
        CLIENT_SAMPLE_INFO* user_ctx = (CLIENT_SAMPLE_INFO*)user_context;
        if (register_result == PROV_DEVICE_RESULT_OK)
        {
            (void)printf("Registration Information received from service: %s!\r\n", iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->iothub_uri, iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->device_id, device_id);
            user_ctx->registration_complete = 1;
        }
        else
        {
            (void)printf("Failure encountered on registration %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_RESULT, register_result) );
            user_ctx->registration_complete = 2;
        }
    }
}

const IO_INTERFACE_DESCRIPTION* socketio_get_interface_description(void)
{
    return NULL;
}

void init_iot_hub(SECURE_DEVICE_TYPE *hsm_type, bool *traceOn, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION *prov_transport, CLIENT_SAMPLE_INFO *user_ctx)
{
    if (!hsm_type || !traceOn || !prov_transport || !user_ctx) {
        return;
    }

    *hsm_type = SECURE_DEVICE_TYPE_X509;
    *traceOn = false;

    (void)IoTHub_Init();
    (void)prov_dev_security_init(*hsm_type);

#ifdef SAMPLE_MQTT
    *prov_transport = Prov_Device_MQTT_Protocol;
#endif
    user_ctx->registration_complete = 0;
    user_ctx->sleep_time = 10;
}

bool create_iot_device_handle(bool *traceOn, IOTHUB_CLIENT_TRANSPORT_PROVIDER *iothub_transport, IOTHUB_DEVICE_CLIENT_LL_HANDLE *device_ll_handle, CLIENT_SAMPLE_INFO *user_ctx)
{
    if (!traceOn || !iothub_transport || !device_ll_handle || !user_ctx) {
        return false;
    }

    (void)printf("Creating IoTHub Device handle\r\n");
    if ((*device_ll_handle = IoTHubDeviceClient_LL_CreateFromDeviceAuth(user_ctx->iothub_uri, user_ctx->device_id, MQTT_Protocol)) == NULL)
    {
        (void)printf("failed create IoTHub client from connection string %s!\r\n", user_ctx->iothub_uri);
        return false;
    }
    else
    {
        (void)printf("successfully created iothub from connection string\r\n");
        TICK_COUNTER_HANDLE tick_counter_handle = tickcounter_create();
        tickcounter_ms_t current_tick;
        tickcounter_ms_t last_send_time = 0;
        size_t msg_count = 0;
        iothub_info.stop_running = 0; // Use the global iothub_info
        iothub_info.connected = 0;    // Use the global iothub_info

        (void)printf("iothub: %d, %d \r\n", iothub_info.stop_running, iothub_info.connected);

        IOTHUB_CLIENT_RESULT result = IoTHubDeviceClient_LL_SetConnectionStatusCallback(*device_ll_handle, iothub_connection_status, &iothub_info);
        if(result == IOTHUB_CLIENT_OK){
            (void)printf("successful\r\n");
        } else {
            (void)printf("error\r\n");
            // Optionally, you can print the error code
            (void)printf("Error code: %d\r\n", result);
            return false;
        }
        result = IoTHubDeviceClient_LL_SetOption(*device_ll_handle, OPTION_LOG_TRACE, traceOn);
        if(result == IOTHUB_CLIENT_OK){
            (void)printf("successful\r\n");
        } else {
            (void)printf("error\r\n");
            // Optionally, you can print the error code
            (void)printf("Error code: %d\r\n", result);
            return false;
        }
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
        // Setting the Trusted Certificate.  This is only necessary on system with without
        // built in certificate stores.
        IoTHubDeviceClient_LL_SetOption(*device_ll_handle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES
    }

    return true;
}


bool provisioning(PROV_DEVICE_LL_HANDLE *handle, bool *traceOn, CLIENT_SAMPLE_INFO *user_ctx, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION *prov_transport)
{
    if ((*handle = Prov_Device_LL_Create(global_prov_uri, id_scope, Prov_Device_MQTT_Protocol)) == NULL)
    {
        (void)printf("failed calling Prov_Device_LL_Create\r\n");
        return false;
    }
    else 
    {
        (void)printf("prov_device_LL_create succesful\r\n");
        Prov_Device_LL_SetOption(*handle, PROV_OPTION_LOG_TRACE, traceOn);

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
        // Setting the Trusted Certificate.  This is only necessary on system with without
        // built in certificate stores.
        Prov_Device_LL_SetOption(*handle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES

        if (Prov_Device_LL_Register_Device(*handle, register_device_callback, user_ctx, registration_status_callback, user_ctx) != PROV_DEVICE_RESULT_OK)
        {
            (void)printf("failed calling Prov_Device_LL_Register_Device\r\n");
            return false;
        }
        else
        {
            do
            {
                Prov_Device_LL_DoWork(*handle);
                ThreadAPI_Sleep(user_ctx->sleep_time);
            } while (user_ctx->registration_complete == 0);
        }
        (void)printf("destroying handle\r\n");
        Prov_Device_LL_Destroy(*handle);
    }

    if (user_ctx->registration_complete != 1)
    {
        (void)printf("registration failed!\r\n");
        return false;
    }
    else
    {
        (void)printf("registration completed\r\n");
        IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;
        // Protocol to USE - HTTP, AMQP, AMQP_WS, MQTT, MQTT_WS
#if defined(SAMPLE_MQTT) || defined(SAMPLE_HTTP) // HTTP sample will use mqtt protocol
        iothub_transport = MQTT_Protocol;
#endif // SAMPLE_MQTT
    }
    return true;
}

void sendMessage(IOTHUB_DEVICE_CLIENT_LL_HANDLE *device_ll_handle, CLIENT_SAMPLE_INFO *user_ctx, const unsigned char* buffer, size_t buffer_length)
{
    if (!device_ll_handle || !user_ctx || !buffer) {
        return;
    }

    // Set the message callback
    IOTHUB_CLIENT_RESULT result = IoTHubDeviceClient_LL_SetMessageCallback(*device_ll_handle, receive_msg_callback, &iothub_info);
    if(result != IOTHUB_CLIENT_OK){
        (void)printf("Error setting message callback. Error code: %d\r\n", result);
        return;
    }

    // Check if the device is connected to the IoT Hub
    if (iothub_info.connected != 0)
    {
        // Create a message handle from the provided buffer
        IOTHUB_MESSAGE_HANDLE msg_handle = IoTHubMessage_CreateFromByteArray(buffer, buffer_length);
        if (msg_handle == NULL)
        {
            (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
        }
        else
        {
            // Send the message
            if (IoTHubDeviceClient_LL_SendEventAsync(*device_ll_handle, msg_handle, NULL, NULL) != IOTHUB_CLIENT_OK)
            {
                (void)printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
            }
            else
            {
                (void)printf("IoTHubClient_LL_SendEventAsync accepted the message for transmission to IoT Hub.\r\n");
            }
            IoTHubMessage_Destroy(msg_handle);
        }
    }

    // Perform any pending work related to the IoT Hub client
    IoTHubDeviceClient_LL_DoWork(*device_ll_handle);
}

void disconnect_and_deinit(IOTHUB_DEVICE_CLIENT_LL_HANDLE *device_ll_handle, CLIENT_SAMPLE_INFO *user_ctx)
{
    if (!device_ll_handle || !user_ctx) {
        return;
    }

    (void)printf("disconnecting");

    IoTHubDeviceClient_LL_Destroy(*device_ll_handle);
    free(user_ctx->iothub_uri);
    free(user_ctx->device_id);
    prov_dev_security_deinit();
    IoTHub_Deinit();
}
