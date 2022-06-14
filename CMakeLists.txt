# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(k6bp_rigcontrol)

# To build a firmware archive for the web installer, use:
# idf.py build firmware
add_custom_command(
  OUTPUT firmware.tar.gz
  COMMAND tar cvlzf firmware.tar.gz bootloader/bootloader.bin partition_table/partition-table.bin k6bp_rigcontrol.bin
  DEPENDS bootloader/bootloader.bin partition_table/partition-table.bin k6bp_rigcontrol.bin
)
add_custom_target(firmware DEPENDS firmware.tar.gz)
