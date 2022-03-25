#include "generic_main.h"
#include <esp_console.h>
#include <driver/uart.h>
#include "linenoise/linenoise.h"

static bool did_initialize = false;

// This is called from CONSTRUCTOR functions, before main().
void register_command(const esp_console_cmd_t * command)
{
  fprintf(stderr, "Register command called.\n");
  fflush(stderr);
  // Configure the console, so that we can register commands to it.
  if ( !did_initialize ) {
    repl = 0;
    // Configure the console command system.
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.task_stack_size = 30 * 1024;
    repl_config.prompt = ">";
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    did_initialize = true;
    fprintf(stderr, "Initialized console, repl is: %lx\n", (unsigned long)repl);
    fflush(stderr);
  }

  // Register the command.
  ESP_ERROR_CHECK( esp_console_cmd_register(command) );
}
