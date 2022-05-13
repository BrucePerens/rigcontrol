#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <arpa/inet.h>
#include "generic_main.h"

static int icmpv6_socket = -1;

typedef struct _icmpv6_message {
  struct _ipv6_header {
    uint32_t	fields;
    uint16_t	payload_length;
    uint8_t	next_header;
    uint8_t	hop_limit;
    uint8_t	source_address[16];
    uint8_t	destination_address[16];
  } ipv6;
  uint8_t	type;
  uint8_t	code;
  uint16_t	checksum;
  struct _ra {
    uint8_t	current_hop_limit;
    uint8_t	flags;
    uint16_t	router_lifetime;
    uint32_t	reachable_time;
    uint32_t	retransmit_timer;
  } ra;
} icmpv6_message_t;

enum icmpv6_message_types {
  ICMPV6_ROUTER_ADVERTISEMENT = 134,
};

static void
decode_router_advertisement(icmpv6_message_t * m, ssize_t message_size, void * data, struct sockaddr_in6 * address)
{
  if ( !gm_all_zeroes(m->ipv6.source_address, sizeof(m->ipv6.source_address))
   && m->ra.router_lifetime > 0 ) {
    gm_ipv6_router_advertisement_after_t after = (gm_ipv6_router_advertisement_after_t)data;
    (*after)(address, m->ra.router_lifetime);
  }
}

static void
decode_packet(icmpv6_message_t * m, ssize_t message_size, void * data, struct sockaddr_in6 * address)
{
  switch ( m->type ) {
  case ICMPV6_ROUTER_ADVERTISEMENT:
    decode_router_advertisement(m, message_size, data, address);
  }
}

static void
incoming_packet(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( readable ) {
    icmpv6_message_t		packet;
    struct sockaddr_storage	address;
    socklen_t			address_size;
    ssize_t			message_size;

    message_size = recvfrom(fd, &packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
    if ( address.ss_family != AF_INET6 ) {
      char	buffer[INET6_ADDRSTRLEN];
      void *	a = ((struct sockaddr_in6 *)&address)->sin6_addr.s6_addr;

      if ( address.ss_family == AF_INET )
        a = &((struct sockaddr_in *)&address)->sin_addr.s_addr;

      inet_ntop(address.ss_family, a, buffer, sizeof(buffer));
      GM_FAIL("Packet sender wasn't an IPv6 address %.\n");
      return;
    }
    decode_packet(&packet, message_size, data, (struct sockaddr_in6 *)&address);
  }
}

void
gm_icmpv6_start_listener_ipv6 (gm_ipv6_router_advertisement_after_t after)
{
  icmpv6_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if ( icmpv6_socket < 0 ) {
    GM_FAIL("Socket creation failed: %s.\n", strerror(errno));
  }

  gm_fd_register(icmpv6_socket, incoming_packet, after, after, false, true, 0);
}
