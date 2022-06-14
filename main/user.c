#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <esp_log.h>
#include <esp_vfs.h>
#include <driver/uart.h>
#include "generic_main.h"

void gm_user_initialize_early(void)
{
  // This will be the default WiFi access point name, with the factory MAC address
  // appended to it. Keep it short.
  GM.application_name = "rigcontrol";
  gm_printf("\n*** K6BP RigControl ***\n");
  gm_printf("Version: %s\n", gm_build_version);
}

void gm_user_initialize_late(void)
{
  char	byte;
  errno = 0;
  gm_printf("Reading a byte.\n");
  // int result = uart_read_bytes(0, &byte, 1, 100000);
  int result = read(0, &byte, 1);
  gm_printf("Got %d bytes, %s\n", result, strerror(errno));
}
