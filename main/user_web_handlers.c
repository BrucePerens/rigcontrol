#include "generic_main.h"

// This is called to register any user web handlers.
// Don't register handlers for the GET method, use user_web_get below.
void
gm_user_web_handlers(httpd_handle_t server)
{
}

// This is called for a GET method when the compressed filesystem handler
// does not find a file. The user can serve any number of files from here.
// If req->url doesn't match anything you serve, call httpd_resp_send_404(req).
int
gm_user_web_get(httpd_req_t * req)
{
  // Return 0 if the request was fulfilled, 1 for a 404 not found response.
  return 1;
}
