#include <esp_http_server.h>
#include "generic_main.h"

int gm_web_get_param(httpd_req_t * req);

int
gm_internal_web_get(httpd_req_t * req)
{
  if ( strcmp(req->uri, "gm/param") == 0 )
    return gm_web_get_param(req);
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
int
gm_web_get_param(httpd_req_t * req)
{
  gm_web_set_client_context(req);
  doctype;
  html
    attr("lang", "en");
    head
      title
        text("Web Parameters")
      end
    end
    body
    end
  end
  return 0;
}
