#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include "generic_main.h"

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  gm_port_control_protocol(false);
  return 0;
}

CONSTRUCTOR install(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "pcp",
    .help = "Run the port control protocol to get a firewall pinhole.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
