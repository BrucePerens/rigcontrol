#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
set_parameter(httpd_req_t * req)
{
  const char * name = "parameter_name";
  const char * value = "value";

  boilerplate("Set Parameter %s", name)

  form
    attr("method", "post")
    attr("action", "/set_parameter")

    label
      attr("for", name)
      text(name)
    end

    input

    input
    attr("name", name)
    attr("type", "text")
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
    .name = "set_parameter",
    .handler = set_parameter
  };

  gm_web_handler_register(&handler, GET);
}
