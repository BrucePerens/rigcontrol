#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define NUMBER_OF_FDS 100

typedef void (*gm_fd_handler_t)(int fd, bool readable, bool writable, bool exception);

static TaskHandle_t system_task_id = NULL;

static fd_set read_fds = {};
static fd_set write_fds = {};
static fd_set exception_fds = {};
static int nfds = 0;
gm_fd_handler_t handlers[NUMBER_OF_FDS];
time_t timeouts[NUMBER_OF_FDS];


void
gm_register_fd(int fd, gm_fd_handler_t handler, bool readable, bool writable, bool exception, uint32_t seconds) {
}

void
gm_unregister_fd(int fd, bool readable, bool writable, bool exception) {
}

static void
system_task(void * param)
{
  for ( ; ; ) {
    fd_set read_now;
    fd_set write_now;
    fd_set exception_now;
    struct timeval tv = {};
    int ready_fd_count;

    tv.tv_sec = 60;
    tv.tv_usec = 0;

    memcpy(&read_now, &read_fds, sizeof(read_now));
    memcpy(&write_now, &write_fds, sizeof(write_now));
    memcpy(&exception_now, &exception_fds, sizeof(exception_now));

    ready_fd_count = select(nfds, &read_now, &write_now, &exception_now, &tv);

    if ( ready_fd_count > 0 ) {
    }
  }
}

void
gm_start_system_task(void)
{
  xTaskCreate(system_task, "system", 10240, NULL, 3, &system_task_id);
}
