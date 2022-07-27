#include <stdarg.h>
#include <stdio.h>
#include "web_template.h"

void html_boilerplate(const char * pattern, ...)
{
  const char * titl = VSPRINTF(pattern);

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
