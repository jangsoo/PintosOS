/* Reads from file should be more effective with caching */

#include "tests/filesys/seq-test.h"
#include "tests/main.h"
#include <syscall.h>
#include "tests/lib.h"

#define FILE_NAME "myfile.txt"
#define SIZE 2048

static char buf[SIZE];

static size_t
return_block_size (void) 
{
  return 162;
}

void
test_main (void) 
{
  int fd;
  size_t offsett;
  unsigned zero, first, second;

  seq_test (FILE_NAME,
            buf, sizeof buf, sizeof buf,
            return_block_size, NULL);
  
  msg ("clearing the cache");
  clearcache ();

  // msg ("read count zero");
  zero = readcnt ();

  CHECK ((fd = open (FILE_NAME)) > 1, "open \"%s\"", FILE_NAME);

  offsett = 0;
  msg ("reading \"%s\" first time", FILE_NAME);
  for (offsett = 0; offsett < sizeof buf; offsett++)
    {
      if (read (fd, buf + offsett, 1) != (int) 1)
        fail ("read 1 byte at offset %zu in \"%s\" failed",
              offsett, FILE_NAME);
    }

  msg ("read count first");
  first = readcnt ();

  msg ("close \"%s\"", FILE_NAME);
  close (fd);

  CHECK ((fd = open (FILE_NAME)) > 1, "open \"%s\"", FILE_NAME);

  offsett = 0;
  msg ("reading \"%s\" second time", FILE_NAME);
  for (offsett = 0; offsett < sizeof buf; offsett++)
    {
      if (read (fd, buf + offsett, 1) != (int) 1)
        fail ("read 1 byte at offset %zu in \"%s\" failed",
              offsett, FILE_NAME);
    }
  
  msg ("read count second time");
  second = readcnt ();

  msg ("close \"%s\"", FILE_NAME);
  close (fd);

  if (first - zero <= second - first)
    fail ("read count to first (%d) not more than read count to second (%d)",
          first - zero, second - first);
  msg ("read count to first more than read count to second");
}
