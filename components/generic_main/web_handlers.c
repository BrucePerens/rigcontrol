#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"

static void * gm_web_request;
static gm_web_get_handler_t * handlers = 0;
static gm_web_get_handler_t * * last = &handlers;

int
gm_web_get_run_handlers(httpd_req_t * req)
{
  const char * path = req->uri;
  const gm_web_get_handler_t * h = handlers;

  if ( *path == '/' )
    path++;

  gm_printf("Request for %s\n");

  while ( h ) {
    gm_printf("Checking %s\n", path);
    if ( strcmp(h->name, path) == 0 ) {
      gm_printf("Running the handler.\n");
      return (*(h->handler))(req);
    }
    h = h->next;
  }
  gm_printf("Returning 1 from gm_web_get_run_handlers\n");
  return 1;
}

void
gm_web_get_handler_register(gm_web_get_handler_t * handler)
{
  printf("Registering handler for %s\n", handler->name);
  *last = handler;
  last = &(handler->next);
}

void
gm_web_set_request(void * context)
{
  gm_web_request = context;
}

void
gm_web_send_to_client (const char *data, size_t size)
{
  gm_printf("Sending %d bytes to the client.\n", size);
  httpd_resp_send_chunk((httpd_req_t *)gm_web_request, data, size);
}

void
gm_web_finish(const char *data, size_t size)
{
  gm_printf("Running gm_web_finish\n");
  httpd_resp_send_chunk((httpd_req_t *)gm_web_request, "", 0);
  gm_web_request = 0;
}
