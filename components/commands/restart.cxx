#include <stdio.h>
#include <stdlib.h>
#include <esp_console.h>
#include <esp_system.h>
#include "generic_main.h"

static int restart(int argc, char * * argv)
{
  esp_restart();
}

CONSTRUCTOR void install_restart_command(void)
{
  static const esp_console_cmd_t param_cmd = {
    .command = "restart",
    .help = "Restart this program.",
    .hint = NULL,
    .func = &restart,
    .argtable = NULL
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&param_cmd) );
}
