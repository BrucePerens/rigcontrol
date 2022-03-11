#include <stdio.h>
// #include "commands.h"

extern void install_param_command(void);
extern void install_restart_command(void);

void install_commands(void)
{
  install_param_command();
  install_restart_command();
}
