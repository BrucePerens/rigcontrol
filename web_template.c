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
  struct html *		next;
  struct html *		parent;
  bool			nesting;
  union {
    struct {
      param_t *		params;
      param_t * *	params_end;
      struct html *	content;
      struct html * *	content_end;
    } tag;
    struct {
      const char *	text;
    } raw;
  };
};
typedef struct html html_t;

static const char	document[] = "document";
static const char	raw[] = "raw";

static html_t		root = { };
static html_t *		current = &root;
static char		scratchpad[1024];

static char *		stralloc(const char *);
static void		fail(const char * pattern, ...);

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
  if ( current->nesting ) {
    current = current->parent;
    if ( current == &root ) {
    }
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
  param_t * const p = mem(sizeof(*p));

  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  snprintf(scratchpad, sizeof(scratchpad), pattern, argument_pointer);
  va_end(argument_pointer);

  p->name = name;
  p->value = stralloc(scratchpad);

  *(current->tag.params_end) = p;
  current->tag.params_end = &(p->next);
}

static void
splice(html_t * h)
{
  if ( root.name == 0 ) {
    root.name = document;
    root.tag.content_end = &(root.tag.content);
    root.tag.params_end = &(root.tag.params);
  }
  h->parent = current;
  *(current->tag.content_end) = h;
  current->tag.content_end = &(h->next);
  if ( h->nesting )
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
  html_t * h = mem(sizeof(html_t));

  h->name = name;
  h->nesting = nesting;
  splice(h);
}

static void
text(const char * pattern, ...)
{
  va_list argument_pointer;
  html_t * h = mem(sizeof(html_t));

  va_start(argument_pointer, pattern);
  h->name = raw;
  snprintf(scratchpad, sizeof(scratchpad), pattern, argument_pointer);
  h->raw.text = stralloc(scratchpad);
  h->nesting = false;
  splice(h);
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
