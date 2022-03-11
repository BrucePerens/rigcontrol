#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <time.h>

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
  { "timezone", STRING, false, "Set time zone (see https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)" },
  { }
};

int list_nvs_params(FILE * f)
{
  static const char not_set[] = "not set";
  static const char secret[] = "(secret)";
  const struct parameter * p = parameters;
  char buffer[128];
  size_t buffer_size;

  while (p->type) {
    buffer_size = sizeof(buffer);
    esp_err_t err = nvs_get_str(nvs, p->name, buffer, &buffer_size);
    if (err != ESP_OK)
      memcpy(buffer, not_set, sizeof(not_set));
    if (p->secret)
      memcpy(buffer, secret, sizeof(secret));
    printf("%-8s\t%-8s\t%s\n", p->name, buffer, p->explanation);
    p++;
  }
  fflush(f);
  return 0;
}

int get_nvs_param(const char * key)
{
  char buffer[128];
  size_t buffer_size;

  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    fprintf(stderr, "%s is not in the parameter table.\n", key);
    return -1;
  }

  buffer_size = sizeof(buffer);
  esp_err_t err = nvs_get_str(nvs, key, buffer, &buffer_size);

  if (err)
    printf("not set\n");
  else {
    if (p->secret)
      printf("%s (secret)", key);
    else
      printf("%s %s", key, buffer);
    printf("\n");
  }

  return 0;
}

int set_nvs_param(const char * key, const char * value)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    fprintf(stderr, "%s is not in the parameter table.\n", key);
    return -1;
  }

  esp_err_t err = (nvs_set_str(nvs, key, value));
  if (err) {
    ESP_ERROR_CHECK(err);
    return -1;
  }
  if (p->call_after_set)
    (p->call_after_set)();
  else {
    if (p->secret)
      printf("%s (secret)", key);
    else
      printf("%s %s", key, value);
    printf("\n");
  }
  return 0;
}

int erase_nvs_param(const char * key)
{
  const struct parameter * p = parameters;
  while (p->type) {
    if (strcmp(p->name, key) == 0)
      break;
    p++;
  }
  if (!p->type) {
    fprintf(stderr, "%s is not in the parameter table.\n", key);
    return -1;
  }

  esp_err_t err = (nvs_erase_key(nvs, key));
  switch (err) {
  case ESP_OK:
    printf("Erased.\n");
    if (p->call_after_set)
      (p->call_after_set)();
    return 0;
  case ESP_ERR_NVS_NOT_FOUND:
    fprintf(stderr, "%s: was not set.\n", key);
    return -1;
  default:
    ESP_ERROR_CHECK(err);
    return -1;
  }
}
