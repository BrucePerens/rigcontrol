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
} param_args;

static void
print_param(const char * name, const char * value, nvs_param_result_t type)
{
  const char * v = value;
  switch (type) {
  case NORMAL:
    break;
  case NOT_SET:
    v = "(not set)";
    break;
  case SECRET:
    v = "(secret)";
    break;
  default:
    fprintf(stderr, "Error: type %d.\n", type);
    return;
  }
  printf("%s:\t%s\n", name, v);
}

static int param(int argc, char * * argv)
{
  char buffer[128];
  const char * v = buffer;
  nvs_param_result_t type;

  int nerrors = arg_parse(argc, argv, (void **) &param_args);
  if (nerrors) {
    arg_print_errors(stderr, param_args.end, argv[0]);
      return 1;
  }

  if (param_args.erase->count > 0) {
    if (param_args.name->count < 1) {
      fprintf(stderr, "name must be specified.\n");
      return -1;
    }
    erase_nvs_param(param_args.name->sval[0]);
    return 0;
  }

  switch (argc) {
  case 1:
    list_nvs_params(print_param);
    return 0;
  case 3:
    type = set_nvs_param(param_args.name->sval[0], param_args.value->sval[0]);
    switch (type) {
    case ERROR:
      return -1;
    case NOT_IN_PARAMETER_TABLE:
      fprintf(stderr, "Error: not in parameter table: %s\n",  param_args.name->sval[0]);
      return -1;
    default:
      break;
    }
    // [[fallthrough]];
    // fall through
  case 2:
    type = get_nvs_param(param_args.name->sval[0], buffer, sizeof(buffer));
    switch (type) {
    case NORMAL:
      break;
    case NOT_SET:
      v = "(not set)";
      break;
    case SECRET:
      v = "(secret)";
      break;
    default:
      fprintf(stderr, "Error: type %d.\n", type);
      return -1;
    }
    printf("%s\t%s\n", param_args.name->sval[0], v);
    return 0;
  default:
    return -1;
  }
}

void install_param_command(void)
{
  param_args.name  = arg_str0(NULL, NULL, "name", "parameter name");
  param_args.value = arg_str0(NULL, NULL, "value", "value to set parameter");
  param_args.erase  = arg_litn(NULL, "erase", 0, 1, "[name] erase the parameter");
  param_args.end = arg_end(10);
  static const esp_console_cmd_t param_cmd = {
    .command = "param",
    .help = "Read or set parameters in non-volatile storage.",
    .hint = "[name] [value] (none to list, name to read, name value to set)",
    .func = &param,
    .argtable = &param_args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&param_cmd) );
}
