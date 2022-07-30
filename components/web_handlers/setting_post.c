#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
setting_post(httpd_req_t * req, const gm_uri * uri)
{
  char		buffer[1024];
  gm_param_t	params[2] = {};
  gm_nonvolatile_result_t result;

  int size = httpd_req_recv(req, buffer, sizeof(buffer) - 1);

  if ( size >= 0 )
    buffer[size] = '\0';

  gm_param_parse(buffer, params, COUNTOF(params));

  const char * name = gm_param(params, COUNTOF(params), "name");
  const char * value = gm_param(params, COUNTOF(params), "value");
  
  if ( !name || !value )
    return -1;

  boilerplate("Setting %s", name)

  result = gm_nonvolatile_set(name, value);

  switch ( result ) {
  case GM_NOT_SET:
  case GM_ERROR:
    text("Non-volatile memory error, not set.");
    break;
  case GM_NOT_IN_PARAMETER_TABLE:
    text("%s is not in the non-volatile parameter table.", name);
    break;
  case GM_NORMAL:
    text("The value was set: %s=%s\n", name, value);
    break;
  case GM_SECRET:
    text("The value was set.");
    break;
  }

  ul
    li
      a
      attr("href", "/");
        text("Front page.");
      end
    end
    li
      a
      attr("href", "/settings");
        text("Settings.");
      end
    end
  end

  end_boilerplate

  return 0;
}

CONSTRUCTOR install(void)
{
  static gm_web_handler_t handler = {
    .name = "setting",
    .handler = setting_post
  };

  gm_web_handler_register(&handler, POST);
}
