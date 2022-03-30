#include <stdarg.h>
#include <stdio.h>
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "generic_main.h"

// Print to the console, with mutual exclusion.
// This is only usable in tasks, not anything at interrupt level.
int
gm_vprintf(const char * pattern, va_list args)
{
  int length;
  pthread_mutex_lock(&GM.console_print_mutex);
  length = vfprintf(stdout, pattern, args);
  pthread_mutex_unlock(&GM.console_print_mutex);
  return length;
}

int
gm_printf(const char * pattern, ...)
{
  int length;
  va_list args;
  va_start(args, pattern);
  length = gm_vprintf(pattern, args);
  va_end(args);
  return length;
}
