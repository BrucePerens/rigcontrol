#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <netinet/in.h>
#include "generic_main.h"

struct pcp {
  uint8_t	version;
  uint8_t	r:1;
  uint8_t	opcode:7;
  union {
    struct pcp_request {
      uint16_t	reserved;
      uint32_t	lifetime;
      uint32_t	client_ip_address[4];
    } request;
    struct pcp_response {
      uint8_t	reserved;
      uint32_t	result_code;
      uint32_t	lifetime;
      uint32_t	epoch_time;
      uint32_t	reserved1[3];
    } response;
  };
  union {
    struct pcp_map_peer {
      // For MAP or PEER opcode.
      uint32_t	nonce;
      uint8_t	protocol;
      uint8_t	reserved[3];
      uint16_t	internal_port;
      uint16_t	external_port;
      uint32_t	external_ip_address[4];
      // For PEER opcode only.
      uint16_t	remote_peer_port;
      uint16_t	reserved1;
      uint32_t	remote_peer_address[4];
    } mp;
  };
};

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
  PCP_UPD = 17
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

int pcp(bool ipv6)
{
  struct pcp			packet[1100];
  struct sockaddr_storage	address;
  struct sockaddr_storage	receive_address;
  int				ip_protocol;
  ssize_t			address_size;
  socklen_t			receive_size = 0;

  if ( !ipv6 ) {
    address_size = sizeof(struct sockaddr_in);
    struct sockaddr_in * a4 = (struct sockaddr_in *)&address;
    a4->sin_addr.s_addr = GM.sta.ip_info.gw.addr;
    a4->sin_family = AF_INET;
    a4->sin_port = htons(PCP_PORT);
    ip_protocol = IPPROTO_IP;
    
  }
  else {
    address_size = sizeof(struct sockaddr_in6);
    struct sockaddr_in6 * a6 = (struct sockaddr_in6 *)&address;
    memcpy(&a6->sin6_addr.un, GM.sta.router_ip6.addr, sizeof(a6->sin6_addr.un));
    a6->sin6_family = AF_INET6;
    a6->sin6_port = htons(PCP_PORT);
    a6->sin6_scope_id = esp_netif_get_netif_impl_index(GM.sta.esp_netif);
    ip_protocol = IPPROTO_IPV6;
  }

  int sock = socket(address.ss_family, SOCK_DGRAM, ip_protocol);

  ssize_t result = sendto(
   sock,
   packet,
   sizeof(packet),
   0,
   (const struct sockaddr *)&address,
   (socklen_t)address_size);

   static int timeout = 5 * 1000;
   setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));

  result = recvfrom(
   sock,
   packet,
   sizeof(packet),
   0,
   (struct sockaddr *)&receive_address,
   &receive_size);
  return 0;
}
