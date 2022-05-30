file(GLOB DOT_C *.c)
idf_component_register(
SRCS
  ${DOT_C}
INCLUDE_DIRS
  "."
  "include"
PRIV_REQUIRES
  commands
REQUIRES
  bootloader_support
  console
  driver
  esp_http_server
  esp_hw_support
  esp_netif
  esp_timer
  esp_wifi
  freertos
  hal
  lwip
  main
  nvs_flash
  wpa_supplicant
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs_gen
  COMMAND cc -I ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_gen.c -lz -o ${CMAKE_CURRENT_BINARY_DIR}/fs_gen
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_gen.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/compressed_fs.h
)
add_custom_target(fs-gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs_gen)
add_dependencies(${COMPONENT_LIB} fs-gen)
