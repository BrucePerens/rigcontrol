/// Console
//
// Install all of the console commands.
//
#include <stdio.h>
#include <stdlib.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include "generic_main.h"

static struct {
    struct arg_str * name;
    struct arg_str * value;
    struct arg_lit * erase;
    struct arg_end * end;
} nonvolatile_args;

static void
print_nonvolatile(const char * name, const char * value, const char * explanation, gm_nonvolatile_result_t type)
{
  const char * v = value;
  switch (type) {
  case GM_NORMAL:
    break;
  case GM_NOT_SET:
    v = "(not set)";
    break;
  case GM_SECRET:
    v = "(secret)";
    break;
  default:
    GM_FAIL("type %d", type);
    return;
  }
  gm_printf("%-8s\t%-8s\t%s\n", name, v, explanation);
}

static int nonvolatile(int argc, char * * argv)
{
  char buffer[128];
  const char * v = buffer;
  gm_nonvolatile_result_t type;

  int nerrors = arg_parse(argc, argv, (void **) &nonvolatile_args);
  if (nerrors) {
    arg_print_errors(stderr, nonvolatile_args.end, argv[0]);
      return 1;
  }

  if (nonvolatile_args.erase->count > 0) {
    if (nonvolatile_args.name->count < 1) {
      gm_printf("name must be specified.\n");
      return -1;
    }
    gm_nonvolatile_erase(nonvolatile_args.name->sval[0]);
    return 0;
  }

  switch (argc) {
  case 1:
    gm_nonvolatile_list(print_nonvolatile);
    return 0;
  case 3:
    type = gm_nonvolatile_set(nonvolatile_args.name->sval[0], nonvolatile_args.value->sval[0]);
    switch (type) {
    case GM_ERROR:
      return -1;
    case GM_NOT_IN_PARAMETER_TABLE:
      gm_printf("Error: not in nonvolatileeter table: %s\n",  nonvolatile_args.name->sval[0]);
      return -1;
    default:
      break;
    }
    // [[fallthrough]];
    // fall through
  case 2:
    type = gm_nonvolatile_get(nonvolatile_args.name->sval[0], buffer, sizeof(buffer));
    switch (type) {
    case GM_NORMAL:
      break;
    case GM_NOT_SET:
      v = "(not set)";
      break;
    case GM_SECRET:
      v = "(secret)";
      break;
    default:
      GM_FAIL("type %d", type);
      return -1;
    }
    gm_printf("\n\n%s \t%s\n", nonvolatile_args.name->sval[0], v);
    return 0;
  default:
    return -1;
  }
}

CONSTRUCTOR install(void)
{
  nonvolatile_args.name  = arg_str0(NULL, NULL, "name", "nonvolatileeter name");
  nonvolatile_args.value = arg_str0(NULL, NULL, "value", "value to set nonvolatileeter");
  nonvolatile_args.erase  = arg_litn(NULL, "erase", 0, 1, "[name] erase the nonvolatileeter");
  nonvolatile_args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "nv",
    .help = "Read or set parameters in non-volatile storage.",
    .hint = "[name] [value] (none to list, name to read, name value to set)",
    .func = &nonvolatile,
    .argtable = &nonvolatile_args
  };

  gm_command_register(&command);
}
