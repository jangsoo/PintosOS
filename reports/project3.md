Final Report for Project 3: File System
=======================================

## ASUNA

## Changes since design doc

- Added two different kinds of new block methods for cached access, one allowing for offset specification (used in read and write) and one that just sets the entire block at once (useful for inode create with 0s and other block-based methods)
- Added a lot of ASSERT statements to freemap calls to enforce proper locking-usage of freemap, and changed code around to make sure this usage was being properly followed. 

- Added a helper function for extensible files that deals with complications of allocating new blocks. Used both in inode_create and inode_write.
- Changed number of direct nodes down by 1 to account for the usage of is_dir uint on inode disk

- The design doc only had isdir field in file descriptor struct, but we added isdir field on inode struct and disk-inode struct as well.
- To implement `inumber` syscall, we added pointer to inode as a field to file secriptor struct. 
- In init.c during thread pool initialization, we set the current working directory of the very first thread to root, added inheritance of parent thread's cwd to the child thread at `thread create`. 
- Design Doc had no specification on `readdir` operations, which we made it iterate over the directory entries while skipping over "." and ".." entries
- Realized that `dir_lookup` opens up underlying inodes, so had to do closing behind during directory traversal to keep the open_cnt consistent.

## Reflection

- Will did parts 1, most of part 2, and debugging on 3
- Link worked on part 2 of the project.
- Heesoo did part 3 and the design doc.
- Amir did student testing code and the testing report.
- What could have gone better was communication and resources. Many members were away from Berkeley, and rare office hours over dead week made it difficult to seek help. Both of the issues could have been allieviated if Heesoo, Link, and Amir started their respective parts earlier or asked for help before the beginning of usage of slip days. 

We worked much harder than previous projects, but the task was also far more difficult.


## Student Testing Report

### Test 1: 
Our first test was for the overall efectiveness of the cache.
We clear the cache.
Read a file the first time at a certain offset.
Record the read count of how many mem. accesses were needed for that.
Close the file descriptor.
Read the same file again at the same offset.
Record the second read count we've got for memory accesses.
If cache is implemented correctly then our second result should be less than the first one, and that is what we check for in our test.


### Test 2: 
For our second test we check how many metadata accesses are made upon a mem. access.
We clear the cache.
We document the read and write counts to the memory.
We do a number (X) of random writes to the file.
We document the read and write counts to the memory.
We see if the difference between documented read counts is lower (as it is supposed to be) than 3 * X (number of writes we did) (because that is the cap we allow for reads for metadata pulling)
We see if the difference between documented write counts is lower than 2 * X (because that is the cap we allow for writes for metadata pulling)

## Pintos Testing
We had number of issues using GDB on pintos. In fact, we somehow had different result running same code on different machines of our memebers. The persistence testing was also very difficult, as we could not step through it with a debugger.
Also, it would be much more helpful if it were easier to test these cases in IDE's instead of gdb/cgdbs. The IDE we used required CMake, so we had to try debugging across dozens of test files using gdb, which made navigating difficult. 
