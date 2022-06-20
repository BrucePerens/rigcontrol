#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <arpa/inet.h>
#include "generic_main.h"

typedef struct _nat_pmp_or_pcp {
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
} nat_pmp_or_pcp_t;
typedef struct _last_request {
  struct sockaddr_storage	address;
  nat_pmp_or_pcp_t		packet;
  struct timeval		time;
  bool				valid;
} last_request_t;

const size_t map_packet_size = (size_t)&(((nat_pmp_or_pcp_t *)0)->pcp.mp.remote_peer_port);
const size_t announce_packet_size = (size_t)&(((nat_pmp_or_pcp_t *)0)->pcp.mp);

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

static last_request_t	last_request_ipv4;
static last_request_t	last_request_ipv6;
static int		ipv4_sock = -1;
static int		ipv6_sock = -1;

static int
request_mapping_ipv4(gm_port_mapping_t * m)
{
  last_request_t *		r = &last_request_ipv4;
  nat_pmp_or_pcp_t *		p = &r->packet;
  socklen_t			send_address_size;
  ssize_t			send_result;
  struct sockaddr_in * 		a4 = (struct sockaddr_in *)&r->address;

  memset(r, '\0', sizeof(last_request_ipv4));
  r->valid = true;

  send_address_size = sizeof(struct sockaddr_in);
  a4->sin_addr.s_addr = GM.sta.ip4.router.sin_addr.s_addr;
  a4->sin_family = AF_INET;
  a4->sin_port = htons(PCP_PORT);
  p->pcp.request.client_address.s6_addr[10] = 0xff;
  p->pcp.request.client_address.s6_addr[11] = 0xff;
  memcpy(&p->pcp.request.client_address.s6_addr[12], &GM.sta.ip4.address.sin_addr.s_addr, 4);

  // This sets the requested address to the all-zeroes IPV4 address: ::ffff:0.0.0.0 .
  // if external_address was all zeroes, it would be the all-zeroes IPV6 address.
  // Some hosts would take that to mean a request for an IPV6-to-IPV4 port mapping.
  p->pcp.mp.external_address.s6_addr[10] = 0xff;
  p->pcp.mp.external_address.s6_addr[11] = 0xff;

  p->version = PORT_MAPPING_PROTOCOL;
  p->opcode = PCP_MAP;
  p->pcp.lifetime = htonl(24 * 60 * 60);
  memcpy(p->pcp.mp.nonce, m->nonce, sizeof(m->nonce));
  p->pcp.mp.protocol = m->tcp ? PCP_TCP : PCP_UDP;
  p->pcp.mp.internal_port = htons(m->internal_port);
  p->pcp.mp.external_port = htons(m->external_port);
  p->pcp.lifetime = htonl(m->lifetime);

  // Send the packet to the gateway.
  send_result = sendto(
   ipv4_sock,
   p,
   map_packet_size,
   0,
   (const struct sockaddr *)a4,
   send_address_size);
  gettimeofday(&r->time, 0);

  if ( send_result < map_packet_size ) {
    GM_FAIL("Send returned %d\n", send_result);
    return -1;
  }
  return 0;
}

int
gm_port_control_protocol_request_mapping_ipv4()
{
  gm_port_mapping_t m = {};

  esp_fill_random(&m.nonce, sizeof(m.nonce));
  m.ipv6 = false;
  m.tcp = true;
  m.internal_port = m.external_port = 8080;
  m.lifetime = 24 * 60 * 60;
  return request_mapping_ipv4(&m);
}

