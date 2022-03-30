#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_timer.h>
#include <inttypes.h>
#include "generic_main.h"

const int64_t second = 1000000;
const int64_t minute = second * 60;
const int64_t hour = minute * 60;
const int64_t day = hour * 24;

extern void gm_timer_to_human(int64_t t, char * buffer, size_t size)
{
  int days = t / day;
  int hours = (t % day) / hour;
  int minutes = (t % hour) / minute;
  int seconds = (t % minute) / second;

  if (days > 0) {
    snprintf(buffer, size, "%d days, ", days);
    size_t length = strlen(buffer);
    buffer += length;
    size -= size;
  }
  snprintf(buffer, size, "%02d:%02d:%02d", hours, minutes, seconds);
}
