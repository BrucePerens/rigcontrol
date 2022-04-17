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
    struct arg_str * host;
    struct arg_str * port;
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  char	buffer[128];
  struct sockaddr_storage sock;
  int result;

  printf("\n"); 
  args.port->sval[0] = "3478";
  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  result = gm_stun(args.host->sval[0], args.port->sval[0], args.ipv6->count > 0, (struct sockaddr *)&sock);
  if ( result != 0 )
    return 1;

  void * address;
  if ( sock.ss_family == AF_INET )
    address = &((struct sockaddr_in *)&sock)->sin_addr.s_addr;
  else
    address = ((struct sockaddr_in6 *)&sock)->sin6_addr.s6_addr;

  inet_ntop(sock.ss_family, address, buffer, sizeof(buffer));
  fprintf(stderr, "Mapped address: at %x, %s, port: %d\n", (unsigned int)((struct sockaddr_in6 *)&sock)->sin6_addr.s6_addr, buffer, ((struct sockaddr_in6 *)&sock)->sin6_port);
  return 0;
}

CONSTRUCTOR install(void)
{
  args.ipv6 =  arg_lit0("6", NULL, "Use IPv6 (default IPv4)");
  args.host  = arg_str1(NULL, NULL, "host", "STUN server hostname");
  args.port  = arg_str0(NULL, NULL, "port", "port number (default 3478)");
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
