#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <sys/random.h>
#include "generic_main.h"
#include <sys/socket.h>

// Sites that return your external IP in JSON with { "ip": "address string" }
// and don't have a problem being called by robots, or a fee.
// Try to be fair to them all and fault-tolerant by randomly calling one from
// a list, and retrying another randomly-selected one as necessary.
const char * const urls[] = {
  "https://api.myip.com/",
  "https://api.my-ip.io/ip.json",
  "https://api.ipify.org?format=json",
  "https://www.myexternalip.com/json",
  "https://ip.seeip.org/jsonip?",
};
const size_t number_of_entries = (sizeof(urls) / sizeof(*urls));

static int gm_public_ipv4_internal(const char * url, char * data, size_t size)
{
  char	buffer[1024];
  int	return_value = -1;

  int status = gm_web_get(url, buffer, sizeof(buffer));
  if (status == 200) {
    struct cJSON * json = cJSON_Parse(buffer);
    if (json) {
      struct cJSON * ip_json = cJSON_GetObjectItemCaseSensitive(json, "ip");
      if (cJSON_IsString(ip_json) && ip_json->valuestring) {
        strncpy(data, ip_json->valuestring, size - 1);
        data[size - 1] = '\0';
        return_value = 0;
      }
      cJSON_Delete(json);
    }
  }
  return return_value;
}

int gm_public_ipv4(char * data, size_t size)
{
  for (int tries = 0; tries < (number_of_entries * 2); tries++) {
    if (gm_public_ipv4_internal(urls[gm_choose_one(number_of_entries)], data, size) == 0)
      return 0;
  }
  return -1;
}
