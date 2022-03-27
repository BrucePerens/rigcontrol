#include <stdio.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_console.h>
#include <esp_system.h>
#include <argtable3/argtable3.h>
#include "generic_main.h"

extern esp_err_t print_real_time_stats(TickType_t xTicksToWait);

static struct {
    struct arg_lit * cpu;
    struct arg_end * end;
} tasks_args;

static int tasks(int argc, char * * argv)
{
  char stats_buffer[1024];

  int nerrors = arg_parse(argc, argv, (void **) &tasks_args);
  if (nerrors) {
    arg_print_errors(stderr, tasks_args.end, argv[0]);
      return 1;
  }

  if (tasks_args.cpu->count > 0) {
    // The task statistics functions block interrupts while they are running,
    // and that does something odd to command-line processing, so that the
    // carriage return isn't printed. So, add one here.
    printf("\n");
    print_real_time_stats(100);
  }
  else {
    printf("Task Name\tStatus\tPrio\tHWM\tTask\tAffinity\n");
    vTaskList(stats_buffer);
    printf("%s\n", stats_buffer);
  }
  return 0;
}

CONSTRUCTOR install(void)
{
  tasks_args.cpu  = arg_lit0(NULL, "cpu", "Collect CPU time usage for one second, and display.");
  tasks_args.end = arg_end(10);
  static const esp_console_cmd_t command = {
    .command = "tasks",
    .help = "Display FreeRTOS tasks.",
    .hint = NULL,
    .func = &tasks,
    .argtable = &tasks_args
  };

  gm_command_register(&command);
}
