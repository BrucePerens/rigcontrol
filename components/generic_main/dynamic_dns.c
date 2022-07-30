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
#include <arpa/inet.h>
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
    inet_ntop(AF_INET, &GM.sta.ip4.pub.sin_addr.s_addr, buffer, buffer_size);
    return 0;
  }
  else if ( strcmp(name, "ipv6") == 0 ) {
    inet_ntop(AF_INET6, &GM.sta.ip6.pub.sin6_addr.s6_addr, buffer, buffer_size);
    return 0;
  }
  else {
    const struct param * p = params;

    while ( p->name ) {
      if ( strcmp(name, p->name) == 0 ) {
        gm_nonvolatile_result_t result;

        result = gm_nonvolatile_get(p->param_name, buffer, buffer_size);
        if ( result == GM_NORMAL || result == GM_SECRET )
          return 0;
        else {
          GM_WARN_ONCE("Warning: Dynamic DNS parameter %s is required, and is not set.\n", p->param_name);
          return -1;
        }
      }
      p++;
    }
  }
  GM_FAIL("No such DDNS parameter %s.\n", name);
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
    ; // gm_printf("\nDynamic DNS succeeded: %s.\n", response);
    fflush(stderr);
    return 0;
  }
  else {
    ; // gm_printf("Dynamic DNS failed: %d %s\n", status, response);
    return -1;
  }
}

int gm_ddns(void)
{
  char ddns_provider[64];
  const struct ddns_provider * p = ddns_providers;
  gm_nonvolatile_result_t result;
  static bool i_told_you_once = false;

  result = gm_nonvolatile_get("ddns_provider", ddns_provider, sizeof(ddns_provider));
  
  switch ( result ) {
  case GM_NOT_SET:
    if (!i_told_you_once) {
      GM_WARN_ONCE("Warning: Dynamic DNS provider not set.\n");
      i_told_you_once = true;
    }
    return -1;
  case GM_NORMAL:
  case GM_SECRET:
    break;
  default:
    GM_WARN_ONCE("Error reading ddns_provider parameter.\n");
    return -1;
  }


  while ( p->name ) {
    if ( strcmp(p->name, ddns_provider) == 0 ) {
      send_ddns(p->url);
      if (p->url_ipv6 && !gm_all_zeroes(&GM.sta.ip6.pub.sin6_addr.s6_addr, sizeof(GM.sta.ip6.pub.sin6_addr.s6_addr))) {
        send_ddns(p->url_ipv6);
      }
      return 0;
    }
    p++;
  }
  GM_WARN_ONCE("Dynamic DNS provider %s is not implemented.\n", ddns_provider);
  return -1;
}
