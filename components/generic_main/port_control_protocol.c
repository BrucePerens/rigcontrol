#include <stdint.h>

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

enum pcp_opcode {
  PCP_ANNOUNCE = 0,
  PCP_MAP = 1,
  PCP_PEER = 2
};

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
