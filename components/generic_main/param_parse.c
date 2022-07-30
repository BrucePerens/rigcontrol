#include <stdlib.h>
#include <string.h>
#include "generic_main.h"

// This parses URI parameters, and form posts in the application/x-www-form-urlencoded
// format. The provided string must be urldecoded before this is called.
int
gm_param_parse(const char * s, gm_param_t * p, int count)
{
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

      if ( ampersand )
        s = ampersand;
      else
        break;
    }
    else
      return -1;
  }
  return 0;
}
