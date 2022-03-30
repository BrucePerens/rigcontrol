#include <stdio.h>
#include <stdint.h>
#include <esp_log.h>
#include "generic_main.h"

void user_startup(void)
{
  // This will be the default WiFi access point name, with the factory MAC address
  // appended to it. Keep it short.
  GM.application_name = "rigcontrol";
  gm_printf("\n*** K6BP RigControl ***\n");
}
