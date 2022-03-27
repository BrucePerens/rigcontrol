#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "generic_main.h"

extern void gm_wifi_restart(void);

enum parameter_type {
  END = 0,
  STRING,
  INT,
  FLOAT,
  URL,
  DOMAIN
};

struct parameter {
  const char * name;
  enum parameter_type type;
  bool secret;
  const char * explanation;
  void (*call_after_set)(void);
};

static const struct parameter parameters[] = {
  { "ddns_basic_auth", STRING, false, "send HTTP basic authentication on the first transaction with the Dynamic DNS server.\n", 0},
  { "ddns_hostname", STRING, false, "readable hostname to set in dynamic DNS.", 0 },
  { "ddns_password", STRING, true, "password for secure access to the dynamic DNS host.", 0 },
  { "ddns_provider", STRING, false, "name of the Dynamic DNS provider.", 0 },
  { "ddns_token", STRING, true, "secret token to set in dynamic DNS.", 0 },
  { "ddns_username", STRING, false, "user name for secure access to the dynamic DNS host.", 0 },
  { "ssid", STRING, false, "Name of WiFi access point", gm_wifi_restart },
  { "timezone", STRING, false, "Set time zone (see https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)", 0 },
  { "wifi_password", STRING, true, "Password of WiFi access point", gm_wifi_restart },
  { }
};

void
gm_param_list(gm_param_list_coroutine_t coroutine)
{
  const struct parameter * p = parameters;
  char buffer[1024];
  size_t buffer_size;

  while (p->type) {
    buffer_size = sizeof(buffer);
    esp_err_t err = nvs_get_str(GM.nvs, p->name, buffer, &buffer_size);
    if (err != ESP_OK) {
      *buffer = '\0';
      (*coroutine)(p->name, buffer, p->explanation, PR_NOT_SET);
    }
    else if (p->secret) {
      *buffer = '\0';
      (*coroutine)(p->name, buffer, p->explanation, PR_SECRET);
    }
    else
      (*coroutine)(p->name, buffer, p->explanation, PR_NORMAL);
    p++;
  }
}

gm_param_result_t
gm_param_get(const char * key, char * buffer, size_t buffer_size)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }

  if (!p->type) {
    return PR_NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = nvs_get_str(GM.nvs, key, buffer, &buffer_size);

  if (err) {
    *buffer = '\0';
    return PR_NOT_SET;
  }

  if (p->secret)
    return PR_SECRET;
  else
    return PR_NORMAL;
}

gm_param_result_t
gm_param_set(const char * key, const char * value)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    return PR_NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = (nvs_set_str(GM.nvs, key, value));
  if (err) {
    ESP_ERROR_CHECK(err);
    return PR_ERROR;
  }

  if (p->call_after_set)
    (p->call_after_set)();

  return PR_NORMAL;
}

gm_param_result_t
gm_param_erase(const char * key)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    return PR_NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = (nvs_erase_key(GM.nvs, key));

  switch (err) {
  case ESP_OK:
    if (p->call_after_set)
      (p->call_after_set)();
    return PR_NORMAL;
  case ESP_ERR_NVS_NOT_FOUND:
    return PR_NOT_IN_PARAMETER_TABLE;
  default:
    ESP_ERROR_CHECK(err);
    return PR_ERROR;
  }
}
