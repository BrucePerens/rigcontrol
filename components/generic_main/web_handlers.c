#include <stdarg.h>
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

  if ( strcmp(path, "settings") == 0 ) {
    gm_web_get_param(req);
    return 0;
  }
  return 1;
}

static void * gm_web_request;

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

#include "web_template.h"

void
post_button(const char * t, const char * l, ...)
{
  char		buffer[128];
  va_list	argument_list;

  va_start(argument_list, l);
  vsnprintf(buffer, sizeof(buffer), l, argument_list);
  va_end(argument_list);

  form
    attr("action", buffer)
    input
    attr("type", "submit")
    attr("value", t)
  end
}

void
gm_web_get_param(httpd_req_t * req)
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
}
