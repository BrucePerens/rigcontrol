idf_component_register(SRCS "commands.c"
                       "param.c"
                       "param_cmd.c"
                       INCLUDE_DIRS "." "include"
                       REQUIRES console nvs_flash)
