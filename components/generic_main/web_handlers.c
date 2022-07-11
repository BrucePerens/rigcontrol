#include <esp_http_server.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "generic_main.h"

void gm_web_get_param(httpd_req_t * req);

int
gm_internal_web_get(httpd_req_t * req)
{
  const char * path = req->uri;

  if ( *path == '/' )
    path++;

  gm_printf("Request for %s\n", path);
  if ( strcmp(path, "settings") == 0 ) {
    gm_printf("Running settings\n");
    gm_web_get_param(req);
    return 0;
  }
  return 1;
}

static void * gm_web_client_context;

void
gm_web_set_client_context(void * context)
{
  gm_web_client_context = context;
}

void
gm_web_send_to_client (const char *data, size_t size)
{
  httpd_resp_send_chunk((httpd_req_t *)gm_web_client_context, data, size);
}

void
gm_web_finish(const char *data, size_t size)
{
  httpd_resp_send_chunk((httpd_req_t *)gm_web_client_context, "", 0);
  gm_web_client_context = 0;
}

#include "web_template.h"

void
post_button(const char * t, const char * l)
{
  form
    attr("action", l)
    input
    attr("type", "submit")
    attr("value", t)
  end
}

void
gm_web_get_param(httpd_req_t * req)
{
  gm_web_set_client_context(req);

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
              post_button("Set", "/settings");
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

  gm_printf("End of settings\n");
}
