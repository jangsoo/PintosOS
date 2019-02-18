Final Report for Project 2: User Programs
=========================================
## Changes since design doc

- Design doc did not mention how to properly do file descriptor table, so we implemented that through a 500 element array that was kept on kernel thread memory.
- portioned every syscall out into a helper function with the name of the syscall which would call file / filesys functions
- in system write call, we needed to NOT create a kernel memory slot of buffer to write, because when done the copied buffer differs always. Also if you do not check it as a string then it will also blow up, unlike read. This is real strange and a TA was unable to explain why this was the case.
- a `user_exit` function was created to correctly set eax, print the thread exiting, and set exit_status inside of syscall. Some of this code was also necessary in exception.c but we did not change syscall header file to transfer over 4 lines of code.
- Also we did not mention any edit to exception.c since we did not understand we needed to do that at the time
- In `process.c`, `process_execute` we needed to check for a space in file_name and grab everything before the space when naming the thread. This could have been made better by editing args of `start_process` to allow for a passing of a tokenized name + number of tokens and having tokenization occur in `process_execute`, but that felt too convoluted.

## Reflection

- Will did parts 1 and 2, and the safety checks for part 3.
- Heesoo did test case 1, parts 1 and 2 of the design doc, and debugging for Part 3.
- Link did test case 2 and part 3 of the design doc.
- Amir did part 3.
What could have gone better is if we didn't have to do this project over spring break, because we didn't work on it over most of break, and we had to rush it at the end. Overall our communication was a lot better though, which enabled us to finish it quickly. Also, workload was more evenly spread.

## Student Testing Report

### Test 1: Filesize
- This test checks that `filesize` is implemented. I chose this test case because it was not tested while other syscalls like `open`, `read`, `write` were extensively tested. This was despite the fact that `filesize` was used inside other functions, mostly for safety checks.
- This test creates a file of fixed size, opens it to get the file descriptor, and calls `filesize` with the file descriptor as an argument. The test then compares the output of `filesize` with the initial size input for file creation. The output of `filesize` should be equivalent to the filesize put in as input to create.
- One potential bug is that if kernel fails to open the file and returns -1, `filesize` will fail the sanity check inside the function call. `filesize` would return -1, and the output would trigger the test to fail.
- Another case is if the kernel thread keeps the wrong fd in the current_thread->fds[fd], which can be possible if there is an off-by-one bug. In this case, the `filesize` will return the size of a different file, and the incorrect filesize will cause the test to fail.

#### filesize.output
```
Copying tests/userprog/filesize to scratch partition...
qemu -hda /tmp/U4P0wZFxsZ.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run filesize
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  209,510,400 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 167 sectors (83 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 101 sectors (50 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'filesize' into the file system...
Erasing ustar archive...
Executing 'filesize':
(filesize) begin
(filesize) end
filesize: exit(0)
Execution of 'filesize' complete.
Timer: 56 ticks
Thread: 0 idle ticks, 56 kernel ticks, 1 user ticks
hda2 (filesys): 87 reads, 211 writes
hda3 (scratch): 100 reads, 2 writes
Console: 878 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```
#### filesize.result
```
PASS
```


### Test 2: Seek-Tell
- This test checks that `seek` and `tell` are implemented correctly. I chose this test case because there were no tests in userprog that test any of these explicitly, even though they are part of task 3, although seek is required to be implemented for some other tests to work correctly.
- The test opens up sample.txt, and calls `tell` after doing various operations. It calls `tell` before anything, then `seek`s to the middle, then reads some bytes, then `seek`s past the end of the file, then attempts to `read` which should fail gracefully.
- One bug is if the kernel fails to open the file (ie thinks it does not exist or is empty) and instead returns -1. The output would be an immediate fail as the test checks to see if a file descriptor is returned.
- Another bug is if the kernel failed to open the file, but still returned a viable file descriptor, as if it were an empty file.  So what would happen is tell would output 0 at the beginning, 100 because seek would still go beyond the end of a file, then 100 again because it wouldn't read any bytes, then 100 again because it would go to filesize=0 + 100, then print 0 bytes read, then tell 100 again. 

#### seek-tell.output
```
Copying tests/userprog/seek-tell to scratch partition...
Copying ../../tests/userprog/sample.txt to scratch partition...
qemu -hda /tmp/nBjBWYlErz.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run seek-tell
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  405,913,600 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 167 sectors (83 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 105 sectors (52 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'seek-tell' into the file system...
Putting 'sample.txt' into the file system...
Erasing ustar archive...
Executing 'seek-tell':
(seek-tell) begin
(seek-tell) filesize: 239
(seek-tell) tell: 0
(seek-tell) tell: 100
(seek-tell) tell: 200
(seek-tell) tell: 339
(seek-tell) 0 bytes read
(seek-tell) tell: 339
(seek-tell) end
seek-tell: exit(0)
Execution of 'seek-tell' complete.
Timer: 59 ticks
Thread: 0 idle ticks, 58 kernel ticks, 1 user ticks
hda2 (filesys): 93 reads, 216 writes
hda3 (scratch): 104 reads, 2 writes
Console: 1089 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

#### seek-tell.result
```
PASS
```

## Pintos Testing
It took us a while to understand how the pintos testing works, as it uses `msg()` and `fail()`, as well as `CHECK` macros instead of more commonly used print statements and asserts. The fact that it spans across so many files meant that it was hard to trace sequence of instructions via gdb/cgdb. Many test cases, along with slow test executions meant that it took 3~5 minutes to compile and run tests after minor changes.
For further improvement, it should be more evident how to run a single, specific test case so the user does not have to wait long time for running 80+ tests.
Also, it would be much more helpful if it were easier to test these cases in IDE's instead of gdb/cgdbs. The IDE we used required CMake, so we had to try debugging across dozens of test files using gdb, which made navigating difficult. 
