#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
set_nonvolatile_get(httpd_req_t * req, const gm_uri * uri)
{
  const char * name = gm_uri_param(uri, "name");
  const char * value = gm_uri_param(uri, "value");

  if ( !name || !value )
    return -1;


  boilerplate("Set Parameter %s", name)

  form
    attr("method", "post")
    attr("action", "/set_nonvolatile")

    input
    attr("type", "hidden")
    attr("name", "name")
    attr("value", name)

    label
      attr("for", name)
      text(name)
    end

    input
    attr("type", "text")
    attr("name", "value")
    attr("value", value)

    input
    attr("type", "submit")
  end

  end_boilerplate

  return 0;
}

CONSTRUCTOR install(void)
{
  static gm_web_handler_t handler = {
    .name = "set_nonvolatile",
    .handler = set_nonvolatile_get
  };

  gm_web_handler_register(&handler, GET);
}
