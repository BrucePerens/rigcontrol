#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <arpa/inet.h>
#include "generic_main.h"

struct nat_pmp_or_pcp {
  uint8_t	version;
  uint8_t	opcode; // Also contains the request/response bit.
  uint8_t	reserved;
  uint8_t	result_code;
  union {
    struct nat_pmp_packet {
      struct nat_pmp_request {
        uint16_t	internal_port;
        uint16_t	external_port;
        uint32_t	lifetime;
      } request;
      struct nat_pmp_response {
        uint32_t	epoch;
        uint16_t	internal_port;
        uint16_t	external_port;
        uint32_t	lifetime;
      } response;
    } nat_pmp;
    struct pcp_packet {
      uint32_t	lifetime;
      union {
        struct pcp_request {
          struct in6_addr client_address;
        } request;
        struct pcp_response {
          uint32_t	epoch;
          uint32_t	reserved1[3];
        } response;
      };
      union {
        struct pcp_map_peer {
          // For MAP or PEER opcode.
          uint32_t	nonce[3];
          uint8_t	protocol;
          uint8_t	reserved[3];
          uint16_t	internal_port;
          uint16_t	external_port;
          struct in6_addr external_address;
          // For PEER opcode only.
          uint16_t	remote_peer_port;
          uint16_t	reserved1;
          struct in6_addr	remote_peer_address;
        } mp;
      };
    } pcp;
  };
};
const size_t map_packet_size = (size_t)&(((struct nat_pmp_or_pcp *)0)->pcp.mp.remote_peer_port);

enum pcp_version {
  NAT_PMP = 0,
  PORT_MAPPING_PROTOCOL = 2
};

// MAP and PEER are sent from any port on the host to 5351 on the router.
// ANNOUNCE is multicast from 5351 on the router to 5350 on a host.
enum pcp_port {
  PCP_PORT = 5351,
  PCP_ANNOUNCE_PORT = 5350
};
const size_t	PCP_MAX_PAYLOAD = 1100;

enum pcp_opcode {
  PCP_ANNOUNCE = 0,
  PCP_MAP = 1,
  PCP_PEER = 2
};

enum nat_pmp_opcode {
  NAT_PMP_ANNOUNCE = 0,
  NAT_PMP_MAP_UDP = 1,
  NAT_PMP_MAP_TCP = 2
};

enum pcp_protocol {
  PCP_TCP = 6,
  PCP_UDP = 17
};

enum nat_pmp_response_code {
  NAT_PMP_SUCCESS = 0,
  NAT_PMP_UNSUPP_VERSION = 1,
  NAT_PMP_NOT_AUTHORIZED = 2,
  NAT_PMP_NETWORK_FAILURE = 3,
  NAT_PMP_OUT_OF_RESOURCES = 4,
  NAT_PMP_UNSUPPORTED_OPCODE = 5
};

// Ugh. The PCP response codes really should have included the first 5 NAT-PMP
// response codes with the same values. They just include the first 3.
enum pcp_response_code {
  PCP_SUCCESS = 0,
  PCP_UNSUPP_VERSION = 1,
  PCP_NOT_AUTHORIZED = 2,
  PCP_MALFORMED_REQUEST = 3,
  PCP_UNSUPP_OPCODE = 4,
  PCP_UNSUPP_OPTION = 5,
  PCP_MALFORMED_OPTION = 6,
  PCP_NETWORK_FAILURE = 7,
  PCP_NO_RESOURCES = 8,
  PCP_UNSUPP_PROTOCOL = 9,
  PCP_USER_EX_QUOTA = 10,
  PCP_CANNOT_PROVIDE_EXTERNAL = 11,
  PCP_ADDRESS_MISMATCH = 12,
  PCP_EXCESSIVE_REMOTE_PEERS = 13
};