int request_mapping_ipv6(gm_port_mapping_t * m)
{
  last_request_t *		r = &last_request_ipv6;
  nat_pmp_or_pcp_t *		p = &r->packet;
  socklen_t			send_address_size;
  ssize_t			send_result;
  struct sockaddr_in6 * 	a6 = (struct sockaddr_in6 *)&r->address;
  char				buffer[INET6_ADDRSTRLEN + 1];

  memset(r, '\0', sizeof(last_request_ipv6));
  r->valid = true;

  send_address_size = sizeof(struct sockaddr_in6);
  memcpy(a6->sin6_addr.s6_addr, GM.sta.ip6.router.sin6_addr.s6_addr, sizeof(a6->sin6_addr.s6_addr));
  a6->sin6_family = AF_INET6;
  a6->sin6_port = htons(PCP_PORT);
  // FIX: What if there is no link-local address?
  memcpy(&p->pcp.request.client_address.s6_addr, &GM.sta.ip6.link_local.sin6_addr.s6_addr, sizeof(p->pcp.request.client_address.s6_addr));

  memset(buffer, '\0', sizeof(buffer));
  inet_ntop(a6->sin6_family, &a6->sin6_addr, buffer, sizeof(buffer));
  ; // gm_printf("Request IPv6 port mapping of router %s\n", buffer);
  p->version = PORT_MAPPING_PROTOCOL;
  p->opcode = PCP_MAP;
  p->pcp.lifetime = htonl(24 * 60 * 60);
  memcpy(p->pcp.mp.nonce, m->nonce, sizeof(m->nonce));
  p->pcp.mp.protocol = m->tcp ? PCP_TCP : PCP_UDP;
  // memcpy(p->pcp.mp.external_address.s6_addr, GM.sta.ip6.public.sin6_addr.s6_addr, sizeof(p->pcp.mp.external_address.s6_addr));
  p->pcp.mp.internal_port = htons(m->internal_port);
  p->pcp.mp.external_port = htons(m->external_port);
  p->pcp.lifetime = htonl(m->lifetime);

  // Send the packet to the gateway.
  send_result = sendto(
   ipv6_sock,
   p,
   map_packet_size,
   0,
   (const struct sockaddr *)a6,
   send_address_size);
  gettimeofday(&r->time, 0);

  if ( send_result < map_packet_size ) {
    GM_FAIL("Send returned %d\n", send_result);
    return -1;
  }
  return 0;
}

int
gm_port_control_protocol_request_mapping_ipv6()
{
  gm_port_mapping_t m = {};

  esp_fill_random(&m.nonce, sizeof(m.nonce));
  m.ipv6 = true;
  m.tcp = true;
  m.internal_port = m.external_port = 8080;
  m.lifetime = 24 * 60 * 60;
  return request_mapping_ipv6(&m);
}

static void
decode_pcp_announce(nat_pmp_or_pcp_t * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  ; // gm_printf("Received PCP Announce\n");
}

