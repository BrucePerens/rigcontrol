#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "generic_main.h"

static struct {
    struct arg_lit * ipv6;
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  static struct sockaddr_storage sock;
  int result;

  printf("\n"); 
  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  result = gm_stun(args.ipv6->count > 0, (struct sockaddr *)&sock);
  if ( result != 0 )
    return 1;

  return 0;
}

CONSTRUCTOR install(void)
{
  args.ipv6 =  arg_lit0("6", NULL, "Use IPv6 (default IPv4)");
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "stun",
    .help = "Run the STUN protocol to get the external IP address.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
