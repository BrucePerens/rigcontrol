#include <stdarg.h>
#include <esp_http_server.h>
#include "generic_main.h"
#include "web_template.h"

static int
function(httpd_req_t * req)
{
  gm_web_set_request(req);

  doctype
  html
    attr("lang", "en")
    head
      title
        text("Settings")
      end
    end
    body
      h1
        text("Settings")
      end

      table
        const gm_parameter_t * v = gm_parameters;
        while ( v->name ) {

          tr
            td
              post_button("Set", "/settings?%s", v->name);
            end
            th
              text(v->name)
            end
            td
            if ( v->secret )
              text("(secret)")
            else
              {
                char	buffer[128];
                size_t	buffer_size = sizeof(buffer);
                esp_err_t err = nvs_get_str(GM.nvs, v->name, buffer, &buffer_size);
                if ( err )
                  text("(not set)")
                else
                  text(buffer)
              }
            end
            td
              text(v->explanation)
            end
          end

          v++;
        }
      end
    end
  end
  return 0;
}


CONSTRUCTOR install(void)
{
  static gm_web_get_handler_t handler = {
    .name = "settings",
    .handler = function
  };

  gm_web_get_handler_register(&handler);
}
