#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "generic_main.h"

typedef struct tag {
  const char *	name;
  struct tag *	parent;
  unsigned int	open:1;
  unsigned int	nesting:1;
} tag_t;

static const char	document[] = "document";

static tag_t		root = { };
static tag_t *		current = &root;

static void		emit(const char * pattern, ...);
static void		fail(const char * pattern, ...);
static void		finish_current_tag();
static void		flush();
void			html_text(const char * pattern, ...);

static char	buffer[2048];
static int	buf_index = 0;

static void
emit_va(const char * pattern, va_list argument_pointer)
{
  size_t size = vsnprintf(&buffer[buf_index], sizeof(buffer) - buf_index, pattern, argument_pointer);
  buf_index += size;
  if ( buf_index >= sizeof(buffer) )
    fail("output (probably text) too large for buffer.\n");

  if ( buf_index > sizeof(buffer) / 2 )
    flush();
}

static void
emit(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  emit_va(pattern, argument_pointer);
  va_end(argument_pointer);
}

static void
fail(const char * pattern, ...)
{
  va_list argument_pointer;
  va_start(argument_pointer, pattern);
  vfprintf(stderr, pattern, argument_pointer);
  va_end(argument_pointer);
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
      emit(">"); // There is no "/>" in HTML 5.
      tag_t * parent = current->parent;
      free(current);
      current = parent;
    }
  }
}

static void
flush()
{
  gm_web_send_to_client(buffer, buf_index);
  buf_index = 0;
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
splice(tag_t * h)
{
  if ( root.name == 0 ) {
    root.name = document;
  }
  h->parent = current;
  current = h;
}

void
html_tag(const char * name, bool nesting)
{
  finish_current_tag();
  emit("<%s", name);

  tag_t * h = (tag_t *)mem(sizeof(tag_t));
  h->name = name;
  h->open = true;
  h->nesting = nesting ? 1 : 0;
  splice(h);
}

void
html_attr(const char * name, const char * pattern, ...)
{
  va_list argument_pointer;

  if ( !current->open ) {
    fail("attr() must be under the tag it applies to, before anything but another param().\n");
  }
  emit(" %s=\"", name);

  va_start(argument_pointer, pattern);
  emit_va(pattern, argument_pointer);
  va_end(argument_pointer);

  emit("\"");
}

void
html_doctype()
{
  html_text("<!DOCTYPE HTML>\n");
}

void
html_end()
{
  finish_current_tag(); // This changes current;

  tag_t * const h = current;
  if ( h->nesting ) {
    emit("</%s>", h->name); 
    tag_t * const parent = h->parent;
    free(current);
    current = parent;
    if ( current == &root ) {
      flush();
      gm_web_finish();
    }
  }
  else {
    fail("end() called too many times (check for non-nesting tags).\n");
  }
}

void
html_text(const char * pattern, ...)
{
  va_list argument_pointer;

  finish_current_tag();
  va_start(argument_pointer, pattern);
  emit_va(pattern, argument_pointer);
  va_end(argument_pointer);
}
