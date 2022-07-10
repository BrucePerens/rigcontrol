#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct param {
  const char *		name;
  const char *		value;
  struct param *	next;
} param_t;

struct html {
  const char *		name;
  struct html *		next;
  struct html *		parent;
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

static html_t		doc = { };
static html_t *		current = &doc;
static char		scratchpad[1024];

void
emit(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  vfprintf(stdout, pattern, argument_pointer);
  va_end(argument_pointer);
}

void
end()
{
}

void
fail(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  vfprintf(stderr, pattern, argument_pointer);
  va_end(argument_pointer);
  exit(1);
}

void *
mem(size_t size)
{
  void * const m = malloc(size);
  if ( m == 0 ) 
    fail("Out of memory.\n");
  memset(m, '\0', size);
  return m;
}

void
param(const char * name, const char * pattern, ...)
{
  param_t * const p = mem(sizeof(*p));

  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  vsnprintf(scratchpad, sizeof(scratchpad), pattern, argument_pointer);
  va_end(argument_pointer);

  p->name = name;
  p->value = stralloc(scratchpad);
  *(current->tag.params_end) = p;
  current->tag.params_end = &(p->next);
}

void
splice(html_t * h)
{
  if ( doc.name == 0 ) {
    doc.name = document;
    doc.tag.content_end = &(doc.tag.content);
    doc.tag.params_end = &(doc.tag.params);
  }
  if ( h->name != raw ) {
    h->parent = current;
    *(current->tag.content_end) = h;
    current->tag.content_end = &(h->next);
    current = h;
  }
}

const char *
stralloc(const char * s)
{
  const size_t 	length = strlen(s) + 1;
  char * const	c = malloc(length);
  if ( c == 0 )
    fail("Out of memory.\n");

  memcpy(c, s, length);
  return c;
}

void
tag(const char * name)
{
  html_t * h = mem(sizeof(html_t));

  emit("<%s", name);
  h->name = name;
  splice(h);
}

void
p()
{
  tag("p");
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
