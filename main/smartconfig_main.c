#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include <esp_http_server.h>

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t my_events;
enum EventBits {
  CONNECTED_BIT = 1 << 0,
  ESPTOUCH_DONE_BIT = 1 << 1
};

static const char *TASK_NAME = "wifi_smart_config";
nvs_handle_t nvs;
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;
httpd_handle_t server = NULL;

static void wifi_smart_config(void * parm);
static void wifi_connect_to_ap(const char * ssid, const char * password);
static httpd_handle_t start_webserver(void);

// This is called when WiFi is ready for configuration.
// If an SSID and password are stored, attempt to connect to an access 
// point. If not, start smart configuration, and wait for the user to set
// the SSID and password from the EspTouch Android or iOS app.
//
static void wifi_event_sta_start(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  char ssid[33] = { 0 };
  char password[65] = { 0 };
  size_t ssid_size = sizeof(ssid);
  size_t password_size = sizeof(password);

  esp_err_t ssid_err = nvs_get_str(nvs, "ssid", ssid, &ssid_size);
  esp_err_t password_err = nvs_get_str(nvs, "password", password, &password_size);

  // ssid_size includes the terminating null.
  if (ssid_err == ESP_OK && password_err == ESP_OK && ssid_size > 1)
    wifi_connect_to_ap(ssid, password);
  else
    xTaskCreate(wifi_smart_config, TASK_NAME, 4096, NULL, 3, NULL);
}

static void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  printf("WiFi Disconnected.\n");
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
  esp_wifi_connect();
  xEventGroupClearBits(my_events, CONNECTED_BIT);
}

static void ip_event_sta_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

  xEventGroupSetBits(my_events, CONNECTED_BIT);
  if (server == NULL) {
    printf("WiFi connected, IP address: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    start_webserver();
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

  ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", (const char *)evt->ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs, "password", (const char *)evt->password));
  ESP_ERROR_CHECK(nvs_commit(nvs));
  ESP_ERROR_CHECK( esp_wifi_disconnect() );
  ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  esp_wifi_connect();
}

static void sc_event_send_ack_done(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  xEventGroupSetBits(my_events, ESPTOUCH_DONE_BIT);
}

static void wifi_smart_config(void * parm)
{
    EventBits_t uxBits;
    esp_event_handler_instance_t instance_got_ip;

    printf("Please configure this device using the EspTouch app.");

    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_sta_got_ip,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, &sc_event_got_ssid_pswd, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, SC_EVENT_SEND_ACK_DONE, &sc_event_send_ack_done, NULL) );

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
    esp_smartconfig_stop();

    ESP_ERROR_CHECK( esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_sta_got_ip) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(SC_EVENT, SC_EVENT_GOT_SSID_PSWD, &sc_event_got_ssid_pswd) );
    ESP_ERROR_CHECK( esp_event_handler_unregister(SC_EVENT, SC_EVENT_SEND_ACK_DONE, &sc_event_send_ack_done) );

    vTaskDelete(NULL);
}

static void initialize()
{
  esp_log_level_set("*", ESP_LOG_WARN);
  // esp_log_level_set("*", ESP_LOG_INFO);

  // Empty configuration for starting WiFi.
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  // The global event loop is required for all event handling to work.
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Create a local event group for communication from event handlers to
  // tasks.
  my_events = xEventGroupCreate();

  // Connect the non-volatile-storage FLASH partition. Initialize it if
  // necessary.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TASK_NAME, "Erasing and initializing non-volatile parameter storage.");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(nvs_open("k6bp_rigcontrol", NVS_READWRITE, &nvs));

  // Initialize the TCP/IP stack.
  ESP_ERROR_CHECK(esp_netif_init());

  // Initialize the TCP/IP interfaces for WiFi.
  sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  // ap_netif = esp_netif_create_default_wifi_ap();
  // assert(ap_netif);

  ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, "rigcontrol"));
  // Register the event handler for WiFi station ready.
  // When this is called, the event handler will decide whether to connect
  // to an access point, or start smart configuration for the WiFi SSID and
  // password.
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_sta_start, NULL) );
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_sta_disconnected, NULL) );

  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_start() );
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

static esp_err_t http_root_handler(httpd_req_t *req)
{
  static const char * page = "<html><body>K6BP RigControl</body></html>";
    
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);

  return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  static const httpd_uri_t root = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = http_root_handler,
      .user_ctx  = NULL
  };
  // Start the httpd server
  ESP_LOGI(TASK_NAME, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &root);
    return server;
  }

  ESP_LOGI(TASK_NAME, "Error starting server!");
  return NULL;
}

void app_main(void)
{
  printf("\n\n*** K6BP RigControl ***\n");
  initialize();
}
