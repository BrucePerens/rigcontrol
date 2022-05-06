#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include "generic_main.h"

static struct {
    struct arg_lit * ipv6;
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  char	buffer[64];
  gm_port_mapping_t m = {};

  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }
  printf("\n"); 
  esp_fill_random(&m.nonce, sizeof(m.nonce));
  m.ipv6 = args.ipv6->count > 0;
  if ( m.ipv6 ) {
    memcpy(m.external_address.s6_addr, GM.sta.ip6.public.sin6_addr.s6_addr, sizeof(m.external_address.s6_addr));
  }
  m.tcp = true;
  m.internal_port = m.external_port = 8080;
  m.lifetime = 24 * 60 * 60;
  if ( gm_port_control_protocol(&m) == 0 ) {
    inet_ntop(AF_INET6, m.external_address.s6_addr, buffer, sizeof(buffer));
    return 0;
  }
  return -1;
}

CONSTRUCTOR install(void)
{
  args.ipv6 =  arg_lit0("6", NULL, "Use IPv6 (default IPv4)");
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
