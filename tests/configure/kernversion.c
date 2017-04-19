#include <stdio.h>

#include "autoconf.h"
#include "generated/uapi/linux/version.h"

int main()
{
  printf("%u", LINUX_VERSION_CODE);

  return 0;
}
