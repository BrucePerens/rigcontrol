idf_component_register(SRCS "main.c"
                       "webserver.c"
                       "wifi.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/servercert.pem"
                                   "certs/prvtkey.pem")
