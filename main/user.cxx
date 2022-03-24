#include <stdio.h>
#include <esp_log.h>

void user_startup(void)
{
  printf("\n*** K6BP RigControl ***\n");
  // esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set("*", ESP_LOG_INFO);
}
