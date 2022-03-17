#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "generic_main.h"

extern nvs_handle_t nvs;
extern void wifi_restart(void);

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
  { "ssid", STRING, false, "Name of WiFi access point", wifi_restart },
  { "wifi_password", STRING, true, "Password of WiFi access point", wifi_restart },
  { "timezone", STRING, false, "Set time zone (see https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)", 0 },
  { }
};

void
list_nvs_params(list_nvs_params_coroutine_t coroutine)
{
  const struct parameter * p = parameters;
  char buffer[1024];
  size_t buffer_size;

  while (p->type) {
    buffer_size = sizeof(buffer);
    esp_err_t err = nvs_get_str(nvs, p->name, buffer, &buffer_size);
    if (err != ESP_OK) {
      *buffer = '\0';
      (*coroutine)(p->name, buffer, NOT_SET);
    }
    else if (p->secret) {
      *buffer = '\0';
      (*coroutine)(p->name, buffer, SECRET);
    }
    else
      (*coroutine)(p->name, buffer, NORMAL);
    p++;
  }
}

nvs_param_result_t
get_nvs_param(const char * key, char * buffer, size_t buffer_size)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }

  if (!p->type) {
    return NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = nvs_get_str(nvs, key, buffer, &buffer_size);

  if (err) {
    *buffer = '\0';
    return NOT_SET;
  }

  if (p->secret)
    return SECRET;
  else
    return NORMAL;
}

nvs_param_result_t
set_nvs_param(const char * key, const char * value)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    return NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = (nvs_set_str(nvs, key, value));
  if (err) {
    ESP_ERROR_CHECK(err);
    return ERROR;
  }

  if (p->call_after_set)
    (p->call_after_set)();

  return NORMAL;
}

nvs_param_result_t
erase_nvs_param(const char * key)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    return NOT_IN_PARAMETER_TABLE;
  }

  esp_err_t err = (nvs_erase_key(nvs, key));

  switch (err) {
  case ESP_OK:
    if (p->call_after_set)
      (p->call_after_set)();
    return NORMAL;
  case ESP_ERR_NVS_NOT_FOUND:
    return NOT_IN_PARAMETER_TABLE;
  default:
    ESP_ERROR_CHECK(err);
    return ERROR;
  }
}
