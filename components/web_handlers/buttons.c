#include <stdarg.h>
#include <stdio.h>
#include "web_template.h"

static void
button_internal(const char * t, const char * method, const char * l, va_list argument_list)
{
  char		buffer[128];

  vsnprintf(buffer, sizeof(buffer), l, argument_list);

  form
    attr("action", buffer)
    attr("method", method)
    input
    attr("type", "submit")
    attr("value", t)
  end
}

void
get_button(const char * t, const char * l, ...)
{
  char buffer[128];
  va_list argument_list;
  va_start(argument_list, l);

  vsnprintf(buffer, sizeof(buffer), l, argument_list);
  va_end(argument_list);

  button
    attr("onclick", "window.location.href='%s';", buffer);
    text(t);
  end
}


void
post_button(const char * t, const char * l, ...)
{
  va_list argument_list;
  va_start(argument_list, l);
  button_internal(t, "POST", l, argument_list);
  va_end(argument_list);
}
