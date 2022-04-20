// WARNING:
// The default toolchain for ESP-IDF has the 2038 problem. They give instructions on how to rebuild the toolchain to use a
// 64-bit time_t at https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#bit-time-t
// You should NOT release production binaries with 16-bit time.
//
// Provide a task that does select() on file descriptors that have been registered with it, and calls handlers when there are
// events upon those file descriptors. This allows us to do event-driven I/O without depending upon the yet-immature ESP-IDF
// ASIO port.
//
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include "generic_main.h"

#define NUMBER_OF_FDS	100

static TaskHandle_t select_task_id = NULL;

static fd_set read_fds = {};
static fd_set write_fds = {};
static fd_set exception_fds = {};
static int fd_limit = 0;
static gm_fd_handler_t handlers[NUMBER_OF_FDS];
static void * data[NUMBER_OF_FDS];
static struct timeval timeouts[NUMBER_OF_FDS];

void
gm_fd_register(int fd, gm_fd_handler_t handler, void * d, bool readable, bool writable, bool exception, uint32_t seconds) {
  int limit = fd + 1;
  struct timeval now;
  struct timeval * t;

  if ( fd_limit < limit )
    fd_limit = limit;

  handlers[fd] = handler;
  data[fd] = d;

  gettimeofday(&now, 0);

  t = &timeouts[fd];

  if ( seconds ) {
    t->tv_sec = now.tv_sec + seconds;
    t->tv_usec = now.tv_usec;
  }
  else {
    timerclear(t);
  }
  
  if ( readable )
    FD_SET(fd, &read_fds);
  else
    FD_CLR(fd, &read_fds);

  if ( writable )
    FD_SET(fd, &write_fds);
  else
    FD_CLR(fd, &write_fds);

  if ( exception )
    FD_SET(fd, &exception_fds);
  else
    FD_CLR(fd, &exception_fds);
}

void
gm_fd_unregister(int fd) {
  handlers[fd] = (gm_fd_handler_t)0;
  data[fd] = 0;
  timerclear(&timeouts[fd]);
  FD_CLR(fd, &read_fds);
  FD_CLR(fd, &write_fds);
  FD_CLR(fd, &exception_fds);

  // If the last FD is cleared, set fd_limit to the highest remaining set fd, plus one. 
  if ( fd_limit == fd + 1 ) {
    for ( int i = fd_limit - 2; i >= 0; i-- ) {
       if ( FD_ISSET(i, &read_fds) || FD_ISSET(i, &write_fds) || FD_ISSET(i, &exception_fds) ) {
          fd_limit = i + 1;
          break;
       }
    }
  }
}

static void
select_task(void * param)
{
  for ( ; ; ) {
    fd_set read_now;
    fd_set write_now;
    fd_set exception_now;
    struct timeval now;
    struct timeval min_time = { 0x7fffffff, 0 }; // Absurdly long time.
    int number_of_set_fds;


    gettimeofday(&now, 0);

    for ( unsigned int i = 0; i < NUMBER_OF_FDS; i++ ) {
      struct timeval * t = &timeouts[i];

      if ( timerisset(t) ) {
        struct timeval when;

        if ( timercmp(t, &now, >) ) {
          timerclear(&min_time);
          break; // Can't get lower than this, so no point in checking more values.
        }
        else {
          timersub(&now, t, &when);
        }
      
        if ( timercmp(&when, &min_time, <) )
          min_time = when;
      }
    }

    memcpy(&read_now, &read_fds, sizeof(read_now));
    memcpy(&write_now, &write_fds, sizeof(write_now));
    memcpy(&exception_now, &exception_fds, sizeof(exception_now));

    // `now` must be the time before select was called, so that
    number_of_set_fds = select(fd_limit, &read_now, &write_now, &exception_now, &min_time);

    if ( number_of_set_fds > 0 ) {
      for ( unsigned int i = 0; i < NUMBER_OF_FDS; i++ ) {
        struct timeval * t = &timeouts[i];
        bool readable = false;
        bool writable = false;
        bool exception = false;
        bool timeout = false;

        if ( FD_ISSET(i, &read_now) )
          readable = true;
        if ( FD_ISSET(i, &write_now) )
          writable = true;
        if ( FD_ISSET(i, &exception_now) )
          exception = true;
        if ( t->tv_sec > now.tv_sec || (t->tv_sec == now.tv_sec && t->tv_usec > now.tv_usec) ) {
          timeout = true;
          gm_fd_unregister(i);
        }

        if ( readable || writable || exception || timeout )
          (*handlers[i])(i, readable, writable, exception, timeout);
      }
    }
    else if ( number_of_set_fds == 0 ) {
      for ( unsigned int i = 0; i < NUMBER_OF_FDS; i++ ) {
        struct timeval * t = &timeouts[i];
        if ( t->tv_sec > now.tv_sec || (t->tv_sec == now.tv_sec && t->tv_usec > now.tv_usec) ) {
          gm_fd_unregister(i);
          (*handlers[i])(i, false, false, false, true);
        }
      }
    }
    else {
      // Select failed.
      fprintf(stderr, "Select failed with error: %s\n", strerror(errno));
    }
  }
}

void
gm_select_task(void)
{
  xTaskCreate(select_task, "select", 10240, NULL, 3, &select_task_id);
}