int gm_port_control_protocol(gm_port_mapping_t * m)
{
  struct nat_pmp_or_pcp		send_packet = {};
  struct nat_pmp_or_pcp		receive_packet = {};
  struct sockaddr_storage	send_address = {};
  struct sockaddr_storage	receive_address = {};
  int				ip_protocol;
  socklen_t			send_address_size;
  socklen_t			receive_address_size;
  ssize_t			send_result;
  ssize_t			receive_result;
  struct timeval		timeout = {};
  gm_port_mapping_t * *		p;
  char				buffer[INET6_ADDRSTRLEN + 1];

  if ( !m->ipv6 ) {
    send_address_size = sizeof(struct sockaddr_in);
    struct sockaddr_in * a4 = (struct sockaddr_in *)&send_address;
    a4->sin_addr.s_addr = GM.sta.ip4.router.sin_addr.s_addr;
    a4->sin_family = AF_INET;
    a4->sin_port = htons(PCP_PORT);
    ip_protocol = IPPROTO_IP;
  }
  else {
    send_address_size = sizeof(struct sockaddr_in6);
    struct sockaddr_in6 * a6 = (struct sockaddr_in6 *)&send_address;
    // This doesn't nornally get called until the station has a global address.
    memcpy(&a6->sin6_addr.s6_addr, GM.sta.ip6.global[0].sin6_addr.s6_addr, sizeof(a6->sin6_addr.s6_addr));
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(PCP_PORT);
    a6->sin6_scope_id = esp_netif_get_netif_impl_index(GM.sta.esp_netif);
    ip_protocol = IPPROTO_IPV6;
  }

  if ( !m->ipv6 ) {
    send_packet.pcp.request.client_address.s6_addr[10] = 0xff;
    send_packet.pcp.request.client_address.s6_addr[11] = 0xff;
    memcpy(&send_packet.pcp.request.client_address.s6_addr[12], &GM.sta.ip4.address.sin_addr.s_addr, 4);
  }
  else {
    struct sockaddr_in6 * a6 = (struct sockaddr_in6 *)&send_address;
    memcpy(send_packet.pcp.request.client_address.s6_addr, &a6->sin6_addr.s6_addr, sizeof(a6->sin6_addr.s6_addr));
  }

  memcpy(send_packet.pcp.mp.external_address.s6_addr, m->external_address.s6_addr, sizeof(m->external_address.s6_addr));
  memset(buffer, '\0', sizeof(buffer));
  inet_ntop(AF_INET6, &send_packet.pcp.mp.external_address.s6_addr, buffer, sizeof(buffer));
  gm_printf("Requested address %s\n", buffer);
  send_packet.version = PORT_MAPPING_PROTOCOL;
  send_packet.opcode = PCP_MAP;
  send_packet.pcp.lifetime = htonl(24 * 60 * 60);
  memcpy(send_packet.pcp.mp.nonce, m->nonce, sizeof(m->nonce));
  send_packet.pcp.mp.protocol = m->tcp ? PCP_TCP : PCP_UDP;
  send_packet.pcp.mp.internal_port = htons(m->internal_port);
  send_packet.pcp.mp.external_port = htons(m->external_port);
  send_packet.pcp.lifetime = htonl(m->lifetime);

  int sock = socket(send_address.ss_family, SOCK_DGRAM, IPPROTO_UDP);

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout, sizeof(timeout));

  // The size of the packet does not include the portion used for the PEER operation.
  // We have to trim any possible option segments from the packet, or populate them.

  // Send the packet to the gateway.
  send_result = sendto(
   sock,
   &send_packet,
   map_packet_size,
   0,
   (const struct sockaddr *)&send_address,
   send_address_size);

  if ( send_result < map_packet_size ) {
    gm_printf("Send returned %d\n", send_result);
    return -1;
  }

  receive_address_size = sizeof(struct sockaddr_in6);

  // FIX: This needs bind(). The IPv6 version doesn't work without it.

  // Receive the reply from the gateway, or not.
  receive_result = recvfrom(
   sock,
   &receive_packet,
   sizeof(receive_packet),
   0,
   (struct sockaddr *)&receive_address,
   &receive_address_size);

  if ( receive_result >= map_packet_size ) {
    if ( memcmp(receive_packet.pcp.mp.nonce, send_packet.pcp.mp.nonce, sizeof(receive_packet.pcp.mp.nonce)) < 0 ) {
      gm_printf("Received nonce isn't equal to transmitted one.\n");
      return -1;
    }
    if ( receive_packet.result_code != PCP_SUCCESS ) {
      gm_printf("PCP received result code: %d.\n", receive_packet.result_code);
      return -1;
    }
    if ( receive_packet.opcode != (send_packet.opcode | 0x80) ) {
      gm_printf("Received opcode: %x.\n", receive_packet.opcode);
      return -1;
    }
  }
  else {
    gm_printf("Received result too small.\n");
    return -1;
  }

  gettimeofday(&m->granted_time, 0);
  memcpy(m->nonce, receive_packet.pcp.mp.nonce, sizeof(m->nonce));
  m->epoch = receive_packet.pcp.response.epoch;
  m->ipv6 = ip_protocol == IPPROTO_IPV6;
  m->tcp = receive_packet.pcp.mp.protocol == PCP_TCP;
  m->internal_port = ntohs(receive_packet.pcp.mp.internal_port);
  m->external_port = ntohs(receive_packet.pcp.mp.external_port);
  m->lifetime = ntohl(receive_packet.pcp.lifetime);
  memcpy(m->external_address.s6_addr, receive_packet.pcp.mp.external_address.s6_addr, sizeof(m->external_address.s6_addr));

  inet_ntop(AF_INET6, m->external_address.s6_addr, buffer, sizeof(buffer));
  gm_printf("Router public mapping %s:%d\n", buffer, m->external_port);

  if ( m->ipv6 )
    p = &GM.sta.ip6.port_mappings;
  else
    p = &GM.sta.ip4.port_mappings;
  while ( *p != 0 ) {
    if ( *p == m ) {
      p = 0;
      break;
    }
    p = &((*p)->next);
  }
  if ( p ) {
    gm_port_mapping_t * n = malloc(sizeof(*n));
    memcpy(n, m, sizeof(*n));
    *p = n;
  }
  return 0;
}

