#include <stdlib.h>
#include <string.h>
#include "generic_main.h"

const char *
gm_uri_param(const gm_uri * u, const char * name)
{
  for ( int i = 0; i < sizeof(u->params) / sizeof(*u->params); i++ ) {
    if ( u->params[i].name && strcmp(name, u->params[i].name) == 0 ) {
      return u->params[i].value;
    }
  }
  return 0;
}
