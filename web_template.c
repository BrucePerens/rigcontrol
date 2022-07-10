#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct param {
  const char *		name;
  const char *		value;
  struct param *	next;
} param_t;

struct html {
  const char *		name;
  struct html *		parent;
  bool			open;
  bool			nesting;
};
typedef struct html html_t;

static const char	document[] = "document";
static const char	raw[] = "raw";

static html_t		root = { };
static html_t *		current = &root;
static char		scratchpad[1024];

static void		emit(const char * pattern, ...);
static void		fail(const char * pattern, ...);
static void		finish_current_tag();
static char *		stralloc(const char *);

static void
output(const char * pattern, va_list argument_pointer)
{
  vfprintf(stdout, pattern, argument_pointer);
}

static void
emit(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  output(pattern, argument_pointer);
  va_end(argument_pointer);
}

static void
end()
{
  html_t * const h = current;

  finish_current_tag();
  if ( h->nesting ) {
    emit("</%s>", h->name);
    html_t * const parent = h->parent;
    free(current);
    current = parent;
  }
  else {
    fail("end() called too many times (check for non-nesting tags).\n");
  }
}

static void
fail(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  vfprintf(stderr, pattern, argument_pointer);
  va_end(argument_pointer);
  exit(1);
}

static void
finish_current_tag()
{
  if ( current->open ) {
    if ( current->nesting ) {
      emit(">");
      current->open = false;
    }
    else {
      emit("/>");
      html_t * parent = current->parent;
      free(current);
      current = parent;
    }
  }
}

static void *
mem(size_t size)
{
  void * const m = malloc(size);
  if ( m == 0 ) 
    fail("Out of memory.\n");
  memset(m, '\0', size);
  return m;
}

static void
param(const char * name, const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);

  if ( !current->open ) {
    fail("param() must be under the tag it applies to, before anything but another param().\n");
  }
  emit(" %s=\"", name);
  emit(pattern, argument_pointer);
  emit("\"");

  va_end(argument_pointer);
}

static void
splice(html_t * h)
{
  if ( root.name == 0 ) {
    root.name = document;
  }
  h->parent = current;
  current = h;
}

static char *
stralloc(const char * s)
{
  const size_t 	length = strlen(s) + 1;
  char * const	c = malloc(length);
  if ( c == 0 )
    fail("Out of memory.\n");

  memcpy(c, s, length);
  return c;
}

static void
tag(const char * name, bool nesting)
{
  finish_current_tag();
  emit("<%s", name);

  html_t * h = mem(sizeof(html_t));
  h->name = name;
  h->open = true;
  h->nesting = nesting;
  splice(h);
}

static void
text(const char * pattern, ...)
{
  va_list argument_pointer;

  finish_current_tag();
  va_start(argument_pointer, pattern);
  output(pattern, argument_pointer);
  va_end(argument_pointer);
}

void
body()
{
  tag("body", true);
}

void
html()
{
  tag("html", true);
}

void
p()
{
  tag("p", true);
}

int main(int argc, char * * argv)
{
  html();
    param("lang", "en");
    body();
      text("Hello, World!");
    end();
  end();
  return 0;
}
