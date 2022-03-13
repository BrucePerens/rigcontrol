#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <cJSON.h>

const uint8_t x509_crt_bundle[1] asm("_binary_x509_crt_bundle_start");

static struct {
    struct arg_end * end;
} args;

static esp_err_t event_handler(esp_http_client_event_t * event)
{
  int status;
  struct cJSON * json;

  switch (event->event_id) {
  default:
    break;
  case HTTP_EVENT_ON_DATA:
    json = cJSON_Parse(event->data);

    status = esp_http_client_get_status_code(event->client);
    if (status != 200) {
      fprintf(stderr, "Failed: server returned status %d\n", status);
      return ESP_OK;
    }

    if (json) {
      struct cJSON * ip_json = cJSON_GetObjectItemCaseSensitive(json, "ip");
      if (cJSON_IsString(ip_json) && ip_json->valuestring)
        printf("%s\n", ip_json->valuestring);
    }

    break;
  }
  return ESP_OK;
}

static int run(int argc, char * * argv)
{
  esp_http_client_config_t config = {};

  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  config.url = "https://api.ipify.org/?format=json";
  config.event_handler = &event_handler;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
   

  if (client == NULL) {
    printf("Bad argument.\n");
    return -1;
  }

  esp_err_t err = esp_http_client_perform(client);

  if (err) {
    fprintf(stderr, "Web transaction failed.\n");
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return 0;
}

void install_public_ip_command(void)
{

  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "public_ip",
    .help = "Get the public IP used by this device.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
