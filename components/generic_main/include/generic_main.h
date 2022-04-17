#pragma once
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_console.h>
#include <esp_netif.h>
#include <esp_netif_types.h>
#include <esp_netif_net_stack.h>
#include <netinet/in.h>
#include <../lwip/esp_netif_lwip_internal.h>
#include <pthread.h>
#include <sys/socket.h>

#define CONSTRUCTOR static void __attribute__ ((constructor))

typedef enum _gm_param_result {
  PR_ERROR = -2,
  PR_NOT_IN_PARAMETER_TABLE = -1,
  PR_NORMAL = 0,
  PR_SECRET = 1,
  PR_NOT_SET = 2
} gm_param_result_t;

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

struct _gm_netif {
  esp_netif_t *		esp_netif;
  struct netif *	lwip_netif;
  esp_netif_ip_info_t	ip_info;
  esp_ip4_addr_t	public_ip4;
  esp_netif_ip6_info_t	link_local_ip6;
  esp_netif_ip6_info_t	site_local_ip6;
  esp_netif_ip6_info_t	site_unique_ip6;
  esp_netif_ip6_info_t	public_ip6[3];
  esp_ip6_addr_t	router_ip6;
};
typedef struct _gm_netif gm_netif_t;

struct generic_main {
  nvs_handle_t		nvs;
  gm_netif_t		ap;
  gm_netif_t		sta;
  gm_port_mapping_t *	port_mappings;
  esp_console_repl_t *	repl;
  pthread_mutex_t 	console_print_mutex;
  int64_t		time_last_synchronized;
  uint8_t		factory_mac_address[6];
  const char *		application_name;
  char			unique_name[64];
};

struct _GM_Array;

typedef struct _GM_Array GM_Array;
typedef void (*gm_param_list_coroutine_t)(const char *, const char *, const char *, gm_param_result_t);
typedef void (*gm_web_get_coroutine_t)(const char * data, size_t size);
typedef int (*gm_pattern_coroutine_t)(const char * name, char * result, size_t result_size);

extern struct generic_main GM;
extern const char gm_nvs_index[];


extern bool			gm_all_zeroes(const void *, size_t);
extern const void *		gm_array_add(GM_Array * array, const void * data);
extern GM_Array *		gm_array_create(void);
extern const void * *		gm_array_data(GM_Array * array);
extern void			gm_array_destroy(GM_Array * array);
extern const void *		gm_array_get(GM_Array * array, size_t index);
extern size_t			gm_array_size(GM_Array * array);

extern void			gm_command_add_registered_to_console(void);
extern void			gm_command_register(const esp_console_cmd_t * command);

extern int			gm_ddns(void);

extern gm_param_result_t	gm_param_erase(const char * name);
extern gm_param_result_t	gm_param_get(const char * name, char * buffer, size_t size);
extern void			gm_param_list(gm_param_list_coroutine_t coroutine);
extern gm_param_result_t	gm_param_set(const char * name, const char * value);

extern int			gm_pattern_string(const char * string, gm_pattern_coroutine_t coroutine, char * buffer, size_t buffer_size);
extern int			gm_port_control_protocol(gm_port_mapping_t *);
extern int			gm_printf(const char * format, ...);
extern int			gm_public_ipv4(char * data, size_t size);

extern int			gm_stun(const char * host, const char * port, bool ipv6, struct sockaddr * address);

extern void			gm_timer_to_human(int64_t, char *, size_t);

extern int			gm_vprintf(const char * format, va_list args);

extern int			gm_web_get(const char *url, char *data, size_t size);
extern int			gm_web_get_with_coroutine(const char *url, gm_web_get_coroutine_t coroutine);

extern void			gm_wifi_start(void);
extern void			gm_wifi_restart(void);
