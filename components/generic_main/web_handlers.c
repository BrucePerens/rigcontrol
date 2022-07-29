#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"

static void * gm_web_request;

static gm_web_handler_t * handlers[3] = {};
static gm_web_handler_t * * last[3] = {&handlers[0], &handlers[1], &handlers[2]};

static esp_err_t
run_post_handlers(httpd_req_t * req)
{
  gm_uri uri = {};

  if ( gm_uri_parse(req->uri, &uri) == 0 )
    return gm_web_handler_run(req, &uri, POST) ? ESP_FAIL : ESP_OK;
  else
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t
run_put_handlers(httpd_req_t * req)
{
  gm_uri uri = {};

  gm_uri_parse(req->uri, &uri);

  if ( gm_uri_parse(req->uri, &uri) == 0 )
    return gm_web_handler_run(req, &uri, PUT) ? ESP_FAIL : ESP_OK;
  else
    return ESP_FAIL;
}

void
gm_web_handler_install(httpd_handle_t server)
{
  // The GET method tries to match a file in the compressed ROM filesystem first.
  // If there is no match, it then tries the registered GET methods.
  gm_compressed_fs_web_handlers(server);

  static const httpd_uri_t post = {
      .uri       = "/*",
      .method    = HTTP_POST,
      .handler   = run_post_handlers,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &post);

  static const httpd_uri_t put = {
      .uri       = "/*",
      .method    = HTTP_PUT,
      .handler   = run_put_handlers,
      .user_ctx  = NULL
  };
  httpd_register_uri_handler(server, &put);

}

int
gm_web_handler_run(httpd_req_t * req, const gm_uri * uri, gm_web_method method)
{
  const char * path = req->uri;
  const gm_web_handler_t * h = handlers[method];

  gm_web_set_request(req);

  if ( *path == '/' )
    path++;

  while ( h ) {
    if ( strcmp(h->name, path) == 0 ) {
      return (*(h->handler))(req, uri);
    }
    h = h->next;
  }
  return 1;
}

void
gm_web_handler_register(gm_web_handler_t * handler, gm_web_method method)
{
  *last[method] = handler;
  last[method] = &(handler->next);
}

void
gm_web_set_request(void * context)
{
  gm_web_request = context;
}

void
gm_web_send_to_client (const char *data, size_t size)
{
  httpd_resp_send_chunk((httpd_req_t *)gm_web_request, data, size);
}

void
gm_web_finish(const char *data, size_t size)
{
  httpd_resp_send_chunk((httpd_req_t *)gm_web_request, "", 0);
  gm_web_request = 0;
}
