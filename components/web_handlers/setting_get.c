#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
setting_get(httpd_req_t * req, const gm_uri * uri)
{
  const char * name = gm_param(uri->params, COUNTOF(uri->params), "name");
  const char * value = gm_param(uri->params, COUNTOF(uri->params), "value");

  if ( !name || !value )
    return -1;


  boilerplate("Setting %s", name)

  form
    attr("method", "post")
    attr("action", "/setting")

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
    .name = "setting",
    .handler = setting_get
  };

  gm_web_handler_register(&handler, GET);
}
