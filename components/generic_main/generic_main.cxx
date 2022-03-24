// Main
//
// Start the program. Initialize system facilities. Set up an event handler
// to call wifi_event_sta_start() once the WiFi station starts and is ready
// for configuration.
//
// Code in wifi.c starts the web server once WiFi is configured. All other
// facilities are started from the web server.
//
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <esp_random.h>
#include <bootloader_random.h>
#include <esp_console.h>

extern void wifi_event_sta_start(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
extern void wifi_event_sta_disconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static void initialize(void);

static const char TASK_NAME[] = "main";
nvs_handle_t nvs;
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;
esp_console_repl_t *repl;

extern "C" void user_startup();

extern "C" void app_main(void)
{
  user_startup();
  initialize();
}

// The platform is supposed to run these for us, before main, but at this
// writing, March 2022, it doesn't.
static void run_constructors(void)
{
  typedef void (*constructor)();
  extern const constructor __CTOR_LIST__[];
  const constructor * c = __CTOR_LIST__;
 
  fprintf(stderr, "Running constructors.\n");
  fflush(stderr);
  while ( *c ) {
    (*c)();
  }
  fprintf(stderr, "Done running constructors.\n");
  fflush(stderr);
}

static void initialize(void)
{
  run_constructors();
  // Give the internal random number generator some entropy from the CPU's
  // hardware RNG, just in case WiFi (which is also an entropy source)
  // doesn't start.
  bootloader_random_enable();

  fprintf(stderr, "Starting console, repl is: %lx\n", (unsigned long)repl);
  fflush(stderr);
  sleep(5);
  ESP_ERROR_CHECK(esp_console_start_repl(repl));

  // Empty configuration for starting WiFi.
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  // The global event loop is required for all event handling to work.
  ESP_ERROR_CHECK(esp_event_loop_create_default());

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

  // Register the event handler for WiFi station ready.
  // When this is called, the event handler will decide whether to connect
  // to an access point, or start smart configuration for the WiFi SSID and
  // password.
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_sta_start, NULL) );
  ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_sta_disconnected, NULL) );

  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_netif_set_hostname(sta_netif, "rigcontrol"));
  ESP_ERROR_CHECK( esp_wifi_start() );
}
