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
  esp_log_level_set("*", ESP_LOG_ERROR);
  // This will be the default WiFi access point name, with the factory MAC address
  // appended to it. Keep it short.
  GM.application_name = "rigcontrol";
  gm_printf("\nK6BP RigControl, Version: %s\n", GM.build_version);
}

void gm_user_initialize_late(void)
{
}
