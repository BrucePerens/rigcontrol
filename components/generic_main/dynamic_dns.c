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
#include <lwip/inet.h>
#include "generic_main.h"

struct param {
  const char * name;
  const char * param_name;
};

struct ddns_provider {
  const char * name;
  const char * url;
  const char * url_ipv6; // Only set if the above "url" doesn't work for IPV4 and IPV6.
};

static const struct param params[] = {
  { "hostname", "ddns_hostname" },
  { "password", "ddns_password" },
  { "provider", "ddns_provider" },
  { "token", "ddns_token" },
  { "username", "ddns_username" },
  { 0, 0 }
};


static const struct ddns_provider ddns_providers[] = {
  { "he.net",
    "https://{hostname}:{password}@dyn.dns.he.net/nic/update?hostname={hostname}&myip={ipv4}",
    "https://{hostname}:{password}@dyn.dns.he.net/nic/update?hostname={hostname}&myip={ipv6}"
  },
  { 0, 0, 0 }
};

// Get a variable to substitute into the pattern string.
static int parameter(const char * name,  char * buffer, size_t buffer_size)
{
  if ( strcmp(name, "ipv4") == 0 ) {
    esp_ip4addr_ntoa(&GM.sta.public_ip4, buffer, buffer_size);
    return 0;
  }
  else if ( strcmp(name, "ipv6") == 0 ) {
    snprintf(buffer, buffer_size, IPV6STR, IPV62STR(GM.sta.public_ip6[0].ip));
    return 0;
  }
  else {
    const struct param * p = params;

    while ( p->name ) {
      if ( strcmp(name, p->name) == 0 ) {
        gm_param_result_t result;

        result = gm_param_get(p->param_name, buffer, buffer_size);
        if ( result == PR_NORMAL || result == PR_SECRET )
          return 0;
        else {
          gm_printf("Warning: DDNS parameter %s required and not set.\n", p->param_name);
          return -1;
        }
      }
      p++;
    }
  }
  gm_printf("No such DDNS parameter %s.\n", name);
  return 0;
}

static int
send_ddns(const char * url)
{
  char request[256];
  char response[256];

  gm_pattern_string(url, parameter, request, sizeof(request));
  int status = gm_web_get(request, response, sizeof(response));
  if (status == 200) {
    gm_printf("\nDynamic DNS succeeded: %s.\n", response);
    fflush(stderr);
    return 0;
  }
  else {
    gm_printf("Dynamic DNS failed: %d %s\n", status, response);
    return -1;
  }
}

int gm_ddns(void)
{
  char ddns_provider[64];
  const struct ddns_provider * p = ddns_providers;
  gm_param_result_t result;
  static bool i_told_you_once = false;

  result = gm_param_get("ddns_provider", ddns_provider, sizeof(ddns_provider));
  
  switch ( result ) {
  case PR_NOT_SET:
    if (!i_told_you_once) {
      gm_printf("Warning: DDNS provider not set.\n");
      i_told_you_once = true;
    }
    return -1;
  case PR_NORMAL:
  case PR_SECRET:
    break;
  default:
    gm_printf("Error reading ddns_provider parameter.\n");
    return -1;
  }


  while ( p->name ) {
    if ( strcmp(p->name, ddns_provider) == 0 ) {
      send_ddns(p->url);
      if (p->url_ipv6 && !gm_all_zeroes(&GM.sta.public_ip6[0].ip, sizeof(GM.sta.public_ip6[0].ip))) {
        send_ddns(p->url_ipv6);
      }
      return 0;
    }
    p++;
  }
  gm_printf("DDNS provider %s is not implemented.\n", ddns_provider);
  return -1;
}
