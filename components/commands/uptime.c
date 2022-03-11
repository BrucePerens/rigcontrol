#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_timer.h>
#include <inttypes.h>

const int64_t second = 1000000;
const int64_t minute = second * 60;
const int64_t hour = minute * 60;
const int64_t day = hour * 24;

static struct {
    struct arg_end * end;
} args;

static int run(int argc, char * * argv)
{
  int64_t uptime = esp_timer_get_time();

  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  int64_t days = uptime / day;
  int64_t hours = (uptime % day) / hour;
  int64_t minutes = (uptime % hour) / minute;
  int64_t seconds = (uptime % minute) / second;

  printf("%" PRId64 " days, %" PRId64 ":%" PRId64 ":%" PRId64 "\n", days, hours, minutes, seconds);

  return 0;
}

void install_uptime_command(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "uptime",
    .help = "Display how long the system has been running.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
