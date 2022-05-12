#include <stdint.h>
#include <sys/types.h>
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
// Warning: This assumes the CPU is little-endian.
static const uint32_t stun_magic = 0x42a41221; // 0x2112a442 in network byte order.

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

enum stun_attributes {
  MAPPED_ADDRESS = 1,
  USERNAME = 6,
  MESSAGE_INTEGRITY = 8,
  ERROR_CODE = 9,
  UNKNOWN_ATTRIBUTES = 0x0a,
  REALM = 0x14,
  NONCE = 0x15,
  XOR_MAPPED_ADDRESS = 0x20,
  SOFTWARE = 0x8022,
  ALTERNATE_SERVER = 0x8023,
  FINGERPRINT = 0x8028,
  MESSAGE_INTEGRITY_SHA256 = 0x1c,
  PASSWORD_ALGORITHM = 0x1d,
  USERHASH = 0x1e,
  PASSWORD_ALGORITHMS = 0x8002,
  ALTERNATE_DOMAIN = 0x8003
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

struct stun_server {
  const char *	host;
  uint16_t	port;
};

typedef void (*gm_stun_after_t)(bool success, bool ipv6, struct sockaddr * address);

struct stun_run {
  struct sockaddr *	address;
  unsigned int		tries;
  bool			ipv6;
  gm_stun_after_t	after;
};

static void stun_receive(int fd, void * data, bool readable, bool writable, bool exception, bool timeout);

static const struct stun_server ipv4_servers[] = {
  { "stun.ooma.com", 3478 },
  // { "stun.3cx.com", 3478 },
  // { "stun.ucsb.edu", 3478 },
  { "stun.l.google.com", 19302 },
  { "stun2.l.google.com", 19302 },
  { "stun3.l.google.com", 19302 },
  { "stun4.l.google.com", 19302 },
  // STUN multiplexed on port 80!
  { "openrelay.metered.ca", 80 }
};
static const size_t	ipv4_table_count = sizeof(ipv4_servers) / sizeof(*ipv4_servers);

static const struct stun_server ipv6_servers[] = {
  { "stun.l.google.com", 19302 },
  { "stun2.l.google.com", 19302 },
  { "stun3.l.google.com", 19302 },
  { "stun4.l.google.com", 19302 }
};
static const size_t	ipv6_table_count = sizeof(ipv6_servers) / sizeof(*ipv6_servers);

static void
decode_mapped_address(struct stun_attribute * a, struct sockaddr * address)
{
  if ( a->value.mapped_address.family == 1 ) {
    struct sockaddr_in * in = (struct sockaddr_in *)address;
    memset(in, '\0', sizeof(*in));
    in->sin_family = AF_INET;
    in->sin_port = a->value.mapped_address.port;
    in->sin_addr.s_addr = a->value.mapped_address.ipv4;
  }
  else {
    struct sockaddr_in6 * in6 = (struct sockaddr_in6 *)address;
    memset(in6, '\0', sizeof(*in6));
    in6->sin6_family = AF_INET6;
    in6->sin6_port = a->value.mapped_address.port;
    memcpy(&in6->sin6_addr.s6_addr, a->value.mapped_address.ipv6.s6_addr, sizeof(in6->sin6_addr.s6_addr));
  }
}

static void
decode_error_code(struct stun_attribute * a)
{
  GM_FAIL("STUN Error code.\n");
}

static void
decode_unknown_attributes(struct stun_attribute * a)
{
  GM_FAIL("STUN Unknown attribute.\n");
}

static void
decode_xor_mapped_address(struct stun_attribute * a, struct stun_message * message, struct sockaddr * address)
{
  if ( a->value.mapped_address.family == 1 ) {
    fflush(stderr);
    sleep(1);
    struct sockaddr_in * in = (struct sockaddr_in *)address;
    memset(in, '\0', sizeof(*in));
    in->sin_family = AF_INET;
    in->sin_port = a->value.mapped_address.port ^ ((stun_magic >> 16) & 0xffff);
    in->sin_addr.s_addr = a->value.mapped_address.ipv4 ^ stun_magic;
  }
  else {
    struct sockaddr_in6 * in6 = (struct sockaddr_in6 *)address;
    memset(in6, '\0', sizeof(*in6));
    in6->sin6_family = AF_INET6;
    uint32_t *	out = (uint32_t *)&in6->sin6_addr.s6_addr;
    uint32_t *	in = (uint32_t *)&a->value.mapped_address.ipv6.s6_addr;
    for ( int i = 0; i < 4; i++ ) {
      out[i] = in[i] ^ stun_magic;
    }
  }
}

static void
decode_software(struct stun_attribute * a)
{
  // gm_printf("Software: ");
  // fwrite((char *)&a->value, 1, ntohs(a->length), stderr);
  // gm_printf("\n");
}

static void
decode_alternate_server(struct stun_attribute * a)
{
  // gm_printf("Alternate Server: ");
  // fwrite((char *)&a->value, 1, ntohs(a->length), stderr);
  // gm_printf("\n");
}

static void
decode_fingerprint(struct stun_attribute * a)
{
}

struct addrinfo *
get_address(const char * host, uint16_t port, bool ipv6)
{
  struct addrinfo *		send_address = 0;
  struct addrinfo		hints = {};
  char				port_string[6];

  // Only return the address family that is requested in hints->ai_family.
  hints.ai_flags = AI_ADDRCONFIG;

  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  if ( ipv6 )
    hints.ai_family = AF_INET6;
  else
    hints.ai_family = AF_INET;

  snprintf(port_string, sizeof(port_string), "%d", port);
  int gai_result =  getaddrinfo(host, port_string, &hints, &send_address);

  if ( gai_result != 0 ) {
    GM_FAIL("%s: getaddrinfo() error %d\n", host, gai_result);
    return 0;
  }
  return send_address;
}

static int
send_stun_request(bool ipv6)
{
  uint32_t send_buffer[128] = {};
  struct stun_message *	const	send_packet = (struct stun_message *)send_buffer; 
  struct addrinfo *		send_address;
  ssize_t			send_result;
  unsigned			int message_class = STUN_REQUEST;
  unsigned			int method = STUN_BINDING;
  const struct stun_server *	servers;
  size_t			count;
  const struct stun_server *	server;

  if ( ipv6 ) {
    servers = ipv6_servers;
    count = ipv6_table_count;
  }
  else {
    servers = ipv4_servers;
    count = ipv4_table_count;
  }

  server = &servers[gm_choose_one(count)];

  if ( (send_address = get_address(server->host, server->port, ipv6)) == 0 )
    return -1;

  int sock = socket (send_address->ai_family, SOCK_DGRAM, send_address->ai_protocol);
  if ( sock < 0 ) {
    GM_FAIL("Can't get socket: %s\n", strerror(errno));
    return -1;
  }

  send_packet->magic_cookie = stun_magic;
  send_packet->type = htons(((message_class & 0x1) << 4) | ((message_class & 0x2) << 8) | (method & 0xf));
  esp_fill_random(send_packet->transaction_id, sizeof(send_packet->transaction_id));

  send_result = sendto(
   sock,
   send_packet,
   send_packet->length + 20,
   0,
   send_address->ai_addr,
   send_address->ai_addrlen);

  freeaddrinfo(send_address);

  if ( send_result < (send_packet->length + 20) ) {
    GM_FAIL("Send error: %d %s.\n", send_result, strerror(errno));
    close(sock);
    return -1;
  }
  return sock;
}

static int
process_received_packet(struct stun_message * receive_packet, struct sockaddr * address, ssize_t receive_result)
{
  bool				got_xor_mapped_address = false;
  bool				got_an_address = false;
  struct stun_attribute * attribute = (struct stun_attribute *)receive_packet->attributes;
  uint16_t attribute_size = ntohs(receive_packet->length);
  if ( attribute_size < (receive_result - 20) ) {
    GM_FAIL("STUN packet was truncated.\n");
    return -1;
  }
  
  while ( attribute_size > 0 ) {
    uint16_t type = ntohs(attribute->type);
    uint16_t length = ntohs(attribute->length);
    if ( length == 0 || length >= 768 ) {
      GM_FAIL("STUN attribute length %d is invalid.\n", length);
    }
    switch ( type ) {
    case MAPPED_ADDRESS:
      if ( !got_xor_mapped_address ) {
        got_an_address = true;
        decode_mapped_address(attribute, address);
      }
      break;
    case ERROR_CODE:
      decode_error_code(attribute);
      return -1;
      break;
    case UNKNOWN_ATTRIBUTES:
      decode_unknown_attributes(attribute);
      break;
    case XOR_MAPPED_ADDRESS:
      got_an_address = true;
      got_xor_mapped_address = true;
      decode_xor_mapped_address(attribute, receive_packet, address);
      break;
    case SOFTWARE:
      decode_software(attribute);
      break;
    case ALTERNATE_SERVER:
      decode_alternate_server(attribute);
      break;
    case FINGERPRINT:
      decode_fingerprint(attribute);
      break;
    default:
      break;
    }
    uint16_t odd = length & 3;
    unsigned int increment = length + 4;

    // Pad strings to 32-bit boundaries.
    if ( odd )
      increment += (4 - odd);

    if ( increment > attribute_size ) {
      GM_FAIL("Attribute size mismatch.\n");
      return -1;
    }
    attribute = (struct stun_attribute *)((uint8_t *)attribute + increment);
    attribute_size -= increment;
  }
  if ( got_an_address )
    return 0;
  else {
    GM_FAIL("STUN message didn't include an address.\n");
    return -1;
  }
}

int receive_stun_response(int sock, struct sockaddr * address)
{
  uint32_t receive_buffer[256] = {};
  struct stun_message *	const	receive_packet = (struct stun_message *)receive_buffer; 
  ssize_t			receive_result;
  struct timeval		timeout = { 3, 0 }; // 2 seconds, no microseconds.
  int result;

  setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
	    sizeof (timeout));

