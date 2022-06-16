// Configure WiFi.
//
// wifi_event_sta_start() is called as an event handler after initialize()
// in main.c initializes the WiFi station and the station starts.
//
// If there is already a WiFi SSID and password (possibly blank) in
// the non-volatile parameter storage, connect to an access point with
// that SSID. If not, wait for the user to run the EspTouch app and configure
// the SSID and password.
//
// Once connected to the access point, start the web server.
//
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_smartconfig.h"
#include <esp_netif_net_stack.h>
// Kludge beccause this file isn't exported, but it is necessary to get a LWIP
// struct netif * from an esp_netif_t.
#include <../lwip/esp_netif_lwip_internal.h>
#include <lwip/dhcp6.h>
#include <arpa/inet.h>
#include <lwip/sockets.h>
#include <pthread.h>
#include "generic_main.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t my_events = NULL;
enum EventBits {
  CONNECTED_BIT = 1 << 0,
  ESPTOUCH_DONE_BIT = 1 << 1
};

static const char TASK_NAME[] = "wifi";
static TaskHandle_t smart_config_task_id = NULL;
static esp_event_handler_instance_t handler_wifi_event_sta_connected_to_ap = NULL;
static esp_event_handler_instance_t handler_ip_event_sta_got_ip4 = NULL;
static esp_event_handler_instance_t handler_ip_event_got_ip6 = NULL;
static esp_event_handler_instance_t handler_sc_event_got_ssid_pswd = NULL;
static esp_event_handler_instance_t handler_sc_event_send_ack_done = NULL;

extern void start_webserver(void);
extern void stop_webserver();

static void smart_config_task(void * parm);
static void wifi_connect_to_ap(const char * ssid, const char * password);
static void wifi_event_sta_start(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void after_stun(bool success, bool ipv6, struct sockaddr * address)
{
  char	buffer[INET6_ADDRSTRLEN + 1];

  if ( success ) {
    if ( ipv6 ) {
      inet_ntop(AF_INET6, &((struct sockaddr_in6 *)address)->sin6_addr, buffer, sizeof(buffer));
    }
    else
      inet_ntop(AF_INET, &((struct sockaddr_in *)address)->sin_addr, buffer, sizeof(buffer));
   
    gm_printf("Public address %s.\n", buffer);
  }
  else {
    gm_printf("STUN for %s failed.\n", ipv6 ? "IPv6" : "IPv4");
  }
}

void
gm_wifi_start(void)
{
  // Empty configuration for starting WiFi.
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  // Initialize the TCP/IP interfaces for WiFi.
  GM.sta.esp_netif = esp_netif_create_default_wifi_sta();
  // GM.ap_netif = esp_netif_create_default_wifi_ap();
  // assert(GM.ap.netif);

  // Register the event handler for WiFi station ready.
  // When this is called, the event handler will decide whether to connect
  // to an access point, or start smart configuration for the WiFi SSID and
  // password.
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_sta_start, NULL) );
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_sta_disconnected, NULL) );

  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_netif_set_hostname(GM.sta.esp_netif, "rigcontrol"));
  ESP_ERROR_CHECK( esp_wifi_start() );
}

// This is called when WiFi is ready for configuration.
// If an SSID and password are stored, attempt to connect to an access 
// point. If not, start smart configuration, and wait for the user to set
// the SSID and password from the EspTouch Android or iOS app.
//
void wifi_event_sta_start(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  char ssid[33] = { 0 };
  char password[65] = { 0 };
  size_t ssid_size = sizeof(ssid);
  size_t password_size = sizeof(password);

  esp_err_t ssid_err = nvs_get_str(GM.nvs, "ssid", ssid, &ssid_size);
  esp_err_t password_err = nvs_get_str(GM.nvs, "wifi_password", password, &password_size);

  // Create a local event group for communication from event handlers to
  // tasks.
  if (my_events == NULL)
    my_events = xEventGroupCreate();

  // ssid_size includes the terminating null.
  if (ssid_err == ESP_OK && password_err == ESP_OK && ssid_size > 1)
    wifi_connect_to_ap(ssid, password);
  else
    xTaskCreate(smart_config_task, TASK_NAME, 4096, NULL, 3, &smart_config_task_id);
}

void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  gm_printf("Wifi disconnected.\n");
  fflush(stderr);
  stop_webserver();
  esp_wifi_connect();
  xEventGroupClearBits(my_events, CONNECTED_BIT);
}

static void
ipv6_router_advertisement_handler(struct sockaddr_in6 * address, uint16_t router_lifetime)
{
  if ( !gm_all_zeroes(GM.sta.ip6.router.sin6_addr.s6_addr, sizeof(GM.sta.ip6.router.sin6_addr.s6_addr))
   && memcmp(GM.sta.ip6.router.sin6_addr.s6_addr, address->sin6_addr.s6_addr, sizeof(address->sin6_addr.s6_addr)) != 0 ) {
    static bool first_time = true;
    if ( first_time ) {
      gm_printf("Received a router advertisement from more than one IPv6 router. Ignoring all but the first.\n");
      first_time = false;
    }
    return;
  }
  memcpy(GM.sta.ip6.router.sin6_addr.s6_addr, address->sin6_addr.s6_addr, sizeof(address->sin6_addr.s6_addr));
  GM.sta.ip6.router.sin6_family = AF_INET6;
  GM.sta.ip6.router.sin6_port = 0;
}

