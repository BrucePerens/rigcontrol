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

struct param {
  const char * name;
  const char * param_name;
};

struct ddns_provider {
  const char * name;
  const char * url;
  const char * url_ipv6; // Only set if the above "url" doesn't work for IPV6.
};

static struct {
    struct arg_end * end;
} args;

static const struct param params[] = {
  { "hostname", "ddns_hostname" },
  { "password", "ddns_password" },
  { "provider", "ddns_provider" },
  { "token", "ddns_token" },
  { "username", "ddns_username" },
  { 0, 0 }
};


static const struct ddns_provider ddns_providers[] = {
  { "he.net", "{hostname} {password} {token} {username} {ipv4} {ipv6}", 0 },
  { 0, 0, 0 }
};

// Perform variable substitution on a string.
// extern int pattern_string(const char * string, pattern_coroutine_t coroutine, char * buffer, size_t buffer_size);

// Get a variable to substitute into the pattern string.
static int parameter(const char * name,  char * buffer, size_t buffer_size)
{
  if ( strcmp(name, "ipv4") == 0 ) {
    memcpy(buffer, "IPV4", 5);
    return 0;
  }
  else if ( strcmp(name, "ipv6") == 0 ) {
    memcpy(buffer, "IPV6", 5);
    return 0;
  }
  else {
    const struct param * p = params;

    while ( p->name ) {
      if ( strcmp(name, p->name) == 0 ) {
        enum param_result result;

        result = param_get(p->param_name, buffer, buffer_size);
        if ( result == PR_NORMAL || result == PR_SECRET )
          return 0;
        else {
          fprintf(stderr, "DDNS parameter %s required and not set.\n", p->param_name);
          return -1;
        }
      }
      p++;
    }
  }
  fprintf(stderr, "No such DDNS parameter %s.\n", name);
  return 0;
}

static int
send_ddns(const char * url)
{
  char request[256];

  pattern_string(url, parameter, request, sizeof(request));
  printf("%s\n", request);

  return 0;
}

static int ddns()
{
  char ddns_provider[64];
  const struct ddns_provider * p = ddns_providers;
  enum param_result result;

  result = param_get("ddns_provider", ddns_provider, sizeof(ddns_provider));
  
  switch ( result ) {
  case PR_NOT_SET:
    fprintf(stderr, "DDNS provider not set.\n");
    return -1;
  case PR_NORMAL:
  case PR_SECRET:
    break;
  default:
    fprintf(stderr, "Error reading ddns_provider parameter.\n");
    return -1;
  }


  while ( p->name ) {
    if ( strcmp(p->name, ddns_provider) == 0 ) {
      send_ddns(p->url);
      if ( p->url_ipv6 )
        send_ddns(p->url_ipv6);
      return 0;
    }
    p++;
  }
  fprintf(stderr, "DDNS provider %s is not implemented.\n", ddns_provider);
  return -1;
}

static int run(int argc, char * * argv)
{
  ddns();
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

  command_register(&command);
}
