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
void			text(const char * pattern, ...);

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

void
html_a()
{
  tag("a", true);
}

void
html_abbr()
{
  tag("abbr", true);
}

void
html_address()
{
  tag("address", true);
}

void
html_area()
{
  tag("area", false);
}

void
html_article()
{
  tag("article", true);
}

void
html_aside()
{
  tag("aside", true);
}

void
html_attr(const char * name, const char * pattern, ...)
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
html_audio()
{
  tag("audio", true);
}

void
html_b()
{
  tag("b", true);
}

void
html_base()
{
  tag("base", false);
}

void
html_bdi()
{
  tag("bdi", true);
}

void
html_bdo()
{
  tag("bdo", true);
}

void
html_blockquote()
{
  tag("blockquote", true);
}

void
html_body()
{
  tag("body", true);
}

void
html_br()
{
  tag("br", false);
}

void
html_button()
{
  tag("button", true);
}

void
html_canvas()
{
  tag("canvas", true);
}

void
html_caption()
{
  tag("caption", true);
}

void
html_cite()
{
  tag("cite", true);
}

void
html_code()
{
  tag("code", true);
}

void
html_col()
{
  tag("col", false);
}

void
html_colgroup()
{
  tag("colgroup", true);
}

void
html_data()
{
  tag("data", true);
}

void
html_datalist()
{
  tag("datalist", true);
}

void
html_dd()
{
  tag("dd", true);
}

void
html_del()
{
  tag("del", true);
}

void
html_details()
{
  tag("details", true);
}

void
html_dfn()
{
  tag("dfn", true);
}

void
html_dialog()
{
  tag("dialog", true);
}

void
html_div()
{
  tag("div", true);
}

void
html_dl()
{
  tag("dl", true);
}

void
html_doctype()
{
  text("<!DOCTYPE HTML>\n");
}

void
html_dt()
{
  tag("dt", true);
}

void
html_em()
{
  tag("em", true);
}

void
html_embed()
{
  tag("embed", false);
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
    if ( current == &root )
      flush();
  }
  else {
    fail("end() called too many times (check for non-nesting tags).\n");
  }
}

void
html_fieldset()
{
  tag("fieldset", true);
}

void
html_figcaption()
{
  tag("figcaption", true);
}

void
html_figure()
{
  tag("figure", true);
}

void
html_footer()
{
  tag("footer", true);
}

void
html_form()
{
  tag("form", true);
}

void
html_h1()
{
  tag("h1", true);
}

void
html_head()
{
  tag("head", true);
}

void
html_header()
{
  tag("header", true);
}

void
html_hgroup()
{
  tag("hgroup", true);
}

void
html_hr()
{
  tag("hr", false);
}

void
html_html()
{
  tag("html", true);
}

void
html_i()
{
  tag("i", true);
}

void
html_iframe()
{
  tag("iframe", true);
}

void
html_img()
{
  tag("img", false);
}

void
html_input()
{
  tag("input", false);
}

void
html_ins()
{
  tag("ins", true);
}

void
html_kbd()
{
  tag("kbd", true);
}

void
html_keygen()
{
  tag("keygen", false);
}

void
html_label()
{
  tag("label", true);
}

void
html_legend()
{
  tag("legend", true);
}

void
html_li()
{
  tag("li", true);
}

void
html_link()
{
  tag("link", false);
}

void
html_main()
{
  tag("main", true);
}

void
html_map()
{
  tag("map", true);
}

void
html_mark()
{
  tag("mark", true);
}

void
html_menu()
{
  tag("menu", true);
}

void
html_menuitem()
{
  tag("menuitem", true);
}

void
html_meta()
{
  tag("meta", false);
}

void
html_meter()
{
  tag("meter", true);
}

void
html_nav()
{
  tag("nav", true);
}

void
html_noscript()
{
  tag("noscript", true);
}

void
html_object()
{
  tag("object", true);
}

void
html_ol()
{
  tag("ol", true);
}

void
html_optgroup()
{
  tag("optgroup", true);
}

void
html_option()
{
  tag("option", true);
}

void
html_output()
{
  tag("output", true);
}

void
html_p()
{
  tag("p", true);
}

void
html_param()
{
  tag("param", false);
}

void
html_picture()
{
  tag("picture", true);
}

void
html_pre()
{
  tag("pre", true);
}

void
html_progress()
{
  tag("progress", true);
}

void
html_q()
{
  tag("q", true);
}

void
html_rp()
{
  tag("rp", true);
}

void
html_rt()
{
  tag("rt", true);
}

void
html_ruby()
{
  tag("ruby", true);
}

void
html_s()
{
  tag("s", true);
}

void
html_samp()
{
  tag("samp", true);
}

void
html_script()
{
  tag("script", true);
}

void
html_section()
{
  tag("section", true);
}

void
html_select()
{
  tag("select", true);
}

void
html_small()
{
  tag("small", true);
}

void
html_source()
{
  tag("source", false);
}

void
html_span()
{
  tag("span", true);
}

void
html_strong()
{
  tag("strong", true);
}

void
html_style()
{
  tag("style", true);
}

void
html_sub()
{
  tag("sub", true);
}

void
html_summary()
{
  tag("summary", true);
}

void
html_sup()
{
  tag("sup", true);
}

void
html_svg()
{
  tag("svg", true);
}

void
html_table()
{
  tag("table", true);
}

void
html_tbody()
{
  tag("tbody", true);
}

void
html_td()
{
  tag("td", true);
}

void
html_templ() // "template" is a C++ keyword.
{
  tag("template", true);
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

void
html_textarea()
{
  tag("textarea", true);
}

void
html_tfoot()
{
  tag("tfoot", true);
}

void
html_th()
{
  tag("th", true);
}

void
html_thead()
{
  tag("thead", true);
}

void
html_time()
{
  tag("time", true);
}

void
html_title()
{
  tag("title", true);
}

void
html_tr()
{
  tag("tr", true);
}

void
html_track()
{
  tag("track", false);
}

void
html_u()
{
  tag("u", true);
}

void
html_ul()
{
  tag("ul", true);
}

void
html_var()
{
  tag("var", true);
}

void
html_video()
{
  tag("video", true);
}

void
html_wbr()
{
  tag("wbr", false);
}