static void
decode_pcp_map(nat_pmp_or_pcp_t * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  // FIX: Manage re-authorization of existing mappings. Reject responses that are too
  // long after the request.
  last_request_t *	last;
  gm_port_mapping_t	m = {};
  gm_port_mapping_t * *	mp;
  esp_ip6_addr_t	esp_addr;
  esp_ip6_addr_type_t	ipv6_type;
  char			buffer[INET6_ADDRSTRLEN + 1];

  switch ( address->ss_family ) {
  case AF_INET:
    last = &last_request_ipv4;
    break;
  case AF_INET6:
    last = &last_request_ipv6;

    // esp-idf has its own IPv6 address structure.
    memset(&esp_addr, '\0', sizeof(esp_addr));
    memcpy(esp_addr.addr, p->pcp.mp.external_address.s6_addr, sizeof(esp_addr.addr));

    // Get the address type (global, link-local, etc.) for the IPv6 address.
    ipv6_type = esp_netif_ip6_get_addr_type(&esp_addr);

    if ( ipv6_type != ESP_IP6_ADDR_IS_GLOBAL ) {
      GM_WARN_ONCE("Warning: The router responded to a PCP map request with a useless mapping to an IPv6 %s address, instead of a global address. This is probably a MiniUPnPd bug.\n", GM.ipv6_address_types[ipv6_type]);
      return;
    }
    break;
  default:
    return;
  }

  if ( memcmp(p->pcp.mp.nonce, last->packet.pcp.mp.nonce, sizeof(p->pcp.mp.nonce)) < 0 ) {
    GM_FAIL("Received nonce isn't equal to transmitted one.\n");
    return;
  }
  if ( p->result_code != PCP_SUCCESS ) {
    GM_FAIL("PCP received result code: %d.\n", p->result_code);
    return;
  }
  if ( p->opcode != (last->packet.opcode | 0x80) ) {
    GM_FAIL("Received opcode: %x.\n", p->opcode);
    return;
  }

  memset(&m, '\0', sizeof(m));
  gettimeofday(&m.granted_time, 0);
  memcpy(m.nonce, p->pcp.mp.nonce, sizeof(m.nonce));
  m.epoch = p->pcp.response.epoch;
  m.ipv6 = address->ss_family == AF_INET6;
  m.tcp = p->pcp.mp.protocol == PCP_TCP;
  m.internal_port = ntohs(p->pcp.mp.internal_port);
  m.external_port = ntohs(p->pcp.mp.external_port);
  m.lifetime = ntohl(p->pcp.lifetime);
  memcpy(m.external_address.s6_addr, p->pcp.mp.external_address.s6_addr, sizeof(m.external_address.s6_addr));

  memset(buffer, '\0', sizeof(buffer));
  if ( address->ss_family == AF_INET6 )
    inet_ntop(AF_INET6, p->pcp.mp.external_address.s6_addr, buffer, sizeof(buffer));
  else
    inet_ntop(AF_INET, &p->pcp.mp.external_address.s6_addr[12], buffer, sizeof(buffer));
  ; // gm_printf("Router public mapping address: %s port: %d\n", buffer, m.external_port);
  if ( m.ipv6 )
    mp = &GM.sta.ip6.port_mappings;
  else
    mp = &GM.sta.ip4.port_mappings;
  while ( *mp != 0 )
    mp = &((*mp)->next);
  if ( mp ) {
    gm_port_mapping_t * n = malloc(sizeof(*n));
    memcpy(n, &m, sizeof(*n));
    *mp = n;
  }
}

static void
decode_pcp_peer(nat_pmp_or_pcp_t * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  ; // gm_printf("Received PCP Peer\n");
}

void
decode_packet(nat_pmp_or_pcp_t * p, ssize_t message_size, bool multicast, struct sockaddr_storage * address)
{
  bool		response;
  uint16_t	port;

  // FIX: This assumes all messages are PCP, it should handle NAT-PMP correctly.

  switch ( address->ss_family ) {
  case AF_INET:
    port = htons(((struct sockaddr_in *)address)->sin_port);
    break;
  case AF_INET6:
    port = htons(((struct sockaddr_in6 *)address)->sin6_port);
    break;
  default:
    GM_FAIL("decode_packet(): Address family %d.\n", address->ss_family);
    return;
  }

  if ( port != PCP_PORT ) {
    GM_FAIL("Ignoring message that isn't from the PCP port.\n");
    return;
  }

  switch ( address->ss_family ) {
  case AF_INET:
    if ( ((struct sockaddr_in *)address)->sin_addr.s_addr != GM.sta.ip4.router.sin_addr.s_addr ) {
      GM_FAIL("IPV4 packet not from the router, ignored.\n");
      return;
    }
    break;
  case AF_INET6:
    break;
  }


  response = p->opcode & 0x80;
  
  switch ( p->opcode & 0x7f ) {
  case PCP_ANNOUNCE:
    decode_pcp_announce(p, message_size, multicast, address);
    break;
  case PCP_MAP:
  case PCP_PEER:
    if ( !response ) {
      GM_FAIL("Client received request.\n");
      return;
    }
    if ( multicast ) {
      GM_FAIL("Client received multicast request.\n");
      return;
    }
    switch ( p->opcode & 0x7f ) {
    case PCP_MAP:
      if ( message_size < map_packet_size ) {
        GM_FAIL("Receive packet too small for MAP: %d\n", message_size);
        return;
      }
      decode_pcp_map(p, message_size, multicast, address);
      break;
    case PCP_PEER:
      if ( message_size < sizeof(nat_pmp_or_pcp_t) ) {
        GM_FAIL("Receive packet too small for PEER: %d\n", message_size);
        return;
      }
      decode_pcp_peer(p, message_size, multicast, address);
      break;
    }
    break;
  default:
    GM_FAIL("Unrecognized opcode %x\n", p->opcode);
  }
}

