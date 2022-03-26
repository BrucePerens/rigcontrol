#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_console.h>
#include <argtable3/argtable3.h>
#include <driver/gpio.h>
#include <hal/gpio_hal.h>
#include <hal/gpio_types.h>
#include "generic_main.h"

enum GPIO_Mode {
  FLOATING,
  PULL_DOWN,
  PULL_UP,
  OUT,
};

static struct {
    struct arg_int * pin;
    struct arg_int * level;
    struct arg_lit * out;
    struct arg_lit * pull_up;
    struct arg_lit * pull_down;
    struct arg_lit * floating;
    struct arg_end * end;
} gpio_args;

int
set_gpio_mode(gpio_num_t pin, enum GPIO_Mode mode)
{
  gpio_config_t config;
  bool out = false;

  memset(&config, 0, sizeof(config));

  gpio_reset_pin(pin);

  switch (mode) {
  case FLOATING:
  case PULL_DOWN:
  case PULL_UP:
    out = false;
    break;
  case OUT:
    out = true;
    break;
  default:
    return -1;
  }

  if (pin < 0 || pin > 63) {
    fprintf(stderr, "pin must be in the range of 0-63.");
    return -1;
  }

  config.pin_bit_mask = 1 << pin;

  if (out) {
    gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_2); // Medium.
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
  }
  else {
    gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_0); // Weak.
    config.mode = GPIO_MODE_INPUT;
    switch (mode) {
    case FLOATING:
      config.pull_up_en = GPIO_PULLUP_DISABLE;
      config.pull_down_en = GPIO_PULLDOWN_DISABLE;
      break;
    case PULL_DOWN:
      config.pull_up_en = GPIO_PULLUP_DISABLE;
      config.pull_down_en = GPIO_PULLDOWN_ENABLE;
      break;
    case PULL_UP:
      config.pull_up_en = GPIO_PULLUP_ENABLE;
      config.pull_down_en = GPIO_PULLDOWN_ENABLE;
      break;
    default:
      return -1;
    }
  }
  esp_err_t err = gpio_config(&config);
  if (err) {
    ESP_ERROR_CHECK(err);
    return -1;
  }
  else {
    printf("OK\n");
    return 0;
  }
}

int
get_gpio_level(gpio_num_t pin)
{
  int level = gpio_get_level(pin);

  printf("%d %d\n", pin, level);
  return 0;
}

int
set_gpio_level(gpio_num_t pin, int level)
{
  esp_err_t err = gpio_set_level(pin, level != 0);
  if (err) {
    fprintf(stderr, "Failed.");
    return -1;
  }
  else {
    int level = gpio_get_level(pin);

    printf("%d %d\n", pin, level);
  }
  return 0;
}

static int gpio(int argc, char * * argv)
{
  int nerrors = arg_parse(argc, argv, (void **) &gpio_args);
  if (nerrors) {
    arg_print_errors(stderr, gpio_args.end, argv[0]);
      return 1;
  }

  if (gpio_args.out->count + gpio_args.pull_down->count + gpio_args.pull_up->count + gpio_args.floating->count > 1) {
    fprintf(stderr, "--out, --pull_down, --pull_up, and --float are mutually exclusive.");
    return -1;
  }

  if (gpio_args.out->count > 0) {
    set_gpio_mode((gpio_num_t)gpio_args.pin->ival[0], OUT);
  }
  else if (gpio_args.pull_down->count > 0) {
    set_gpio_mode((gpio_num_t)gpio_args.pin->ival[0], PULL_DOWN);
  }
  else if (gpio_args.pull_up->count > 0) {
    set_gpio_mode((gpio_num_t)gpio_args.pin->ival[0], PULL_UP);
  }
  else if (gpio_args.floating->count > 0) {
    set_gpio_mode((gpio_num_t)gpio_args.pin->ival[0], FLOATING);
  }
  else if (gpio_args.level->count > 0) {
    set_gpio_level((gpio_num_t)gpio_args.pin->ival[0], gpio_args.level->ival[0]);
  }
  else {
    get_gpio_level((gpio_num_t)gpio_args.pin->ival[0]);
  }

  return 0;
}

CONSTRUCTOR install(void)
{
  gpio_args.pin  = arg_int1(NULL, NULL, "pin", "pin to manipulate");
  gpio_args.level = arg_int0(NULL, NULL, "level", "value 0 or 1 to output");
  gpio_args.out = arg_lit0(NULL, "out", "set pin to be an out");
  gpio_args.pull_down = arg_lit0(NULL, "pull_down", "set pin to be an input with weak pull-down");
  gpio_args.pull_up = arg_lit0(NULL, "pull_up", "set pin to be an input with weak pull-up");
  gpio_args.floating = arg_lit0(NULL, "float", "set pin to be a floating input");
  gpio_args.end = arg_end(10);

  static const esp_console_cmd_t command = {
    .command = "gpio",
    .help = "Read from or output to GPIO pins.",
    .hint = "[pin] [level] or (pin to read, pin level to output)",
    .func = &gpio,
    .argtable = &gpio_args
  };

  command_register(&command);
}
