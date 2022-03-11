#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <time.h>

extern nvs_handle_t nvs;

static struct {
    struct arg_end * end;
} args;

// ESP-32 setenv() is not global across tasks or even commands.
// this has to be done every time.
static void timezone_set(void)
{
  char buffer[128];
  size_t buffer_size;
  esp_err_t err = nvs_get_str(nvs, "timezone", buffer, &buffer_size);

  if (err) {
    unsetenv("TZ");
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
  struct tm timeinfo;
  sntp_sync_status_t sync_status = sntp_get_sync_status();
  const char not_synchronized[] = "not synchronized via SNTP";
  const char * sync_string = not_synchronized;
  
  int nerrors = arg_parse(argc, argv, (void **) &args);
  if (nerrors) {
    arg_print_errors(stderr, args.end, argv[0]);
      return 1;
  }

  switch (sync_status) {
  case SNTP_SYNC_STATUS_COMPLETED:
    sync_string = "synchronized via SNTP";
    break;
  case SNTP_SYNC_STATUS_IN_PROGRESS:
    sync_string = "SNTP synchronization in progress";
    break;
  case SNTP_SYNC_STATUS_RESET:
    sync_string = not_synchronized;
  }

  time(&now);
  
  timezone_set();

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  printf("%s, %s\n", strftime_buf, sync_string);
  return 0;
}

void install_date_command(void)
{
  args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "date",
    .help = "Display the date and time, and SNTP synchronization status.",
    .hint = NULL,
    .func = &run,
    .argtable = &args
  };

  ESP_ERROR_CHECK( esp_console_cmd_register(&command) );
}
