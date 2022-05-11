#include <stdlib.h>
#include <string.h>
#include "generic_main.h"
#include <esp_console.h>
#include <driver/uart.h>
#include "linenoise/linenoise.h"

static GM_Array * array = 0;

// This is called from CONSTRUCTOR functions, before main().
void
gm_command_register(const esp_console_cmd_t * command)
{
  if ( !array ) {
    array = gm_array_create();
  }
  gm_array_add(array, (const void *)command);
}

static int
compare(const void * _a, const void * _b)
{
  const esp_console_cmd_t * a = *(const esp_console_cmd_t * *)_a;
  const esp_console_cmd_t * b = *(const esp_console_cmd_t * *)_b;
  int result = strcmp(a->command, b->command);
  return result;
}

void
gm_command_add_registered_to_console(void)
{
  size_t size;

  if (!array || (size = gm_array_size(array)) == 0) {
    GM_FAIL("No commands are registered.\n");
    return;
  }
  
  // Alphabetically sort the commands.
  qsort(gm_array_data(array), gm_array_size(array), sizeof(void *), compare);

  for ( size_t i = 0; i < size; i++ ) {
    const esp_console_cmd_t * c = (const esp_console_cmd_t *)gm_array_get(array, i);

    ESP_ERROR_CHECK(esp_console_cmd_register(c));
  }
  gm_array_destroy(array);
}
