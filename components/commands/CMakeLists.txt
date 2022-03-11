idf_component_register(SRCS "commands.c"
                       "gpio.c"
                       "param.c"
                       "param_cmd.c"
                       "restart.c"
                       INCLUDE_DIRS "." "include"
                       REQUIRES console driver hal nvs_flash)
