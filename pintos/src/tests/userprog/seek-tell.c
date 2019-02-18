/* Tests the filesize, tell and seek */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
	int handle = open ("sample.txt");
	if (handle < 2)
    	fail ("open() returned %d", handle);
 	msg("filesize: %d", filesize(handle));
 
 	char buf[100];

 	//tell pos 0
 	msg("tell: %d", tell(handle));
 	// seek to middle 100
 	seek(handle, 100);
 	//tell pos 100
 	msg("tell: %d", tell(handle));
 	// read some bytes 100
 	read(handle, buf, 100);
 	// tell 200
 	msg("tell: %d", tell(handle));
 	// seek past eof (should not error) (filesize + 100)
 	seek(handle, filesize(handle) + 100);
 	// tell filesize + 100 
 	msg("tell: %d", tell(handle));
 	//read (should return 0 bytes and not advance pointer)
 	msg("%d bytes read", read(handle, buf, 100));
 	msg("tell: %d", tell(handle));
}
