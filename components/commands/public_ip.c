#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include "web_get.h"
#include "public_ip.h"

/* Sites that return your external IP in JSON with { "ip": "address string" } */
const char * urls[] = {
  "https://api.myip.com/",
  "https://api.my-ip.io/ip.json",
  "https://api.ipify.org?format=json",
  "https://www.myexternalip.com/json",
  "https://ip.seeip.org/jsonip?",
  0
};

int public_ip(char * data, size_t size)
{
  char	buffer[1024];

  int status = web_get("https://api.ipify.org?format=json", buffer, sizeof(buffer));
  if (status == 200) {
    struct cJSON * json = cJSON_Parse(data);
    if (json) {
      struct cJSON * ip_json = cJSON_GetObjectItemCaseSensitive(json, "ip");
      if (cJSON_IsString(ip_json) && ip_json->valuestring) {
        strncpy(data, ip_json->valuestring, size - 1);
        data[size - 1] = '\0';
        return 0;
      }
    }
  }
  return -1;
}
