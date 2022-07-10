#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <pthread.h>
extern "C" {
  #include "generic_main.h"
};
#include "web_template.hxx"

typedef struct tag {
  const char *	name;
  struct tag *	parent;
  unsigned int	open:1;
  unsigned int	nesting:1;
} tag_t;

static const char	document[] = "document";
static const char	raw[] = "raw";

static tag_t		root = { };
static tag_t *		current = &root;

static void		emit(const char * pattern, ...);
static void		fail(const char * pattern, ...);
static void		finish_current_tag();
static void		flush();

char	buffer[2048];
int	buf_index = 0;

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
end()
{
  finish_current_tag(); // This changes current;

  tag_t * const h = current;
  if ( h->nesting ) {
    emit("</%s>", h->name); 
    tag_t * const parent = h->parent;
    free(current);
    current = parent;
    if ( current == &root )
      flush();
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
  gm_send_to_client(buffer, buf_index);
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

static void
tag(const char * name, bool nesting)
{
  finish_current_tag();
  emit("<%s", name);

  tag_t * h = (tag_t *)mem(sizeof(tag_t));
  h->name = name;
  h->open = true;
  h->nesting = nesting ? 1 : 0;
  splice(h);
}

static void
text(const char * pattern, ...)
{
  va_list argument_pointer;

  finish_current_tag();
  va_start(argument_pointer, pattern);
  emit_va(pattern, argument_pointer);
  va_end(argument_pointer);
}

namespace HTML {
  void
  attr(const char * name, const char * pattern, ...)
  {
    va_list argument_pointer;
    va_start(argument_pointer, pattern);
  
    if ( !current->open ) {
      fail("attr() must be under the tag it applies to, before anything but another param().\n");
    }
    emit(" %s=\"", name);
    emit(pattern, argument_pointer);
    emit("\"");
  
    va_end(argument_pointer);
  }

  void
  a()
  {
    tag("a", true);
  }
  
  void
  abbr()
  {
    tag("abbr", true);
  }
  
  void
  address()
  {
    tag("address", true);
  }
  
  void
  area()
  {
    tag("area", false);
  }
  
  void
  article()
  {
    tag("article", true);
  }
  
  void
  aside()
  {
    tag("aside", true);
  }
  
  void
  audio()
  {
    tag("audio", true);
  }
  
  void
  b()
  {
    tag("b", true);
  }
  
  void
  base()
  {
    tag("base", false);
  }
  
  void
  bdi()
  {
    tag("bdi", true);
  }
  
  void
  bdo()
  {
    tag("bdo", true);
  }
  
  void
  blockquote()
  {
    tag("blockquote", true);
  }
  
  void
  body()
  {
    tag("body", true);
  }
  
  void
  br()
  {
    tag("br", false);
  }
  
  void
  button()
  {
    tag("button", true);
  }
  
  void
  canvas()
  {
    tag("canvas", true);
  }
  
  void
  caption()
  {
    tag("caption", true);
  }
  
  void
  cite()
  {
    tag("cite", true);
  }
  
  void
  code()
  {
    tag("code", true);
  }
  
  void
  col()
  {
    tag("col", false);
  }
  
  void
  colgroup()
  {
    tag("colgroup", true);
  }
  
  void
  data()
  {
    tag("data", true);
  }
  
  void
  datalist()
  {
    tag("datalist", true);
  }
  
  void
  dd()
  {
    tag("dd", true);
  }
  
  void
  del()
  {
    tag("del", true);
  }
  
  void
  details()
  {
    tag("details", true);
  }
  
  void
  dfn()
  {
    tag("dfn", true);
  }
  
  void
  dialog()
  {
    tag("dialog", true);
  }
  
  void
  div()
  {
    tag("div", true);
  }
  
  void
  dl()
  {
    tag("dl", true);
  }
  
  void
  doctype()
  {
    text("<!DOCTYPE HTML>\n");
  }

  void
  dt()
  {
    tag("dt", true);
  }
  
  void
  em()
  {
    tag("em", true);
  }
  
  void
  embed()
  {
    tag("embed", false);
  }
  
  void
  fieldset()
  {
    tag("fieldset", true);
  }
  
  void
  figcaption()
  {
    tag("figcaption", true);
  }
  
  void
  figure()
  {
    tag("figure", true);
  }
  
  void
  footer()
  {
    tag("footer", true);
  }
  
  void
  form()
  {
    tag("form", true);
  }
  
  void
  h1()
  {
    tag("h1", true);
  }
  
  void
  head()
  {
    tag("head", true);
  }
  
  void
  header()
  {
    tag("header", true);
  }
  
  void
  hgroup()
  {
    tag("hgroup", true);
  }
  
  void
  hr()
  {
    tag("hr", false);
  }
  
  void
  html()
  {
    tag("html", true);
  }
  
  void
  i()
  {
    tag("i", true);
  }
  
  void
  iframe()
  {
    tag("iframe", true);
  }
  
  void
  img()
  {
    tag("img", false);
  }
  
  void
  input()
  {
    tag("input", false);
  }
  
  void
  ins()
  {
    tag("ins", true);
  }
  
  void
  kbd()
  {
    tag("kbd", true);
  }
  
  void
  keygen()
  {
    tag("keygen", false);
  }
  
  void
  label()
  {
    tag("label", true);
  }
  
  void
  legend()
  {
    tag("legend", true);
  }
  
  void
  li()
  {
    tag("li", true);
  }
  
  void
  link()
  {
    tag("link", false);
  }
  
  void
  main()
  {
    tag("main", true);
  }
  
  void
  map()
  {
    tag("map", true);
  }
  
  void
  mark()
  {
    tag("mark", true);
  }
  
  void
  menu()
  {
    tag("menu", true);
  }
  
  void
  menuitem()
  {
    tag("menuitem", true);
  }
  
  void
  meta()
  {
    tag("meta", false);
  }
  
  void
  meter()
  {
    tag("meter", true);
  }
  
  void
  nav()
  {
    tag("nav", true);
  }
  
  void
  noscript()
  {
    tag("noscript", true);
  }
  
  void
  object()
  {
    tag("object", true);
  }
  
  void
  ol()
  {
    tag("ol", true);
  }
  
  void
  optgroup()
  {
    tag("optgroup", true);
  }
  
  void
  option()
  {
    tag("option", true);
  }
  
  void
  output()
  {
    tag("output", true);
  }
  
  void
  p()
  {
    tag("p", true);
  }
  
  void
  param()
  {
    tag("param", false);
  }
  
  void
  picture()
  {
    tag("picture", true);
  }
  
  void
  pre()
  {
    tag("pre", true);
  }
  
  void
  progress()
  {
    tag("progress", true);
  }
  
  void
  q()
  {
    tag("q", true);
  }
  
  void
  rp()
  {
    tag("rp", true);
  }
  
  void
  rt()
  {
    tag("rt", true);
  }
  
  void
  ruby()
  {
    tag("ruby", true);
  }
  
  void
  s()
  {
    tag("s", true);
  }
  
  void
  samp()
  {
    tag("samp", true);
  }
  
  void
  script()
  {
    tag("script", true);
  }
  
  void
  section()
  {
    tag("section", true);
  }
  
  void
  select()
  {
    tag("select", true);
  }
  
  void
  small()
  {
    tag("small", true);
  }
  
  void
  source()
  {
    tag("source", false);
  }
  
  void
  span()
  {
    tag("span", true);
  }
  
  void
  strong()
  {
    tag("strong", true);
  }
  
  void
  style()
  {
    tag("style", true);
  }
  
  void
  sub()
  {
    tag("sub", true);
  }
  
  void
  summary()
  {
    tag("summary", true);
  }
  
  void
  sup()
  {
    tag("sup", true);
  }
  
  void
  svg()
  {
    tag("svg", true);
  }
  
  void
  table()
  {
    tag("table", true);
  }
  
  void
  tbody()
  {
    tag("tbody", true);
  }
  
  void
  td()
  {
    tag("td", true);
  }
  
  void
  templ() // "template" is a C++ keyword.
  {
    tag("template", true);
  }
  
  void
  textarea()
  {
    tag("textarea", true);
  }
  
  void
  tfoot()
  {
    tag("tfoot", true);
  }
  
  void
  th()
  {
    tag("th", true);
  }
  
  void
  thead()
  {
    tag("thead", true);
  }
  
  void
  time()
  {
    tag("time", true);
  }
  
  void
  title()
  {
    tag("title", true);
  }
  
  void
  tr()
  {
    tag("tr", true);
  }
  
  void
  track()
  {
    tag("track", false);
  }
  
  void
  u()
  {
    tag("u", true);
  }
  
  void
  ul()
  {
    tag("ul", true);
  }
  
  void
  var()
  {
    tag("var", true);
  }
  
  void
  video()
  {
    tag("video", true);
  }
  
  void
  wbr()
  {
    tag("wbr", false);
  }
}

int
main(int argc, char * * argv)
{
  using namespace HTML;
  doctype();
  html();
    head();
    end();
    body();
      text("Hello!");
    end();
  end();
}

