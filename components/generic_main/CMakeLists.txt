idf_component_register(SRCS "generic_main.c"
                    "wifi.c"
                    "webserver.c"
                    INCLUDE_DIRS "." "include"
                    REQUIRES console commands driver esp_timer freertos hal lwip nvs_flash esp_wifi wpa_supplicant esp_http_server main)
