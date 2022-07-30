#include <stdlib.h>
#include <string.h>
#include "generic_main.h"

int
gm_uri_parse(const char * uri, gm_uri * u)
{
  if ( *uri == '/' )
    uri++;

  gm_uri_decode(uri, u->path, sizeof(u->path));

  char * s = index(uri, '?');
  if ( s ) {
    *s++ = '\0';
    return gm_param_parse(s, u->params, COUNTOF(u->params));
  }
  else
    return 0;
}
