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

int run(int argc, char * * argv)
{
  char	data[128];
  if ( public_ip(data, sizeof(data)) == 0 ) {
    printf("%s\n", data);
    return 0;
  }
  return -1;
}

CONSTRUCTOR void install_public_ip_command(void)
{

  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "public_ip",
    .help = "Get the public IP used by this device.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