static void
decode_pcp_announce(struct nat_pmp_or_pcp * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
}

static void
decode_pcp_map(struct nat_pmp_or_pcp * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
}

static void
decode_pcp_peer(struct nat_pmp_or_pcp * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
}

void
decode_packet(struct nat_pmp_or_pcp * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  char		buffer[INET_ADDRSTRLEN + 1];
  void *	a;
  bool		response;
  uint16_t	port;

  switch ( address->ss_family ) {
  case AF_INET:
    a = &((struct sockaddr_in *)address)->sin_addr;
    port = ((struct sockaddr_in *)address)->sin_port;
    break;
  case AF_INET6:
    a = &((struct sockaddr_in6 *)address)->sin6_addr;
    port = ((struct sockaddr_in6 *)address)->sin6_port;
    break;
  default:
    gm_printf("decode_packet(): Address family %d.\n", address->ss_family);
    return;
  }

  inet_ntop(address->ss_family, a, buffer, sizeof(buffer));
  gm_printf(
   "Received %s %s with opcode %x from %s port %d.\n",
   multicast ? "multicast" : "unicast",
   p->opcode & 0x80 ? "response" : "request",
   p->opcode & 0x7f,
   buffer,
   port
  );

  if ( port != PCP_PORT ) {
    gm_printf("Ignoring message that isn't from the PCP port.\n");
    return;
  }

  switch ( address->ss_family ) {
  case AF_INET:
    if ( ((struct sockaddr_in *)address)->sin_addr.s_addr != GM.sta.ip4.router.sin_addr.s_addr ) {
      gm_printf("IPV4 packet not from the router, ignored.\n");
      return;
    }
    break;
  case AF_INET6:
    break;
  }

  if ( message_size < map_packet_size ) {
    gm_printf("Receive packet too small: %d\n", message_size);
  }
  response = p->opcode & 0x80;
  
  switch ( p->opcode & 0x7f ) {
  case PCP_ANNOUNCE:
    decode_pcp_announce(p, message_size, multicast, address);
    break;
  case PCP_MAP:
    if ( !response ) {
      gm_printf("Client received PCP_MAP request.\n");
      return;
    }
    decode_pcp_map(p, message_size, multicast, address);
    break;
  case PCP_PEER:
    if ( !response ) {
      gm_printf("Client received PCP_PEER request.\n");
      return;
    }
    decode_pcp_peer(p, message_size, multicast, address);
    break;
  default:
    gm_printf("Unrecognized opcode %x\n", p->opcode);
  }
}

