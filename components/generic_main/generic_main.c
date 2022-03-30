// Main
//
// Start the program. Initialize system facilities. Set up an event andler
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
#include "generic_main.h"

static void initialize(void);

static const char TASK_NAME[] = "main";
struct generic_main GM = {};

extern void user_startup();

void app_main(void)
{
  user_startup();
  initialize();
}

static void initialize(void)
{
  // Set the console print lock, so that things in tasks don't print over each other.
  // This can't be used for non-tasks.
  pthread_mutex_init(&GM.console_print_mutex, 0);

  // Give the internal random number generator some entropy from the CPU's
  // hardware RNG, just in case WiFi (which is also an entropy source)
  // doesn't start.
  bootloader_random_enable();

  // Connect the non-volatile-storage FLASH partition. Initialize it if
  // necessary.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TASK_NAME, "Erasing and initializing non-volatile parameter storage.");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }
  ESP_ERROR_CHECK(nvs_open(gm_nvs_index, NVS_READWRITE, &GM.nvs));

  // Configure the console command system.
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.task_stack_size = 40 * 1024;
  repl_config.prompt = ">";
  esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &GM.repl));
  gm_command_add_registered_to_console();
  ESP_ERROR_CHECK(esp_console_start_repl(GM.repl));
  // The global event loop is required for all event handling to work.
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Get the factory-set MAC address, which is a permanent unique number programmed
  // into e-fuse bits of this CPU, and thus is useful for identifying the device.
  esp_efuse_mac_get_default(GM.factory_mac_address);

  // This will be used for the name of the WiFi access point until the user sets the
  // host name.
  snprintf(
   GM.unique_name,
   sizeof(GM.unique_name),
   "%s-%02x:%02x:%02x:%02x:%02x:%02x",
   GM.application_name,
   GM.factory_mac_address[0],
   GM.factory_mac_address[1],
   GM.factory_mac_address[2],
   GM.factory_mac_address[3],
   GM.factory_mac_address[4],
   GM.factory_mac_address[5]);

  gm_printf("Device name: %s\n", GM.unique_name);

  // Start WiFi, if it's already configured.
  gm_wifi_start();
}
