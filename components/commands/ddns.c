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
#include <lwip/inet.h>

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  gm_ddns();
  printf("\n");
  return 0;
}

CONSTRUCTOR install(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "ddns",
    .help = "Register the system with the configured dynamic DNS provider.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
