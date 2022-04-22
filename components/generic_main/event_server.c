#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "generic_main.h"

// These are events that are delivered via gm_fd_register(), which is the interface to
// a select() loop.
//
// gm_select_task() depends on this to wake up the select() when there is a new file descriptor to monitor.
// Having this also allows us to use one task to handle events, instead of one for select-based events
// and one for FreeRTOS event registration system events. We still have the high priority FreeRTOS system
// event loop, but we don't have to make a low-priority FreeRTOS user event loop.
//
// This is greater overhead than it should be, because the FreeRTOS-lwIP combination doesn't provide
// pipes or Unix-domain sockets, and FreeRTOS queues, the FreeRTOS equivalent of pipes, don't have a
// file descriptor to use with select(). I can write a pipe driver using the ESP Virtual Filesystem,
// but haven't done so yet.

enum gm_event_opcode {
  GM_EVENT_INVALID = 0,		// For catching incorrect initialization.
  GM_EVENT_WAKE_SELECT = 1,	// Wake up the select() in the select task, because the FDs have changed.
  GM_EVENT_RUN = 2		// Call a function in the context of the select task.
};

struct gm_event {
  uint32_t	operation;
  uint32_t	size;
  union {
    struct {
      gm_run_t function;
      void * data;
    } call;
  } data;
};

static int	client = -1;

static void
event_handler(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  char			buffer[128];
  struct gm_event *	event = (struct gm_event *)buffer;

  fprintf(stderr, "In event handler\n");

  int	result = read(fd, buffer, 8);

  if ( result != 8 ) {
    fprintf(stderr, "Event header read returned %d\n", result);
    return;
  }

  if ( event->size > 0 ) {
    if ( event->size > sizeof(buffer) - 8 ) {
      fprintf(stderr, "size of event data too large: %d\n", event->size);
      return;
    }
    result = read(fd, &event->data, event->size);

    if ( result != event->size ) {
      fprintf(stderr, "Event data read returned %d, error: %s\n", result, strerror(errno));
    }

  }
  switch ( event->operation ) {
  case GM_EVENT_INVALID:
  default:
    fprintf(stderr, "Invalid event opcode %d\n", result);
    break;
  case GM_EVENT_WAKE_SELECT:
    // If we get here, the select has already awakened, there's no need to do any more.
    fprintf(stderr, "Woke up the select().\n");
    break;
  case GM_EVENT_RUN:
    fprintf(stderr, "Calling function.\n");
    (event->data.call.function)(event->data.call.data);
    break;
  }
}


static void
accept_handler(int sock, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  struct sockaddr_in	client_address;
  socklen_t		size;
  int			connection;

  fprintf(stderr, "In accept handler.\n");
  if ( readable ) {
    size = sizeof(client_address);
    if ( (connection = accept(sock, (struct sockaddr *)&client_address, &size)) < 0 ) {
      fprintf(stderr, "Select event server listen failed: %s\n", strerror(errno));
      gm_fd_unregister(sock);
      close(sock);
      return;
    }
    gm_fd_register(connection, event_handler, 0, true, false, true, 0);
  }
  if ( exception ) {
    fprintf(stderr, "Exception on event socket.\n");
    gm_fd_unregister(sock);
    close(sock);
  }
}

void
gm_event_server(void)
{
  struct sockaddr_in address = {};
  int server = socket(AF_INET, SOCK_STREAM, 0);

  client = socket(AF_INET, SOCK_STREAM, 0);

  if ( server < 0 ) {
    fprintf(stderr, "Select event server could not create a socket: %s\n", strerror(errno));
    return;
  }
  
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1"); 
  address.sin_port = 2139; // Private port not expected to be used by other code.

  if ( bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ) {
    fprintf(stderr, "Select event server bind failed: %s\n", strerror(errno));
    return;
  }

  if ( listen(server, 2) != 0 ) {
    fprintf(stderr, "Select event server listen failed: %s\n", strerror(errno));
    close(server);
    return;
  }

  gm_fd_register(server, accept_handler, 0, true, false, true, 0);

  // This blocks until the select task is running.
  if ( connect(client, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    fprintf(stderr, "Connect failed: %s\n", strerror(errno));
  }
}

void
gm_select_wakeup(void)
{
  struct gm_event	event = {};
  if ( client > 0 ) {
    event.operation = GM_EVENT_WAKE_SELECT;
    event.size = 0;
    if ( write(client, &event, 8) != 8 )
      fprintf(stderr, "gm_select_wakeup() write failed: %s\n", strerror(errno));
  }
}

// Run a function in the context of the select task. It must not block.
void
gm_fast_run(gm_run_t * function)
{
  struct gm_event	event = {};
  if ( client > 0 ) {
    event.operation = GM_EVENT_RUN;
    event.size = 0;
    write(client, &event, sizeof(event));
    if ( write(client, &event, sizeof(event)) != sizeof(event) )
      fprintf(stderr, "gm_run() write failed: %s\n", strerror(errno));
  }
}
