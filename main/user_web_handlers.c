#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "generic_main.h"

// The embedded filesystem.
static const char * fs[] asm("_binary_fs_start");

static esp_err_t
http_root_handler(httpd_req_t *req)
{
  // Redirect to /index.html
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_status(req, "301 Moved Permanently");
  httpd_resp_set_hdr(req, "Location", "/index.html");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

static esp_err_t
http_file_handler(httpd_req_t *req)
{
  printf("%s\n", *fs);
  // httpd_resp_send_chunk(req, page, sizeof(page));
  httpd_resp_send_chunk(req, "", 0);
  return ESP_OK;
}

void user_web_handlers(httpd_handle_t server)
{
  static const httpd_uri_t root = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = http_root_handler,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &root);

  static const httpd_uri_t file = {
      .uri       = "*",
      .method    = HTTP_GET,
      .handler   = http_file_handler,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &file);
}
