#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int t = uptime();   
  printf("%d ticks\n", t);
  exit(0);
}