static void
incoming_packet(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  if ( readable ) {
    nat_pmp_or_pcp_t		packet;
    struct sockaddr_storage	address;
    socklen_t			address_size = sizeof(address);
    ssize_t			message_size;
    // int				port;
    char			buffer[INET6_ADDRSTRLEN + 1];

    message_size = recvfrom(fd, &packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *)&address, &address_size);
 
    if ( message_size <= 0 ) {
      GM_FAIL("recvfrom returned %d", message_size);
      return;
    }
    memset(buffer, '\0', sizeof(buffer));
    if ( address.ss_family == AF_INET ) {
      inet_ntop(AF_INET, &((struct sockaddr_in *)&address)->sin_addr.s_addr, buffer, sizeof(buffer));
      // port = htons(((struct sockaddr_in *)&address)->sin_port);
    }
    else {
      inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&address)->sin6_addr, buffer, sizeof(buffer));
      // port = htons(((struct sockaddr_in6 *)&address)->sin6_port);
    }

    ; // gm_printf("PCP received %s packet of size %d from %s port %d.\n",
     // data != 0 ? "multicast" : "unicast",
     // message_size,
     // buffer,
     // port);


    // Data is set to 1 for multicast, 0 for unicast.
    decode_packet(&packet, message_size, (int)data != 0, &address);
  }
}

static void
start_multicast_listener_ipv4(void)
{
  // FIX: Time-out responses and retry, eventually abandon trying.
  int			value = 1;
  struct ip_mreq	multi_request = {};
  struct sockaddr_in	address = {};
  int			sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // IPv4 "all hosts" multicast.
  inet_pton(AF_INET, "224.0.0.1", &multi_request.imr_multiaddr);
  multi_request.imr_interface.s_addr = GM.sta.ip4.address.sin_addr.s_addr;

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = multi_request.imr_multiaddr.s_addr;
  address.sin_port = htons(PCP_ANNOUNCE_PORT);

  // Reuse addresses, because other software listens for all-hosts multicast.
  if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0 ) {
    GM_FAIL("Setsockopt SOL_SOCKET failed: %s.\n", strerror(errno));
    return;
  }
  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    GM_FAIL("bind failed.\n");
    return;
  }

  if ( setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multi_request, sizeof(multi_request)) < 0 ) {
    // This fails because the host is already registered to the "all-hosts" multicast
    // group. Ignore that.
    if ( errno != EADDRNOTAVAIL ) {
      GM_FAIL("Setsockopt IP_ADD_MEMBERSHIP failed: %s.\n", strerror(errno));
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

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = GM.sta.ip4.address.sin_addr.s_addr;
  address.sin_port = htons(PCP_PORT);

  ipv4_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if ( bind(ipv4_sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    GM_FAIL("bind failed.\n");
    return;
  }

  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(ipv4_sock, incoming_packet, (void *)0, true, false, true, 0);
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
    GM_FAIL("Setsockopt SOL_SOCKET failed.\n");
    return;
  }
  if ( bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    GM_FAIL("bind failed");
    return;
  }

  if ( setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &multi_request, sizeof(multi_request)) < 0 ) {
    GM_FAIL("Setsockopt IPV6_JOIN_GROUP failed: %s.\n", strerror(errno));
    return;
  }
  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(sock, incoming_packet, (void *)1, true, false, true, 0);
}

static void
start_unicast_listener_ipv6(void)
{
  struct sockaddr_in6	address = {};

  address.sin6_family = AF_INET6;
  memcpy(&address.sin6_addr.s6_addr, &GM.sta.ip6.link_local.sin6_addr.s6_addr, sizeof(address.sin6_addr.s6_addr));
  address.sin6_port = htons(PCP_PORT);

  ipv6_sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if ( bind(ipv6_sock, (struct sockaddr *)&address, sizeof(address)) < 0 ) {
    GM_FAIL("bind failed.\n");
    return;
  }

  // Data is set to 1 for multicast, 0 for unicast.
  gm_fd_register(ipv6_sock, incoming_packet, (void *)0, true, false, true, 0);
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
