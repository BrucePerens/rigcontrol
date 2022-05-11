// Web Server
//
// Operate an HTTP and HTTPS web server. Maintain the SSL server certificates.
//
#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <inttypes.h>
#include "generic_main.h"

extern void user_web_handlers(httpd_handle_t);

// extern const uint8_t servercert_pem[] asm("_binary_servercert_pem_start");
static const char TASK_NAME[] = "web_server";
static httpd_handle_t server = NULL;

static void time_was_synchronized(struct timeval * t)
{
  // The first time the clock is adjusted, it's changed immediately from the epoch
  // to the current time. This sets the SNTP code so that the second and subsequent
  // times, it is adjusted smoothly.
  if (GM.time_last_synchronized == 0) {
    gm_printf("Time was synchronized.\n");
    fflush(stderr);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_restart();
  }
  GM.time_last_synchronized = esp_timer_get_time();
}

void start_webserver(void)
{
  if (server)
    return;

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_set_time_sync_notification_cb(time_was_synchronized);
  // Adjust the clock suddenly the first time. The time_was_synchronized
  // callback will set it to be adjusted smoothly the second and subsequent
  // time.
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  // Re-adjust the clock every 10 minutes.
  sntp_set_sync_interval(60 * 10 * 1000);

  sntp_init();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  // Start the httpd server
  ESP_LOGI(TASK_NAME, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    user_web_handlers(server);
  }
  else {
    server = NULL;
    ESP_LOGI(TASK_NAME, "Error starting server!");
  }
}

void stop_webserver()
{
  if (server) {
    sntp_stop();
    GM.time_last_synchronized = 0;
    httpd_stop(server);
    server = NULL;
  }
}
