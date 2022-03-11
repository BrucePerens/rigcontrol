/// Console
//
// Install all of the console commands.
//
#include <stdio.h>
#include <stdlib.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>

extern int list_nvs_params(void);
extern int get_nvs_param(const char * name);
extern int set_nvs_param(const char * name, const char * value);
extern int erase_nvs_param(const char * name);

static struct {
    struct arg_str * name;
    struct arg_str * value;
    struct arg_lit * erase;
    struct arg_end * end;
} param_args;

static int param(int argc, char * * argv)
{
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
    return list_nvs_params();
  case 2:
    return get_nvs_param(param_args.name->sval[0]);
  case 3:
    return set_nvs_param(param_args.name->sval[0], param_args.value->sval[0]);
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
    .help = "Configure parameters in non-volatile storage.",
    .hint = "[name] [value] (none to list, name to read, name value to set)",
    .func = &param,
    .argtable = &param_args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&param_cmd) );
}
