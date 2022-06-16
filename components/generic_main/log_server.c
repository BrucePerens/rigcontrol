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

static int server = -1;

static void
logging_connection_closed()
{
  if ( GM.log_fd == 2 )
    return;

  gm_fd_unregister(GM.log_fd);
  fclose(GM.log_file_pointer);
  GM.log_fd = 2;
  GM.log_file_pointer = stderr;
  gm_printf("Now logging to the console.\n");
}

static void
socket_event_handler(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( exception ) {
    logging_connection_closed();
    return;
  }
 
  if ( readable ) {
    char	buffer[64];
    int		result;

    if ( (result = recv(fd, buffer, sizeof(buffer), 0)) == 0 ) {
      logging_connection_closed();
    }
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
      GM_FAIL("Select event server accept failed: %s\n", strerror(errno));
      return;
    }
    gm_printf("Now logging to the telnet client rather than the console.\n");
    GM.log_fd = connection;
    GM.log_file_pointer = fdopen(GM.log_fd, "a");
    setlinebuf(GM.log_file_pointer);
    gm_printf("Logging to the telnet client initiated.\n");
    gm_fd_register(connection, socket_event_handler, 0, true, false, true, 0);
  }
  if ( exception ) {
    GM_FAIL("Exception on event socket.\n");
  }
}

void
gm_log_server_start(void)
{
  struct sockaddr_in address = {};

  server = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = GM.sta.ip4.address.sin_addr.s_addr;
  address.sin_port = htons(23);

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
  gm_printf("Log server waiting for connection.\n");
}

void
gm_log_server_stop(void)
{
  logging_connection_closed();
  if ( server >= 0 ) {
    gm_fd_unregister(server);
    close(server);
    server = -1;
  }
}
