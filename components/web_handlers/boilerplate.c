#include "web_template.h"

void html_boilerplate(const char * t)
{
  doctype
  html
    head
      title
        text(t)
      end
    end
    body
      h1
        text(t)
      end
}

void html_end_boilerplate()
{
    end // body.
  end // html.
}
