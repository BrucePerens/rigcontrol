#include <stdlib.h>
#include <string.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include "generic_main.h"

struct user_data {
  char * data;
  size_t size;
  size_t index;
  web_get_coroutine_t coroutine;
};

static esp_err_t event_handler(esp_http_client_event_t * event)
{
  struct user_data * const user_data = (struct user_data *)event->user_data;


  switch (event->event_id) {
  default:
    break;
  case HTTP_EVENT_ON_DATA:

    if (user_data->coroutine) {
      (*(user_data->coroutine))(event->data, event->data_len);
    }
    else {
      size_t size = user_data->size - user_data->index - 1;
      if (event->data_len < size)
        size = event->data_len;
      if (size > 0) {
        memcpy(&(user_data->data[user_data->index]), event->data, size);
        user_data->index += size;
        user_data->data[user_data->index] = '\0';
      }
    }
    break;
  }
  return ESP_OK;
}

static int web_get_internal(const char * url, struct user_data * user_data)
{
  esp_http_client_config_t config = {};



  config.url = url;
  config.event_handler = &event_handler;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.user_data = user_data;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (client == NULL)
    return -1;

  esp_err_t err = esp_http_client_perform(client);

  if (err) {
    return -1;
  }

  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return status;
}

int
web_get(const char *url, char *data, size_t size)
{
  struct user_data user_data = {};
  user_data.data = data;
  user_data.size = size;
  return web_get_internal(url, &user_data);
}

int
web_get_with_coroutine(const char *url, web_get_coroutine_t coroutine)
{
  struct user_data user_data = {};
  user_data.coroutine = coroutine;
  return web_get_internal(url, &user_data);
}
