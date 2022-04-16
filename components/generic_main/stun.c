#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <lwip/sockets.h>
#include <netdb.h>
#include "generic_main.h"

// The STUN RFC 8489 requires attributes connected with authentication, and requires
// knowledge of the cleartext password on the server to calculate the MESSAGE-INTEGRITY
// attribute, or at least an MD5 of username:password to calculate the
// MESSAGE-INTEGRITY-SHA256 attribute. Public STUN servers would not have this data,
// and I assume they do not insist on those elements. Having a cleartext password on
// the server is not a good security practice. The MD5 derivative used in
// MESSAGE-INTEGRITY-SHA256 is also problematical because the cleartext password is
// necessary to produce it, and because MD5 is no longer considered a secure hash
// algorithm.
//
// The RFC says nothing about how to run a public STUN server, yet many exist. Thus,
// although required by the RFC, this client doesn't send USERNAME, USERHASH,
// MESSAGE-INTEGRITY, MESSAGE-INTEGRITY-SHA256m because a server that doesn't have
// any password data could not authenticate them. It sends FINGERPRINT, and it could
// send REALM, although the REALM information would be arbitrary and probably useless.

// Standard port number for STUN;
// static const uint16_t stun_port = 3478;

// Magic number used in all STUN packets.
static const uint32_t stun_magic = 0x2112A442;

// The FINGERPRINT data is XOR-ed with this:
// static const char fingerprint_xor[] = "STUN";

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
  uint32_t magic_cookie; // Always stun_magic.
  uint32_t transaction_id[3]; // Fill with random, should be same on replies.
  uint8_t  attributes[]; // Variable length attributes, always padded to 32-bit boundary.
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
    struct {
      uint8_t hash[20];
    } message_integrity;
    struct {
      uint8_t hash[32];
    } message_integrity_sha256;
    struct {
      uint8_t hash[32];
    } userhash;
    struct {
      uint8_t hash[32];
    } fingerprint;
  } value;
};

void
fingerprint(uint8_t * * attribute, uint16_t * length)
{
}

int gm_stun(const char * host, const char * port, bool ipv6)
{
  uint32_t send_buffer[128] = {};
  uint32_t receive_buffer[256] = {};
  struct stun_message *	const	send_packet = (struct stun_message *)send_buffer; 
  struct stun_message *	const	receive_packet = (struct stun_message *)receive_buffer; 
  uint8_t * send_attribute = send_packet->attributes;
  uint8_t * receive_attribute = receive_packet->attributes;
  struct sockaddr_storage	receive_address = {};
  socklen_t			receive_address_size;
  ssize_t			send_result;
  ssize_t			receive_result;
  struct timeval		timeout = {};
  struct addrinfo		hints = {};
  struct addrinfo *		send_address = 0;

  // Only return the address family that is requested in hints->ai_family.
  hints.ai_flags = AI_ADDRCONFIG;

  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if ( ipv6 )
    hints.ai_family = AF_INET6;
  else
    hints.ai_family = AF_INET;

  int gai_result =  getaddrinfo(host, port, &hints, &send_address);

  if ( gai_result != 0 ) {
    fprintf(stderr, "%s: getaddrinfo() error %d\n", host, gai_result);
    return -1;
  }

  unsigned int message_class = STUN_REQUEST;
  unsigned int method = STUN_BINDING;
  send_packet->magic_cookie = stun_magic;
  send_packet->type = htons(((message_class & 0x1) << 4) | ((message_class & 0x2) << 8) | (method & 0xf));
  esp_fill_random(send_packet->transaction_id, sizeof(send_packet->transaction_id));

  int sock = socket (send_address->ai_family, SOCK_DGRAM, send_address->ai_protocol);

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout, sizeof(timeout));

  // Send the packet to the gateway.
  send_result = sendto(
   sock,
   send_packet,
   send_packet->length + 20,
   0,
   send_address->ai_addr,
   send_address->ai_addrlen);

  freeaddrinfo(send_address);

  fprintf(stderr, "Send returned %d\n", send_result);

  receive_address_size = sizeof(struct sockaddr_in6);

  // Receive the reply from the gateway, or not.
  receive_result = recvfrom(
   sock,
   receive_packet,
   sizeof(receive_buffer),
   0,
   (struct sockaddr *)&receive_address,
   &receive_address_size);

  struct stun_attribute * attribute = (struct stun_attribute *)receive_attribute;
  fprintf(stderr, "Receive returned %d\n", receive_result);
  fprintf(stderr, "Receive type: %x\n", ntohs(receive_packet->type));
  fprintf(stderr, "First attribute type %d\n", htons(attribute->type));
  fprintf(stderr, "Address: %x\n", htonl(attribute->value.mapped_address.ipv4));
  

  return 0;
}
