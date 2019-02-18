/* Reads from file should be more effective with caching */

#include "tests/filesys/seq-test.h"
#include "tests/main.h"
#include <syscall.h>
#include "tests/lib.h"

#define SIZE 102400
#define FILENAME "please"
#define WRITES 200
#define READ_TOLERANCE 3 // up to 3 metadata reads allowed
#define WRITE_TOLERANCE 2 // up to 1 metadata block allowed
#define BLOCK_SECTOR_SIZE 512

static char buf[SIZE];

static size_t
return_block_size (void) 
{
  return BLOCK_SECTOR_SIZE;
}

void
test_main (void) 
{
  unsigned read_before, read_after;
  unsigned write_before, write_after;

  msg ("clearing cache");
  clearcache ();

  msg ("read_cnt before");
  read_before = readcnt ();

  msg ("write_cnt before");
  write_before = writecnt ();

  seq_test (FILENAME,
            buf, sizeof buf, sizeof buf,
            return_block_size, NULL);
  
  msg ("read_cnt after");
  read_after = readcnt ();

  msg ("write_cnt after");
  write_after = writecnt ();

  if (read_after - read_before > READ_TOLERANCE * WRITES)
    fail ("read_cnt too high even for metadata");
  if (write_after - write_before > WRITE_TOLERANCE * WRITES)
    fail ("write_cnt more blocks back than needed to including metadata");
  msg ("read_cnt and write_cnt within reasonable parameters");
}
