#ifndef HTTPS_REQUEST_H
#define HTTPS_REQUEST_H

#include "esp_tls.h"

// Function declarations
void https_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST, request_type_t reqType);
void download_file_in_ranges(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL);
void https_get_request_using_cacert_buf(void);
bool compare_version();

#ifdef CONFIG_EXAMPLE_CLIENT_SESSION_TICKETS
void https_get_request_using_already_saved_session(const char *url);
#endif

#endif // HTTPS_REQUEST_H

