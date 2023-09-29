#ifndef AZURE_CONNECTION_H
#define AZURE_CONNECTION_H

#include <stdio.h>
#include <stdlib.h>

#include "iothub.h"
#include "iothub_message.h"
#include "iothub_client_version.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/http_proxy_io.h"

#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_security_factory.h"
#include "sdkconfig.h"

extern SECURE_DEVICE_TYPE hsm_type;
extern PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport;
extern IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;

typedef struct CLIENT_SAMPLE_INFO_TAG
{
    unsigned int sleep_time;
    char* iothub_uri;
    char* access_key_name;
    char* device_key;
    char* device_id;
    int registration_complete;
} CLIENT_SAMPLE_INFO;

extern CLIENT_SAMPLE_INFO user_ctx;

typedef struct IOTHUB_CLIENT_SAMPLE_INFO_TAG
{
    int connected;
    int stop_running;
} IOTHUB_CLIENT_SAMPLE_INFO;

extern IOTHUB_CLIENT_SAMPLE_INFO iothub_info;

extern PROV_DEVICE_LL_HANDLE handle;

extern bool traceOn;

void init_iot_hub(SECURE_DEVICE_TYPE *hsm_type, bool *traceOn, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION *prov_transport, CLIENT_SAMPLE_INFO *user_ctx);

bool create_iot_device_handle(bool *traceOn, IOTHUB_CLIENT_TRANSPORT_PROVIDER *iothub_transport, CLIENT_SAMPLE_INFO *user_ctx);

bool provisioning(PROV_DEVICE_LL_HANDLE *handle, bool *traceOn, CLIENT_SAMPLE_INFO *user_ctx, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION *prov_transport);

void sendMessage(const unsigned char* buffer, size_t buffer_length);

void disconnect_and_deinit(CLIENT_SAMPLE_INFO *user_ctx);


#endif