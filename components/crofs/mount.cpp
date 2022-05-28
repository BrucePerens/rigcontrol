#include <stdio.h>
#include <stdint.h>
#include "crofs.h"
extern const uint8_t crofs_image_start[] asm("_binary_crofs_image_start");
extern const uint8_t crofs_image_end[]   asm("_binary_crofs_image_end");
extern uint32_t crofs_image_length;


extern "C" void
mount_crofs()
{
  const crofs_config_t cfg =
  {
    .fs_addr = crofs_image_start,
    .fs_size = crofs_image_length,
    // FIX: Use a menuconfig parameter here.
    .base_path = "/crofs",
    .open_files = 4
  };
  CroFS crofs;

  if (crofs.init(&cfg) != 0)
  {
    fprintf(stderr, "CroFS initialization failed.\n");
    fflush(stderr);
    abort();
  }
}
