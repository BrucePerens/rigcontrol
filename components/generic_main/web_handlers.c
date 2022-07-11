#include "generic_main.h"

int gm_web_get_param(httpd_req_t * req);

int
gm_internal_web_get(httpd_req_t * req)
{
  if ( strcmp(req->uri, "gm/param") == 0 )
    return gm_web_get_param(req);
  return 1;
}

#include "web_template.h"
int
gm_web_get_param(httpd_req_t * req)
{
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
