#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_timer.h>
#include <inttypes.h>

const int64_t second = 1000000;
const int64_t minute = second * 60;
const int64_t hour = minute * 60;
const int64_t day = hour * 24;

extern void timer_to_human(int64_t t, char * buffer, size_t size)
{
  int64_t days = t / day;
  int64_t hours = (t % day) / hour;
  int64_t minutes = (t % hour) / minute;
  int64_t seconds = (t % minute) / second;

  snprintf(buffer, size, "%" PRId64 " days, %" PRId64 ":%" PRId64 ":%" PRId64 "", days, hours, minutes, seconds);
}
