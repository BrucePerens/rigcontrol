#include <stdlib.h>
#include <string.h>
#include "generic_main.h"

const char *
gm_param(const gm_param_t * p, int size, const char * name)
{
  for ( int i = 0; i < size; i++ ) {
    if ( p[i].name && strcmp(name, p[i].name) == 0 ) {
      return p[i].value;
    }
  }
  return 0;
}
