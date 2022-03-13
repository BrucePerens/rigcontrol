#include <stdio.h>
// #include "commands.h"

extern void install_date_command(void);
extern void install_gpio_command(void);
extern void install_param_command(void);
extern void install_restart_command(void);
extern void install_tasks_command(void);
extern void install_uptime_command(void);
extern void install_wget_command(void);

void commands_component_install_commands(void)
{
  install_date_command();
  install_gpio_command();
  install_param_command();
  install_restart_command();
  install_tasks_command();
  install_uptime_command();
  install_wget_command();
}