  receive_result = recvfrom(
   sock,
   receive_packet,
   sizeof(receive_buffer),
   0,
   0,
   0);

  if ( receive_result < 22 ) {
    close(sock);
    return -1;
  }

  result = process_received_packet(receive_packet, address, receive_result);
  close(sock);
  return result;
}

static void
stun_send(void * data)
{
  struct stun_run * run = (struct stun_run *)data;
  int	sock;

  // FIX: Handle address unreachable.
  if ( (sock = send_stun_request(run->ipv6)) < 0 )
    return;

  gm_fd_register(sock, stun_receive, run, true, false, true, 5);
}

static void
stun_receive(int fd, void * data, bool readable, bool writable, bool exception, bool timeout)
{
  struct stun_run * run = (struct stun_run *)data;

  if ( readable ) {
    int status = receive_stun_response(fd, run->address);
    gm_fd_unregister(fd);
    close(fd);

    if ( status == 0 ) {
      if ( run->after )
        (run->after)(true, run->ipv6, run->address);
      free(run);
      return;
    }
  }
  if ( run->tries >= 5 ) {
    free(run);
    if ( run->after )
      (run->after)(false, run->ipv6, run->address);
    return;
  }

  run->tries++;

  gm_run(stun_send, run, GM_FAST);
}

int gm_stun(bool ipv6, struct sockaddr * address, gm_stun_after_t after)
{
  struct stun_run * run = malloc(sizeof(struct stun_run));

  if ( run == 0 ) {
    GM_FAIL("malloc failed");
    return -1;
  }
    
  run->ipv6 = ipv6;
  run->address = address;
  run->tries = 0;
  run->after = after;

  gm_run(stun_send, run, GM_FAST);

  return 0;
}
