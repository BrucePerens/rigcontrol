file(GLOB DOT_C *.c)
idf_component_register(SRCS ${DOT_C}
                    INCLUDE_DIRS "include")
