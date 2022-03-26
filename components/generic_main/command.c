#include <stdlib.h>
#include <string.h>
#include "generic_main.h"
#include <esp_console.h>
#include <driver/uart.h>
#include "linenoise/linenoise.h"

static Array * array = 0;

// This is called from CONSTRUCTOR functions, before main().
void
command_register(const esp_console_cmd_t * command)
{
  if ( !array ) {
    array = array_create();
  }
  array_add(array, (const void *)command);
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
command_add_registered_to_console(void)
{
  size_t size;

  if (!array || (size = array_size(array)) == 0) {
    fprintf(stderr, "No commands registered.\n");
    return;
  }
  
  // Alphabetically sort the commands.
  qsort(array_data(array), array_size(array), sizeof(void *), compare);

  for ( size_t i = 0; i < size; i++ ) {
    const esp_console_cmd_t * c = (const esp_console_cmd_t *)array_get(array, i);

    ESP_ERROR_CHECK(esp_console_cmd_register(c));
  }
  array_destroy(array);
}
