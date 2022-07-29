#include "generic_main.h"

static inline int
hexit(const char c)
{
  if ( c >= '0' && c <= '9' )
    return (c - '0');
  else if ( c >= 'a' && c <= 'f' )
    return (c - 'a') + 10;
  else if ( c >= 'A' && c <= 'F' )
    return (c - 'A') + 10;
  else
    return -1;
}

int
gm_uri_decode(const char * uri, char * buffer, size_t size)
{
  const char * s = (const char *)uri;
  char * b = (char *)buffer;

  while ( *s ) {
    if ( *s != '%' )
      *b++ = *s++;
    else {
      s++;
      
      int first = hexit(*s++);
      int second = hexit(*s++);
      if ( first < 0 || second < 0 ) {
        *buffer = '\0';
        return -1;
      }
      *b++ = (first * 10 + second) & 0xff;
    }

    if ( b > &buffer[size] )
      return -1;
  }
  *b = '\0';
  return 0;
}

