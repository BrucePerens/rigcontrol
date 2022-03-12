idf_component_register(SRCS
                       "user.c"
                       "user_web_handlers.c"
                       "user_commands.c"
                    INCLUDE_DIRS "."
                    REQUIRES console driver esp_timer freertos hal lwip nvs_flash esp_http_server)