static void wifi_event_sta_connected_to_ap(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  
  // Start the ICMPv6 listener as early as possible, so that we get the router
  // advertisement that is solicited when the IPV6 interfaces are configured.
  gm_icmpv6_start_listener_ipv6(ipv6_router_advertisement_handler);
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_create_ip6_linklocal(GM.sta.esp_netif));
  dhcp6_enable_stateful(GM.sta.esp_netif->lwip_netif);
  dhcp6_enable_stateless(GM.sta.esp_netif->lwip_netif);
}

// This handler is called only when the "sta" netif gets an IPv4 address.
static void ip_event_sta_got_ip4(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
  char	buffer[INET6_ADDRSTRLEN + 1];

  // Smartconfig waits on this bit, then prints a message.
  xEventGroupSetBits(my_events, CONNECTED_BIT);

  // Save the IP information for other facilities, like NAT-PCP, to use.
  GM.sta.esp_netif = event->esp_netif;
  GM.sta.ip4.address.sin_family = AF_INET;
  GM.sta.ip4.address.sin_addr.s_addr = event->ip_info.ip.addr;
  GM.sta.ip4.router.sin_family = AF_INET;
  GM.sta.ip4.router.sin_addr.s_addr = event->ip_info.gw.addr;
  GM.sta.ip4.netmask = event->ip_info.netmask.addr;

  inet_ntop(AF_INET, &event->ip_info.ip.addr, buffer, sizeof(buffer));
  gm_printf("Got IPv4: interface %s, address %s ", esp_netif_get_desc(event->esp_netif), buffer);
  inet_ntop(AF_INET, &event->ip_info.gw.addr, buffer, sizeof(buffer));
  gm_printf("router %s\n", buffer);
  fflush(stderr);
  gm_stun(false, (struct sockaddr *)&GM.sta.ip4.public, after_stun);
  gm_port_control_protocol_start_listener_ipv4();
  gm_port_control_protocol_request_mapping_ipv4();
  start_webserver();
  gm_log_server();
}

// This handler is called when any netif gets an IPv6 address.
static void ip_event_got_ip6(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  const unsigned int public_address_count = \
   sizeof(GM.sta.ip6.global) / sizeof(*GM.sta.ip6.global);
  char buffer[INET6_ADDRSTRLEN + 1];

  ip_event_got_ip6_t *	event = (ip_event_got_ip6_t*)event_data;
  esp_ip6_addr_type_t	ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
  const char *		netif_name = esp_netif_get_desc(event->esp_netif);
  gm_netif_t *		interface;
  bool			is_station = false;
   
  inet_ntop(AF_INET6, &event->ip6_info.ip.addr, buffer, sizeof(buffer));
  gm_printf("Got IPv6: interface %s, address, type %s\n",
  netif_name,
  buffer,
  GM.ipv6_address_types[ipv6_type]);
  fflush(stderr);

  if (strcmp(netif_name, "sta") == 0) {
    interface = &GM.sta;
    is_station = true;
  }
  else if (strcmp(netif_name, "ap") == 0)
    interface = &GM.ap;
  else
    return;
  
  switch (ipv6_type) {
  case ESP_IP6_ADDR_IS_UNKNOWN:
    break;
  case ESP_IP6_ADDR_IS_GLOBAL:
    for ( unsigned int i = 0; i < public_address_count - 1; i++) {
       // If we already have this address, don't add it twice.
       if (memcmp(interface->ip6.global[i].sin6_addr.s6_addr, event->ip6_info.ip.addr, sizeof(event->ip6_info.ip.addr)) == 0)
         break;
       // If one of the public address entries is all zeroes, copy the address into it.
       if (gm_all_zeroes(&interface->ip6.global[i].sin6_addr.s6_addr, sizeof(&interface->ip6.global[0].sin6_addr.s6_addr))) {
         interface->ip6.global[i].sin6_family = AF_INET6;
         memcpy(interface->ip6.global[i].sin6_addr.s6_addr, event->ip6_info.ip.addr, sizeof(event->ip6_info.ip.addr));
         break;
       }
    }
    break;
  case ESP_IP6_ADDR_IS_LINK_LOCAL:
    interface->ip6.link_local.sin6_family = AF_INET6;
    memcpy(interface->ip6.link_local.sin6_addr.s6_addr, event->ip6_info.ip.addr, sizeof(event->ip6_info.ip.addr));
    if (is_station) {
      // FIX: We may never get a link-local address on some systems.
      // Cope with it if we don't.
      gm_port_control_protocol_start_listener_ipv6();
      gm_port_control_protocol_request_mapping_ipv6();
      gm_stun(true, (struct sockaddr *)&interface->ip6.public, after_stun);
    }
    break;
  case ESP_IP6_ADDR_IS_SITE_LOCAL:
    interface->ip6.site_local.sin6_family = AF_INET6;
    memcpy(interface->ip6.site_local.sin6_addr.s6_addr, event->ip6_info.ip.addr, sizeof(event->ip6_info.ip.addr));
    break;
  case ESP_IP6_ADDR_IS_UNIQUE_LOCAL:
    interface->ip6.site_unique.sin6_family = AF_INET6;
    memcpy(interface->ip6.site_unique.sin6_addr.s6_addr, event->ip6_info.ip.addr, sizeof(event->ip6_info.ip.addr));
    break;
  case ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6:
    break;
  }
}

