#include <sys/random.h>
#include "generic_main.h"

size_t gm_choose_one(size_t number_of_entries)
{
  size_t random;

  getrandom(&random, sizeof(random), 0);
  
  return random % number_of_entries;
}
