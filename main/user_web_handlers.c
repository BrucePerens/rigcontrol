#include <string.h>
#include <stdlib.h>
#include <esp_http_server.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "generic_main.h"

static esp_err_t http_root_handler(httpd_req_t *req)
{
  char	buf[1024];
  int	fd;
  int	size;
  const char * uri = req->uri;

  if ( strcmp(uri, "/") == 0 )
    uri = "index.html";
  else
    uri++;

  sprintf(buf, "/crofs/%s", uri);
  uri = buf;
  if ((fd = open (uri, O_RDONLY)) < 0)
  {
    gm_printf("HTTP request for %s not found.\n", uri);
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  while ((size = read(fd, buf, sizeof(buf))) > 0 ) {
    httpd_resp_send_chunk(req, buf, size);
  } 
  httpd_resp_send_chunk(req, buf, 0);

  close(fd);

  if ( size < 0 )
    return ESP_FAIL;
  else
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
