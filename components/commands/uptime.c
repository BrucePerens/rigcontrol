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

extern void gm_timer_to_human(int64_t t, char * buffer, size_t size);

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  int64_t uptime = esp_timer_get_time();
  char buffer[64];

  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  gm_timer_to_human(uptime, buffer, sizeof(buffer));
  printf("%s\n", buffer);

  return 0;
}

CONSTRUCTOR install(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "uptime",
    .help = "Display how long the system has been running.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
