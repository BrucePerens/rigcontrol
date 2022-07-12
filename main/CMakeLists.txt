file(GLOB DOT_C *.c)
idf_component_register(
SRCS
  ${DOT_C}
INCLUDE_DIRS
  .
# Components must be listed before any other components that they depend upon.
REQUIRES
  commands
  console
  driver
  esp_http_server
  esp_timer
  freertos
  generic_main
  hal
  lwip
  nvs_flash
  tcpip_adapter
  web_handlers
)
