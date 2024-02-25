#include <stdarg.h>
#include <stdio.h>
#include "web_template.h"

void html_boilerplate(const char * pattern, ...)
{
  char		titl[128];

  va_list argument_list;
  va_start(argument_list, pattern);
  vsnprintf(titl, sizeof(titl), pattern, argument_list);
  va_end(argument_list);

  doctype
  html
    head
      title
        text(titl)
      end
    end
    body
      h1
        text(titl)
      end
}

void html_end_boilerplate()
{
    end // body.
  end // html.
}
