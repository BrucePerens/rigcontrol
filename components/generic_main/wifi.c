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
#include "generic_main.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t my_events = NULL;
enum EventBits {
  CONNECTED_BIT = 1 << 0,
  ESPTOUCH_DONE_BIT = 1 << 1
};

static const char TASK_NAME[] = "wifi smart config";
extern nvs_handle_t nvs;
static TaskHandle_t smart_config_task = NULL;
static esp_event_handler_instance_t handler_ip_event_sta_got_ip = NULL;
static esp_event_handler_instance_t handler_sc_event_got_ssid_pswd = NULL;
static esp_event_handler_instance_t handler_sc_event_send_ack_done = NULL;

extern void start_webserver(void);
extern void stop_webserver();

static void wifi_smart_config(void * parm);
static void wifi_connect_to_ap(const char * ssid, const char * password);

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

  esp_err_t ssid_err = nvs_get_str(nvs, "ssid", ssid, &ssid_size);
  esp_err_t password_err = nvs_get_str(nvs, "wifi_password", password, &password_size);

  // Create a local event group for communication from event handlers to
  // tasks.
  if (my_events == NULL)
    my_events = xEventGroupCreate();

  // ssid_size includes the terminating null.
  if (ssid_err == ESP_OK && password_err == ESP_OK && ssid_size > 1)
    wifi_connect_to_ap(ssid, password);
  else
    xTaskCreate(wifi_smart_config, TASK_NAME, 4096, NULL, 3, &smart_config_task);
}

void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  printf("Wifi disconnected.\n");
  fflush(stdout);
  stop_webserver();
  esp_wifi_connect();
  xEventGroupClearBits(my_events, CONNECTED_BIT);
}

static void ip_event_sta_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

  EventBits_t old_bits = xEventGroupGetBits(my_events);

  if (old_bits & CONNECTED_BIT)
    return;

  xEventGroupSetBits(my_events, CONNECTED_BIT);

  printf("WiFi connected, IP address: " IPSTR "\n", IP2STR(&event->ip_info.ip));
  fflush(stdout);
  start_webserver();
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

  ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", (const char *)evt->ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs, "wifi_password", (const char *)evt->password));
  ESP_ERROR_CHECK(nvs_commit(nvs));
  ESP_ERROR_CHECK( esp_wifi_disconnect() );
  ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  esp_wifi_connect();
}

static void sc_event_send_ack_done(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  xEventGroupSetBits(my_events, ESPTOUCH_DONE_BIT);
}

void stop_smart_config_task(bool external)
{
  smart_config_task = NULL;

  esp_smartconfig_stop();

  if (handler_ip_event_sta_got_ip) {
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, handler_ip_event_sta_got_ip);
    handler_ip_event_sta_got_ip = NULL;
  }
  if (handler_sc_event_got_ssid_pswd) {
    esp_event_handler_instance_unregister(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, handler_sc_event_got_ssid_pswd);
    handler_sc_event_got_ssid_pswd = NULL;
  }
  if (handler_sc_event_send_ack_done) {
    esp_event_handler_instance_unregister(SC_EVENT, SC_EVENT_SEND_ACK_DONE, handler_sc_event_send_ack_done);
    handler_sc_event_send_ack_done = NULL;
  }
  if (smart_config_task)
    vTaskDelete(smart_config_task);
}

static void wifi_smart_config(void * parm)
{
  EventBits_t uxBits;

  ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
   IP_EVENT,
   IP_EVENT_STA_GOT_IP,
   &ip_event_sta_got_ip,
   NULL,
   &handler_ip_event_sta_got_ip));

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
  esp_event_handler_instance_t instance_got_ip;

  bzero(&cfg, sizeof(cfg));
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
  strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));

  if (cfg.sta.password[0] == '\0')
    cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
  else
    cfg.sta.threshold.authmode = WIFI_AUTH_WEP;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_sta_got_ip, NULL) );
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &ip_event_sta_got_ip,
                                                      NULL,
                                                      &instance_got_ip));
  ESP_ERROR_CHECK( esp_wifi_connect() );
}

// FIX: Every time I go through this loop, two more netif interfaces
// are created. Perhaps this is happening in esp_smartconfig.
// One side effect is that the "Got IP" message prints for each netif.
void wifi_restart(void)
{
  stop_smart_config_task(true);
  stop_webserver();
  esp_wifi_disconnect();
  xEventGroupClearBits(my_events, CONNECTED_BIT);
  wifi_event_sta_start(0, 0, 0, 0);
}
