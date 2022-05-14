#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "generic_main.h"

// These are events that are delivered via gm_fd_register(), which is the
// interface to a select() loop. gm_select_task() depends on this to
// wake up the select() when there is a new file descriptor to monitor.
// Having this also allows us to use the select loop task to handle jobs
// (essentially, run coroutines).
// 
// Waking up the select() for something other than I/O is greater overhead
// than it should be, because the FreeRTOS-lwIP combination doesn't provide
// pipes or Unix-domain sockets, and FreeRTOS queues, the FreeRTOS equivalent
// of pipes, don't have a file descriptor to use with select(). So presently
// this implements a server on the loopback interface to listen for
// communication. I could write a pipe driver using the ESP Virtual Filesystem,
// but I haven't done so yet.

enum gm_event_opcode {
  GM_EVENT_INVALID = 0,		// For catching incorrect initialization.
  GM_EVENT_WAKE_SELECT = 1,	// Wake up the select() in the select task, because the FDs have changed.
  GM_EVENT_RUN = 2		// Call a procedure in the context of the select task.
};

typedef struct gm_event_t {
  uint32_t	operation;
  uint32_t	size;
  union {
    struct {
      gm_run_t procedure;
      void * data;
    } run;
  } data;
} gm_event_t;

static int	client = -1;

static void
event_handler(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  char		buffer[128];
  gm_event_t *	event = (gm_event_t *)buffer;

  int	result = read(fd, buffer, 8);

  if ( result != 8 ) {
    GM_FAIL("Event header read returned %d\n", result);
    return;
  }

  if ( event->size > 0 ) {
    if ( event->size > sizeof(buffer) - 8 ) {
      GM_FAIL("size of event data too large: %d\n", event->size);
      return;
    }
    result = read(fd, &event->data, event->size);

    if ( result != event->size ) {
      GM_FAIL("Event data read returned %d, error: %s\n", result, strerror(errno));
      return;
    }

  }
  switch ( event->operation ) {
  case GM_EVENT_INVALID:
  default:
    GM_FAIL("Invalid event opcode %d\n", result);
    break;
  case GM_EVENT_WAKE_SELECT:
    // If we get here, the select has already awakened, there's no need to do any more.
    break;
  case GM_EVENT_RUN:
    (event->data.run.procedure)(event->data.run.data);
    break;
  }
}


static void
accept_handler(int sock, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  struct sockaddr_in	client_address;
  socklen_t		size;
  int			connection;

  if ( readable ) {
    size = sizeof(client_address);
    if ( (connection = accept(sock, (struct sockaddr *)&client_address, &size)) < 0 ) {
      GM_FAIL("Select event server listen failed: %s\n", strerror(errno));
      return;
    }
    gm_fd_register(connection, event_handler, 0, true, false, true, 0);
  }
  if ( exception ) {
    GM_FAIL("Exception on event socket.\n");
  }
}

void
gm_event_server(void)
{
  struct sockaddr_in address = {};
  int server = socket(AF_INET, SOCK_STREAM, 0);

  client = socket(AF_INET, SOCK_STREAM, 0);

  if ( server < 0 ) {
    GM_FAIL("Select event server could not create a socket: %s\n", strerror(errno));
    return;
  }
  
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1"); 
  address.sin_port = 2139; // Private port not expected to be used by other code.

  if ( bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ) {
    GM_FAIL("Select event server bind failed: %s\n", strerror(errno));
    return;
  }

  if ( listen(server, 2) != 0 ) {
    GM_FAIL("Select event server listen failed: %s\n", strerror(errno));
    close(server);
    return;
  }

  gm_fd_register(server, accept_handler, 0, true, false, true, 0);

  // This blocks until the select task is running.
  if ( connect(client, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    GM_FAIL("Connect failed: %s\n", strerror(errno));
  }
}

void
gm_select_wakeup(void)
{
  gm_event_t	event = {};
  if ( client < 0 ) {
    GM_FAIL("gm_select_wakeup(): called before client was created.\n");
    abort();
  }
  event.operation = GM_EVENT_WAKE_SELECT;
  event.size = 0;
  if ( write(client, &event, 8) != 8 ) {
    GM_FAIL("gm_select_wakeup() write failed: %s\n", strerror(errno));
    abort();
  }
}

// Run a procedure in the context of the select task. It must not block.
void
gm_run(gm_run_t procedure, void * data, gm_run_speed_t speed)
{
  gm_event_t		event = {};
  gm_run_data_t 	run = {};

  if ( client < 0 ) {
    GM_FAIL("In gm_fast_run: client FD is < 0\n");
    abort();
  }

  switch ( speed ) {
  case GM_FAST:
    event.operation = GM_EVENT_RUN;
    event.size = sizeof(event.data.run);
    event.data.run.procedure = procedure;
    event.data.run.data = data;
    if ( write(client, &event, sizeof(event)) != sizeof(event) )
      GM_FAIL("gm_run(GM_FAST) write failed: %s\n", strerror(errno));
    break;
  case GM_MEDIUM:
    run.procedure = procedure;
    run.data = data;
    ESP_ERROR_CHECK(esp_event_post_to(&GM.medium_event_loop, GM_EVENT, GM_RUN, &run, sizeof(run), 0));
    break;
  case GM_SLOW:
    run.procedure = procedure;
    run.data = data;
    ESP_ERROR_CHECK(esp_event_post_to(&GM.slow_event_loop, GM_EVENT, GM_RUN, &run, sizeof(run), 0));
    break;
  }
}
