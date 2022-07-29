#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <sys/socket.h>
#include "generic_main.h"

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  close(fd);
  gm_printf("%d\n", fd - 1);
  return 0;
}

CONSTRUCTOR install(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "fds",
    .help = "Display the number of open file descriptors.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
