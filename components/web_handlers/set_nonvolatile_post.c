#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
set_nonvolatile_post(httpd_req_t * req, const gm_uri * uri)
{
  char buffer[1024];
  const char * name = "name";
  int size = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

  if ( size >= 0 )
    buffer[size] = '\0';

  boilerplate("Set Non-Volatile %s", name)

  text(buffer);

  end_boilerplate

  return 0;
}

CONSTRUCTOR install(void)
{
  static gm_web_handler_t handler = {
    .name = "set_nonvolatile",
    .handler = set_nonvolatile_post
  };

  gm_web_handler_register(&handler, POST);
}
