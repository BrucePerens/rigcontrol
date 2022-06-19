#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "generic_main.h"

static const char	page[] = " \
<http> \
<head> \
<title> \
K6BP Rigcontrol \
</title> \
</head> \
<body> \
<h1>K6BP Rigcontrol</h1> \
See <a href=\"https://github.com/BrucePerens/rigcontrol\"> \
https://github.com/BrucePerens/rigcontrol \
</a> \
</body> \
</http> \
";

static esp_err_t
http_root_handler(httpd_req_t *req)
{
  
  httpd_resp_send_chunk(req, page, sizeof(page));
  httpd_resp_send_chunk(req, page, 0);
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
}
