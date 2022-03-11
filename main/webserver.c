// Web Server
//
// Operate an HTTP and HTTPS web server. Maintain the SSL server certificates.
//
#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <esp_log.h>

// extern const uint8_t servercert_pem[] asm("_binary_servercert_pem_start");
static const char TASK_NAME[] = "web_server";
static httpd_handle_t server = NULL;

static esp_err_t http_root_handler(httpd_req_t *req)
{
  static const char * page = "<html><body>K6BP RigControl</body></html>";
    
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
}

void start_webserver(void)
{
  if (server)
    return;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  static const httpd_uri_t root = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = http_root_handler,
      .user_ctx  = NULL
  };
  // Start the httpd server
  ESP_LOGI(TASK_NAME, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &root);
  }
  else {
    server = NULL;
    ESP_LOGI(TASK_NAME, "Error starting server!");
  }
}

void stop_webserver()
{
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
}
