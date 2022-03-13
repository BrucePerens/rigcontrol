#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_http_client.h>

static struct {
    struct arg_str * url;
    struct arg_end * end;
} args;

static esp_err_t event_handler(esp_http_client_event_t * event)
{
  int status;

  switch (event->event_id) {
  // This event occurs when there are any errors during execution
  case HTTP_EVENT_ERROR:
    break;
  
  // Once the HTTP has been connected to the server, no data exchange has been performed
  case HTTP_EVENT_ON_CONNECTED:
    break;
  
  // After sending all the headers to the server
  case HTTP_EVENT_HEADERS_SENT:
    break;
  
  // Occurs when receiving each header sent from the server
  case HTTP_EVENT_ON_HEADER:
    break;
  
  // Occurs when receiving data from the server, possibly multiple portions of the packet
  case HTTP_EVENT_ON_DATA:
    printf("%s", (const char *)event->data);
    break;
  
  // Occurs when finish a HTTP session
  case HTTP_EVENT_ON_FINISH:
    status = esp_http_client_get_status_code(event->client);
    printf("\nStatus code: %d\n", status);
    break;
  
  // The connection has been disconnected
  case HTTP_EVENT_DISCONNECTED:
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

  config.url = args.url->sval[0];
  config.event_handler = &event_handler;
  config.use_global_ca_store = true;

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

void install_wget_command(void)
{

  args.url  = arg_str1(NULL, NULL, "url", "URL to retrieve.");
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "wget",
    .help = "Retrieve a web page and display the raw data on the console.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
