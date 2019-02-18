/* Checks filesize */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  create("example.txt", 128);
  int fd = open("example.txt");

  if (filesize(fd) != 128) { 
  	fail("failed to return the right filesize");
  }


}


