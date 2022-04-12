file(GLOB DOT_C *.c)
idf_component_register(
SRCS
  ${DOT_C}
INCLUDE_DIRS
  .
  include
REQUIRES
  console
  driver
  esp_http_client
  esp_timer
  esp-tls
  freertos
  generic_main
  hal
  json
  lwip
  nvs_flash
)

# The self-installing commands in this component do not have any of their symbols
# referred to by any other code, so there will be nothing that pulls them out of the
# component archive at link time. The --whole-archive flag will force linking of all
# of the objects in the archive. And then it's cleared afterwards so that no other
# component gets the same treatement.
idf_build_set_property(LINK_OPTIONS "-Wl,--whole-archive ${CMAKE_CURRENT_BINARY_DIR}/lib${COMPONENT_NAME}.a -Wl,--no-whole-archive" APPEND)
