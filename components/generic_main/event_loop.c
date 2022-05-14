#include <stdlib.h>
#include <string.h>
#include <esp_event.h>
#include "generic_main.h"

ESP_EVENT_DEFINE_BASE(GM_EVENT);

static void
create_user_event_loop(esp_event_loop_handle_t * loop, const char * name, int core)
{
  esp_event_loop_args_t	args = {};

  args.queue_size = 10;
  args.task_name = name;
  args.task_priority = 0;
  args.task_core_id = core;
  args.task_stack_size = 10 * 1024;

  ESP_ERROR_CHECK(esp_event_loop_create(&args, loop));
}

static void
handle_run_event(void * handler_arg, esp_event_base_t base, int32_t id, void * event_data)
{
  gm_run_data_t * run = (gm_run_data_t *)event_data;

  (run->procedure)(run->data);
}

void
gm_start_user_event_loops(void)
{
  // Since these event handlers are never un-registered, we can throw away the instance
  // data.
  esp_event_handler_instance_t	slow_run_handler;
  esp_event_handler_instance_t	medium_run_handler;

  create_user_event_loop(&GM.medium_event_loop, "generic main: medium-speed job runner", 0);
  esp_event_handler_instance_register_with(&GM.medium_event_loop, GM_EVENT, GM_RUN, handle_run_event, 0, &medium_run_handler);
  create_user_event_loop(&GM.slow_event_loop, "generic main: slow job runner", 1);
  esp_event_handler_instance_register_with(&GM.slow_event_loop, GM_EVENT, GM_RUN, handle_run_event, 0, &slow_run_handler);
}
