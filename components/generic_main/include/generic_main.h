#pragma once
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_console.h>
#include <esp_netif.h>
#include <esp_netif_types.h>
#include <esp_netif_net_stack.h>
#include <esp_event.h>
#include <netinet/in.h>
#include <../lwip/esp_netif_lwip_internal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <esp_debug_helpers.h>

#define CONSTRUCTOR static void __attribute__ ((constructor))

// #define GM_FAIL(args...) (gm_printf("\nError in function: %s at: %s:%d, errno: %s\n", __PRETTY_FUNCTION__, __FILE__, __LINE__, strerror(errno)), gm_printf(args), gm_printf("\n"), esp_backtrace_print(100))
extern void gm_fail(const char *, const char *, int, const char *, ...);
#define GM_FAIL(args...) gm_fail(__PRETTY_FUNCTION__, __FILE__, __LINE__, args)

ESP_EVENT_DECLARE_BASE(GM_EVENT);

typedef enum _gm_param_result {
  GM_ERROR = -2,
  GM_NOT_IN_PARAMETER_TABLE = -1,
  GM_NORMAL = 0,
  GM_SECRET = 1,
  GM_NOT_SET = 2
} gm_param_result_t;

typedef enum _gm_run_speed {
  GM_SLOW,
  GM_MEDIUM,
  GM_FAST
} gm_run_speed_t;

typedef enum _gm_event_id {
  GM_RUN
} gm_event_id_t;

typedef void (*gm_fd_handler_t)(int fd, void * data, bool readable, bool writable, bool exception, bool timeout);
typedef void (*gm_run_t)(void *);
typedef void (*gm_stun_after_t)(bool success, bool ipv6, struct sockaddr * address);
typedef void (*gm_ipv6_router_advertisement_after_t)(struct sockaddr_in6 * address, uint16_t lifetime);

typedef struct _gm_run_data {
  gm_run_t	procedure;
  void *	data;
} gm_run_data_t;

typedef struct _gm_port_mapping { 
  struct timeval granted_time;
  uint32_t nonce[3];
  uint32_t epoch;
  uint32_t lifetime;
  uint16_t internal_port;
  uint16_t external_port;
  struct in6_addr external_address;
  bool ipv6;
  bool tcp;
  struct _gm_port_mapping * next;
} gm_port_mapping_t;

typedef struct _gm_netif {
  esp_netif_t *		esp_netif;
  // lwip_netif is esp_netif->lwip_netif
  struct gm_netif_ip4 {
    struct sockaddr_in	address;
    struct sockaddr_in	router;
    uint32_t		netmask;
    struct sockaddr_in	public;
    int			nat;	// 1 for NAT, 2 for double-nat.
    gm_port_mapping_t *	port_mappings;
  } ip4;
  struct gm_netif_ip6 {
    struct sockaddr_in6	link_local;
    struct sockaddr_in6	site_local;
    struct sockaddr_in6	site_unique;
    struct sockaddr_in6	global[3];
    struct sockaddr_in6 router;
    struct sockaddr_in6	public;
    gm_port_mapping_t *	port_mappings;
    bool pat66; // True if there is prefix-address-translation. Ugh.
    bool nat6;  // True if there is NAT6 that is not PAT66. Double-ugh.
  } ip6;
} gm_netif_t;

typedef struct _generic_main {
  nvs_handle_t		nvs;
  gm_netif_t		ap;
  gm_netif_t		sta;
  esp_console_repl_t *	repl;
  pthread_mutex_t 	console_print_mutex;
  int64_t		time_last_synchronized;
  uint8_t		factory_mac_address[6];
  const char *		application_name;
  char			unique_name[64];
  esp_event_loop_handle_t medium_event_loop;
  esp_event_loop_handle_t slow_event_loop;
} generic_main_t;

struct _GM_Array;

typedef struct _GM_Array GM_Array;
typedef void (*gm_param_list_coroutine_t)(const char *, const char *, const char *, gm_param_result_t);
typedef void (*gm_web_get_coroutine_t)(const char * data, size_t size);
typedef int (*gm_pattern_coroutine_t)(const char * name, char * result, size_t result_size);

generic_main_t			GM;
extern const char *		const gm_ipv6_address_types[6];
extern const char		gm_nvs_index[];

extern bool			gm_all_zeroes(const void *, size_t);
extern const void *		gm_array_add(GM_Array * array, const void * data);
extern GM_Array *		gm_array_create(void);
extern const void * *		gm_array_data(GM_Array * array);
extern void			gm_array_destroy(GM_Array * array);
extern const void *		gm_array_get(GM_Array * array, size_t index);
extern size_t			gm_array_size(GM_Array * array);

extern size_t			gm_choose_one(size_t number_of_entries);
extern void			gm_command_add_registered_to_console(void);
extern void			gm_command_register(const esp_console_cmd_t * command);

extern int			gm_ddns(void);

extern void			gm_event_server(void);

extern void			gm_run(gm_run_t function, void * data, gm_run_speed_t speed);
extern void			gm_fd_register(int fd, gm_fd_handler_t handler, void * data, bool readable, bool writable, bool exception, uint32_t seconds);
extern void			gm_fd_unregister(int fd);

extern void			gm_icmpv6_start_listener_ipv6(gm_ipv6_router_advertisement_after_t after);

extern gm_param_result_t	gm_param_erase(const char * name);
extern gm_param_result_t	gm_param_get(const char * name, char * buffer, size_t size);
extern void			gm_param_list(gm_param_list_coroutine_t coroutine);
extern gm_param_result_t	gm_param_set(const char * name, const char * value);

extern int			gm_pattern_string(const char * string, gm_pattern_coroutine_t coroutine, char * buffer, size_t buffer_size);
extern int			gm_port_control_protocol_request_mapping_ipv4(void);
extern int			gm_port_control_protocol_request_mapping_ipv6(void);
extern void			gm_port_control_protocol_start_listener_ipv4(void);
extern void			gm_port_control_protocol_start_listener_ipv6(void);
extern int			gm_printf(const char * format, ...);
extern int			gm_public_ipv4(char * data, size_t size);

extern int			gm_stun(bool ipv6, struct sockaddr * address, gm_stun_after_t after);
extern void			gm_select_task(void);
extern void			gm_select_wakeup(void);

extern void			gm_timer_to_human(int64_t, char *, size_t);

extern int			gm_vprintf(const char * format, va_list args);

extern int			gm_web_get(const char *url, char *data, size_t size);
extern int			gm_web_get_with_coroutine(const char *url, gm_web_get_coroutine_t coroutine);

extern void			gm_wifi_start(void);
extern void			gm_wifi_restart(void);
