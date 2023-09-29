#ifndef HTTPS_REQUEST_H
#define HTTPS_REQUEST_H

#include "esp_tls.h"

// Function declarations
void https_request(esp_tls_cfg_t cfg, const char *REQUEST, request_type_t reqType, uint8_t *buffer);
void ranged_https_request(uint8_t **buffer, size_t *buffer_size);
bool compare_version();

#endif // HTTPS_REQUEST_H