static void
incoming_packet(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( readable ) {
    struct nat_pmp_or_pcp	packet;
    struct sockaddr_storage	address;
    socklen_t			address_size;
    ssize_t			message_size;

    message_size = recvfrom(fd, &packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
    // Data is set to 1 for multicast, 0 for unicast.
    decode_packet(&packet, message_size, (int)data != 0, &address);
  }
}

static void
start_multicast_listener_ipv4(void)
{
  int			value = 1;
  struct ip_mreq	multi_request = {};
  struct sockaddr_in	address = {};
  int			sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // IPv4 "all hosts" multicast.
  // inet_pton(AF_INET, "224.0.0.1", &multi_request.imr_multiaddr);
  inet_pton(AF_INET, "224.0.0.1", &multi_request.imr_multiaddr);
  multi_request.imr_interface.s_addr = GM.sta.ip4.address.sin_addr.s_addr;

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = multi_request.imr_multiaddr.s_addr;
  address.sin_port = htons(PCP_ANNOUNCE_PORT);

  // Reuse addresses, because other software listens for all-hosts multicast.
  if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0 ) {
    gm_printf("Setsockopt SOL_SOCKET failed: %s.\n", strerror(errno));
    return;
  }
  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    gm_printf("bind failed.\n");
    return;
  }

  if ( setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multi_request, sizeof(multi_request)) < 0 ) {
    // This fails because the host is already registered to the "all-hosts" multicast
    // group. Ignore that.
    if ( errno != EADDRNOTAVAIL ) {
      gm_printf("Setsockopt IP_ADD_MEMBERSHIP failed: %s.\n", strerror(errno));
      return;
    }
  }
  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(sock, incoming_packet, (void *)1, true, false, true, 0);
}

static void
start_unicast_listener_ipv4(void)
{
  struct sockaddr_in	address = {};
  int			sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = GM.sta.ip4.address.sin_addr.s_addr;
  address.sin_port = htons(PCP_PORT);

  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    gm_printf("bind failed.\n");
    return;
  }

  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(sock, incoming_packet, (void *)0, true, false, true, 0);
}

// At this writing, May 2022, OpenWRT is not configured by default to forward multicasts
// across its subnets.
static void
start_multicast_listener_ipv6(void)
{
  int			value = 1;
  struct ipv6_mreq	multi_request = {};
  struct sockaddr_in6	address = {};
  int			sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

  // IPv6 "all hosts" multicast.
  inet_pton(AF_INET6, "ff02::1", &multi_request.ipv6mr_multiaddr);
  multi_request.ipv6mr_interface = esp_netif_get_netif_impl_index(GM.sta.esp_netif);

  memcpy(&address.sin6_addr, &multi_request.ipv6mr_multiaddr, sizeof(multi_request.ipv6mr_multiaddr));
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(PCP_ANNOUNCE_PORT);

  // Reuse addresses, because other software listens for all-hosts multicast.
  if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0 ) {
    gm_printf("Setsockopt SOL_SOCKET failed.\n");
    return;
  }
  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    gm_printf("ipv6 bind failed: %s.\n", strerror(errno));
    return;
  }

  if ( setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &multi_request, sizeof(multi_request)) < 0 ) {
    gm_printf("Setsockopt IPV6_JOIN_GROUP failed: %s.\n", strerror(errno));
    return;
  }
  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(sock, incoming_packet, (void *)1, true, false, true, 0);
}

static void
start_unicast_listener_ipv6(void)
{
  struct sockaddr_in6	address = {};
  int			sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

  address.sin6_family = AF_INET6;
  memcpy(&address.sin6_addr.s6_addr, &GM.sta.ip6.link_local.sin6_addr.s6_addr, sizeof(address.sin6_addr.s6_addr));
  address.sin6_port = htons(PCP_PORT);

  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    gm_printf("bind failed.\n");
    return;
  }

  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(sock, incoming_packet, (void *)0, true, false, true, 0);
}

void
gm_port_control_protocol_start_listener_ipv4(void)
{
  start_unicast_listener_ipv4();
  start_multicast_listener_ipv4();
}

void
gm_port_control_protocol_start_listener_ipv6(void)
{
  start_unicast_listener_ipv6();
  start_multicast_listener_ipv6();
}