static void sc_event_got_ssid_pswd(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  ESP_LOGD(TASK_NAME, "Got SSID and password");

  smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
  wifi_config_t wifi_config;

  bzero(&wifi_config, sizeof(wifi_config_t));
  memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
  memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
  wifi_config.sta.bssid_set = evt->bssid_set;
  if (wifi_config.sta.bssid_set) {
    memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
  }

  ESP_ERROR_CHECK(nvs_set_str(GM.nvs, "ssid", (const char *)evt->ssid));
  ESP_ERROR_CHECK(nvs_set_str(GM.nvs, "wifi_password", (const char *)evt->password));
  ESP_ERROR_CHECK(nvs_commit(GM.nvs));
  ESP_ERROR_CHECK( esp_wifi_disconnect() );
  ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  esp_wifi_connect();
}

static void sc_event_send_ack_done(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  xEventGroupSetBits(my_events, ESPTOUCH_DONE_BIT);
}

void stop_smart_config_task(bool external)
{
  smart_config_task_id = NULL;

  esp_smartconfig_stop();

  if (handler_ip_event_sta_got_ip4) {
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, handler_ip_event_sta_got_ip4);
    handler_ip_event_sta_got_ip4 = NULL;
  }
  if (handler_sc_event_got_ssid_pswd) {
    esp_event_handler_instance_unregister(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, handler_sc_event_got_ssid_pswd);
    handler_sc_event_got_ssid_pswd = NULL;
  }
  if (handler_sc_event_send_ack_done) {
    esp_event_handler_instance_unregister(SC_EVENT, SC_EVENT_SEND_ACK_DONE, handler_sc_event_send_ack_done);
    handler_sc_event_send_ack_done = NULL;
  }
  if (smart_config_task_id)
    vTaskDelete(smart_config_task_id);
}

static void smart_config_task(void * parm)
{
  EventBits_t uxBits;

  ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   IP_EVENT,
   IP_EVENT_STA_GOT_IP,
   &ip_event_sta_got_ip4,
   NULL,
   &handler_ip_event_sta_got_ip4));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   SC_EVENT,
   SC_EVENT_GOT_SSID_PSWD,
   &sc_event_got_ssid_pswd,
   NULL,
   &handler_sc_event_got_ssid_pswd));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   SC_EVENT,
   SC_EVENT_SEND_ACK_DONE,
   &sc_event_send_ack_done,
   NULL,
   &handler_sc_event_send_ack_done));

  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );

  for (;;) {
    uxBits = xEventGroupWaitBits(my_events, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
    if(uxBits & CONNECTED_BIT)
      ESP_LOGI(TASK_NAME, "WiFi Connected to ap");
    if(uxBits & ESPTOUCH_DONE_BIT)
      break;
  }

  ESP_LOGI(TASK_NAME, "WiFi smart config done");

  stop_smart_config_task(false);
}

static void wifi_connect_to_ap(const char * ssid, const char * password)
{
  wifi_config_t cfg;

  bzero(&cfg, sizeof(cfg));
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
  strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));

  if (cfg.sta.password[0] == '\0')
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
  else
    cfg.sta.threshold.authmode = WIFI_AUTH_WEP;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   WIFI_EVENT,
   WIFI_EVENT_STA_CONNECTED,
   &wifi_event_sta_connected_to_ap,
   NULL,
   &handler_wifi_event_sta_connected_to_ap));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   IP_EVENT,
   IP_EVENT_STA_GOT_IP,
   &ip_event_sta_got_ip4,
   NULL,
   &handler_ip_event_sta_got_ip4));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   IP_EVENT,
   IP_EVENT_GOT_IP6,
   &ip_event_got_ip6,
   NULL,
   &handler_ip_event_got_ip6));

  ESP_ERROR_CHECK( esp_wifi_connect() );
}

void gm_wifi_restart(void)
{
  stop_smart_config_task(true);
  stop_webserver();
  esp_wifi_disconnect();
  xEventGroupClearBits(my_events, CONNECTED_BIT);
  wifi_event_sta_start(0, 0, 0, 0);
}
