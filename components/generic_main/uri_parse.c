#include <stdlib.h>
#include <string.h>
#include "generic_main.h"

int
gm_uri_parse(const char * uri, gm_uri * u)
{
  gm_query_param * p = u->params;

  if ( *uri == '/' )
    uri++;

  gm_uri_decode(uri, u->path, sizeof(u->path));


  char * s = index(uri, '?');
  if ( s ) {
    *s++ = '\0';
    for ( ; ; ) {
      char * value = index(s, '=');
      if ( value ) {
        *value++ = '\0';

        char * ampersand = index(value, '&');
        if ( ampersand )
          *ampersand++ = '\0';

        p->name = s;
        p->value = value;
        p++;

        if ( !ampersand )
          break;
      }
      else
        return -1;
    }
  }
  return 0;
}
