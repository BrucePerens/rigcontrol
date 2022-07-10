/*
#include <argtable3/argtable3.h>
#include "generic_main.h"

static struct {
    struct arg_end * end;
} args;

// Implements the user's command.
static int run(int argc, char * * argv)
{
  return 0;
}

CONSTRUCTOR install()
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "mycommand",
    .help = "The user's command to do something special.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
 */
