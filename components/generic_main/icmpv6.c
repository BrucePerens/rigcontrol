#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <arpa/inet.h>
#include "generic_main.h"

static int icmpv6_socket = -1;

typedef struct _icmpv6_message {
  uint8_t	headers[40]; // IPv6 header, to be filled in.
  uint8_t	type;
  uint8_t	code;
  uint16_t	checksum;
} icmpv6_message_t;

static void
decode_packet(void * data, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  icmpv6_message_t * m = (icmpv6_message_t *)data;
  gm_printf("ICMPv6 message type: %d, code: %d.\n", m->type, m->code);
}

static void
incoming_packet(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( readable ) {
    uint8_t			packet[2048];
    struct sockaddr_storage	address;
    socklen_t			address_size;
    ssize_t			message_size;

    message_size = recvfrom(fd, &packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
    // Data is set to 1 for multicast, 0 for unicast.
    decode_packet(&packet, message_size, (int)data != 0, &address);
  }
}

void
gm_icmpv6_start_listener_ipv6(void)
{
  icmpv6_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if ( icmpv6_socket < 0 ) {
    gm_printf("Socket creation failed: %s.\n", strerror(errno));
  }

  gm_printf("Start ICMPv6 listener.\n");

  gm_fd_register(icmpv6_socket, incoming_packet, 0, true, false, true, 0);
}
