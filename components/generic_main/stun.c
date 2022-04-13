#include <stdint.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include "generic_main.h"

const uint32_t stun_magic = 0x2112A442;

// stun_message_class bits are interleaved into the type field.
// Where c is the message class and m is the method:
// htons(((c & 0x1) << 4) | ((c & 0x2) << 8) | (m & 0xf))
enum stun_message_class {
  STUN_REQUEST = 0,
  STUN_INDICATION = 1,
  STUN_RESPONSE = 2,
  STUN_ERROR = 3
};

// stun_methods are the low 4 bits of the type field, but there's only one of them
// defined.
enum stun_methods {
  STUN_BINDING = 1
};

struct stun_message {
  uint16_t type; // First two bits must be zero. Message type and class interleaved.
  uint16_t length; // Does not include the 20-byte header size. Always a multiple of 4.
  uint32_t magic_cookie;
  uint32_t transaction_id[3];
  uint32_t  attributes[]; // Variable length attributes, always padded to 32-bit boundary.
};

struct stun_attribute {
  uint16_t type;
  uint16_t length; // Length of the value part of the attribute, before padding.
  union {
    struct {
      uint8_t	must_be_zero;
      uint8_t	family; // 1 = ipv4, 2 = ipv6.
      uint16_t	port;
      union {
        uint32_t ipv4;
        struct in6_addr ipv6;
      };
    } mapped_address;
    struct {
      uint16_t	must_be_zero;
      uint8_t	class; // Class is the low 3 bits, high bits must be zero.
      uint8_t	number; // Error code consistent with HTTP and SIP.
      char	phrase[]; // Variable-length UTF-8 error message.
    } error_code;
    struct {
      uint16_t types[4];
    } unknown_attributes;
  } value;
};
