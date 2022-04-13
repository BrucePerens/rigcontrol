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
  gm_port_mapping_t * m = malloc(sizeof(*m));
  memset(m, '\0', sizeof(*m));
  printf("\n"); 
  esp_fill_random(&m->nonce, sizeof(m->nonce));
  m->ipv6 = false;
  m->tcp = true;
  m->internal_port = m->external_port = 8080;
  m->lifetime = 24 * 60 * 60;
  if ( gm_port_control_protocol(m) == 0 ) {
    fprintf(stderr, "External address: %s, port: %d\n", inet_ntoa(m->external_address), m->external_port);
    return 0;
  }
  else {
    free(m);
    return -1;
  }
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
