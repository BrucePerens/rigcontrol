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

static void
process_connection(int fd)
{
}

void
gm_event_server(void)
{
  struct sockaddr_in address = {};
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  if ( sock < 0 ) {
    fprintf(stderr, "Select event server could not create a socket: %s\n", strerror(errno));
    return;
  }
  
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1"); 
  address.sin_port = 2139; // Private port not expected to be used by other code.

  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) != 0 ) {
    fprintf(stderr, "Select event server bind failed: %s\n", strerror(errno));
    return;
  }

  if ( listen(sock, 2) != 0 ) {
    fprintf(stderr, "Select event server listen failed: %s\n", strerror(errno));
    close(sock);
    return;
  }

  fprintf(stderr, "Select event server is listening.\n");
  for ( ; ; ) {
    struct sockaddr_in client_address = {};
    size_t size = sizeof(client_address);
    int connection;
  
    if ( (connection = accept(sock, (struct sockaddr *)&client_address, &size)) < 0 ) {
      fprintf(stderr, "Select event server listen failed: %s\n", strerror(errno));
      close(sock);
      return;
    }
    process_connection(conntection);
  }
}
