file(GLOB DOT_C *.c)
file(GLOB DOT_CXX *.cxx)
idf_component_register(
SRCS
  ${DOT_C}
  ${DOT_CXX}
  ${CMAKE_CURRENT_BINARY_DIR}/git_version.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/magic.c 
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
  miniz
  nvs_flash
  wpa_supplicant
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs_gen
  COMMAND cc -g -I ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_gen.c -lz ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/magic.c -o ${CMAKE_CURRENT_BINARY_DIR}/fs_gen
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_gen.c
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/compressed_fs.h
)
add_custom_target(fs-gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs_gen)
add_dependencies(${COMPONENT_LIB} fs-gen)

# FS-read depends on a host implementation of miniz, and is useful for testing.
#
# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs_read
#   COMMAND cc -g -I ${CMAKE_CURRENT_SOURCE_DIR}/include -I ${CMAKE_CURRENT_SOURCE_DIR}/../miniz/include ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_read.c ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/magic.c -lminiz -o ${CMAKE_CURRENT_BINARY_DIR}/fs_read
#   MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/filesystem_generator/fs_read.c
#   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/compressed_fs.h
# )
# add_custom_target(fs-read DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fs_read)
# add_dependencies(${COMPONENT_LIB} fs-read)

file(GLOB_RECURSE FILESYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../../filesystem/*)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fs
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/fs_gen ${CMAKE_CURRENT_SOURCE_DIR}/../../filesystem ${CMAKE_CURRENT_BINARY_DIR}/fs
  MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/fs_gen
  DEPENDS ${FILESYSTEM}
)
target_add_binary_data(${COMPONENT_LIB} ${CMAKE_CURRENT_BINARY_DIR}/fs BINARY)

add_custom_target(
  git-version
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/generate_git_version.sh ${CMAKE_CURRENT_BINARY_DIR}/git_version.c
)

set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/git_version.c 
    PROPERTIES GENERATED TRUE
)
add_dependencies(${COMPONENT_LIB} git-version)
