#include <stdio.h>
#include <stdlib.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include "generic_main.h"

static struct {
    struct arg_str * url;
    struct arg_end * end;
} args;

static void web_data_coroutine(const char * data, size_t size)
{
  printf("%s", data);
}

static int run(int argc, char * * argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  web_get_with_coroutine(args.url->sval[0], web_data_coroutine);

  return 0;
}


CONSTRUCTOR install()
{

  args.url  = arg_str1(NULL, NULL, "url", "URL to retrieve.");
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "wget",
    .help = "Retrieve a web page and display the raw data on the console.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  command_register(&command);
}
