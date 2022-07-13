#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static void
setting_row(const char * name, const char * value, const char * explanation, gm_param_result_t type)
{
  const char * v;

  switch ( type ) {
  case GM_NORMAL:
    v = value;
    break;
  case GM_SECRET:
    v = "(secret)";
    break;
  case GM_NOT_SET:
    v = "(not set)";
    break;
  default:
    v = "(error)";
    break;
  }

  tr
    td
      get_button("Set", "/set_parameter?%s", name);
    end
    th
      text(name)
    end
    td
      text(v)
    end
    td
      text(explanation)
    end
  end
}

static int
settings(httpd_req_t * req)
{
  gm_web_set_request(req);

  boilerplate("Settings");

  table
    gm_param_list(setting_row);
  end
  end_boilerplate

  return 0;
}


CONSTRUCTOR install(void)
{
  static gm_web_get_handler_t handler = {
    .name = "settings",
    .handler = settings
  };

  gm_web_get_handler_register(&handler);
}
