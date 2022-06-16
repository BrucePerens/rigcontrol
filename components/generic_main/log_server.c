#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "generic_main.h"

// Log server. This emits a lot to a stream socket, which you can connect with via
// telnet. Only one connection is supported. When you connect, logging is diverted
// from the console to the socket, and it is restored to the console when you disconnect.
// I wrote this to debug the Improv WiFi protocol, since logging to the same serial port
// that would be running the Improv protocol was problematic.

static void
socket_event_handler(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( exception ) {
    fclose(GM.log_file_pointer);
    GM.log_fd = 2;
    GM.log_file_pointer = stderr;
  }
  gm_printf("Logging to the console.\n");
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
    int flags = fcntl(connection, F_GETFL, 0);
    fcntl(connection, F_SETFL, flags | O_NONBLOCK);
    gm_printf("Now logging to the telnet client rather than the console.\n");
    GM.log_fd = connection;
    GM.log_file_pointer = fdopen(GM.log_fd, "a");
    gm_printf("Logging to the telnet client initiated.\n");
    gm_fd_register(connection, socket_event_handler, 0, true, false, true, 0);
  }
  if ( exception ) {
    GM_FAIL("Exception on event socket.\n");
  }
}

void
gm_log_server(void)
{
  struct sockaddr_in address = {};

  int server = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = 23;

  if ( bind(server, (struct sockaddr *)&address, sizeof(address)) != 0 ) {
    GM_FAIL("Log server bind failed: %s\n", strerror(errno));
    return;
  }

  if ( listen(server, 2) != 0 ) {
    GM_FAIL("Log server listen failed: %s\n", strerror(errno));
    close(server);
    return;
  }

  gm_fd_register(server, accept_handler, 0, true, false, true, 0);
}
