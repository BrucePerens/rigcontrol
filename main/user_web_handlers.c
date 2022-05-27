#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include "generic_main.h"

static esp_err_t http_root_handler(httpd_req_t *req)
{
  gm_printf("HTTP Request for %s\n", req->uri);

  static const char * page = "<html><body>K6BP RigControl</body></html>";
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
}

void user_web_handlers(httpd_handle_t server)
{
  static const httpd_uri_t root = {
      .uri       = "/*",
      .method    = HTTP_GET,
      .handler   = &http_root_handler,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &root);
}
