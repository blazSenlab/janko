idf_component_register(SRCS "time_sync.c" "https_request.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_timer esp-tls
                    EMBED_TXTFILES server_root_cert.pem local_server_cert.pem)
                    
