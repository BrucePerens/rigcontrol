#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_timer.h>
#include <inttypes.h>
#include "generic_main.h"

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{

  return 0;
}

void install_ddns_command(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "ddns",
    .help = "Register the system with the configured dynamic DNS provider.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
