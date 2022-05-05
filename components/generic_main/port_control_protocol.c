#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
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

const uint8_t pcp_ipv4_broadcast_address[4] = { 221, 0, 0, 4 };
const uint8_t pcp_ipv6_broadcast_address[8] = { 0xff, 0x02, 0, 0, 0, 0, 0, 0x01 };

enum pcp_version {
  NAT_PMP = 0,
  PORT_MAPPING_PROTOCOL = 2
};

// MAP and PEER are sent from any port on the host to 5351 on the router.
// ANNOUNCE is broadcast from 5351 on the router to 5350 on a host.
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
    memcpy(&a6->sin6_addr.s6_addr, GM.sta.ip6.router.sin6_addr.s6_addr, sizeof(a6->sin6_addr.s6_addr));
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

  send_packet.version = PORT_MAPPING_PROTOCOL;
  send_packet.opcode = PCP_MAP;
  send_packet.pcp.lifetime = htonl(24 * 60 * 60);
  memcpy(send_packet.pcp.mp.nonce, m->nonce, sizeof(m->nonce));
  send_packet.pcp.mp.protocol = m->tcp ? PCP_TCP : PCP_UDP;
  send_packet.pcp.mp.internal_port = htons(m->internal_port);
  send_packet.pcp.mp.external_port = htons(m->external_port);
  send_packet.pcp.lifetime = htonl(m->lifetime);
  memcpy(send_packet.pcp.mp.external_address.s6_addr, m->external_address.s6_addr, sizeof(m->external_address.s6_addr));

  int sock = socket(send_address.ss_family, SOCK_DGRAM, ip_protocol);

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

  // Receive the reply from the gateway, or not.
  receive_result = recvfrom(
   sock,
   &receive_packet,
   sizeof(receive_packet),
   0,
   (struct sockaddr *)&receive_address,
   &receive_address_size);

  if ( receive_result >= map_packet_size ) {
    if ( memcmp(receive_packet.pcp.mp.nonce, send_packet.pcp.mp.nonce, sizeof(receive_packet.pcp.mp.nonce)) != 0 ) {
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
  if ( p )
    *p = m;
  return 0;
}
