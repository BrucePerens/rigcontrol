// Initialized values in the GM struct.
//
#include <stdio.h>
#include "generic_main.h"

extern const char gm_build_version[];
extern const char gm_build_number[];

static const char nvs_index[] = {
  194, 169, 32, 66, 114, 117, 99, 101, 32, 80, 101, 114, 101, 110, 115, 0
};

generic_main_t GM = {
  .build_version = gm_build_version,
  .build_number = gm_build_number,

  // Address type names corresponding to the value returned by
  // esp_netif_ip6_get_addr_type().
  //
  .ipv6_address_types = {
      "unknown",
      "global",
      "link-local",
      "site-local",
      "unique-local",
      "IPv4-mapped-IPv6"
  },

  // File descriptor upon which logging is emitted.
  // This will change while the log server has accepted a connection.
  //
  .log_fd = 2,

  // Magic number for non-volatile storage.
  //
  .nvs_index = nvs_index
};

