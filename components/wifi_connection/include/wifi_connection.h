#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

void initialise_wifi(void);

#endif // WIFI_CONNECTION_H
