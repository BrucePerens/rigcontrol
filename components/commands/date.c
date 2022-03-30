#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <time.h>
#include <esp_timer.h>
#include "generic_main.h"

static struct {
    struct arg_end * end;
} args;

// ESP-32 setenv() is not global across tasks or even commands.
// this has to be done every time.
static void timezone_set(void)
{
  char buffer[128];
  size_t buffer_size = sizeof(buffer);
  esp_err_t err = nvs_get_str(GM.nvs, "timezone", buffer, &buffer_size);

  if (err) {
    unsetenv("TZ");
    if (err != ESP_ERR_NVS_NOT_FOUND)
      gm_printf("Error getting timezone: %s\n", esp_err_to_name(err));
  }
  else {
    setenv("TZ", buffer, 1);
  }

  tzset();
}

static int run(int argc, char * * argv)
{
  time_t now;
  char strftime_buf[64];
  char duration_buf[64];
  struct tm timeinfo;
  
  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  time(&now);
  
  timezone_set();

  const char * ago = " ago";
  int64_t timer_now = esp_timer_get_time();
  int64_t duration = timer_now - GM.time_last_synchronized;

  if (GM.time_last_synchronized == -1) {
    strcpy(duration_buf, "never");
    ago = "";
  }
  else {
    gm_timer_to_human(duration, duration_buf, sizeof(duration_buf));
  }
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  gm_printf("%s, last synchronized: %s%s\n", strftime_buf, duration_buf, ago);
  return 0;
}

CONSTRUCTOR install(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "date",
    .help = "Display the date and time, and SNTP synchronization status.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  gm_command_register(&command);
}
