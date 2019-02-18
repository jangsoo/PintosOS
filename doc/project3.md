Design Document for Project 3: File System
==========================================

## Group 82 Members

* William Zhuk <williamzhuk@berkeley.edu>
* Link Arneson <arneson@berkeley.edu>
* Heesoo Jang <hejang@berkeley.edu>
* Amir Dargulov <amir_dargulov@berkeley.edu>

## Task 1: Buffer Cache
### Data Structures and Functions
```
struct cached_block {
  block_sector_t sector,      //sector represented by this cached block
  block* block,               //block device represented
  bool dirty,                 //whether block is dirty or not
  void* loc_in_mem,           //pointer to some location in memory for that block
  struct list_elem elem,      //for use in block_cache
  struct lock block_lock,     //for reading/writing to cache synchronizatio
}
```

`struct list block_cache`
  - contains 64 `cached_block` elements

`struct lock global_cache_lock`
  - lock over entire cache for eviction / looping
  
`struct lock free_map_lock`
  - lock for using free map so bad things about freeing / unfreeing blocks don't happen
  
`filesys_done`:
  - flush cache to disk and free
  
`get_cache_block(block_sector_t)`
  - ASSERT we have `global_cache_lock`
  - loop through `block_cache` in search of matching section and block
  - if not present, loop from back to front to find first unpinned `cached_block` to evict. No contendors means we wait on last `cached_block` and evict when we wake.
  - move evicted block to front of `block_cache` and fill in correct metadata (not dirty)
  - disable interrupts
  - release `global_cache_lock`
  - acquire `block_lock`
  - enable interrupts
  - if was evicted block, write from disk into memory

`block_read`:
  - acquire `global_cache_lock`
  - call `get_cache_block(block_sector_t)` to get `struct cache_block` with matching sector (and possibly evict in process)
  - read from data to buffer
  - release `block_lock`
 
`block_write`:
  - acquire `global_cache_lock`
  - call `get_cache_block(block_sector_t)` to get `struct cache_block` with matching sector (and possibly evict in process)
  - copy from buffer to cache, set dirty to true
  - release `block_lock`
  
`block_cache_init`:
  - initialize the `block_cache` list with 64 `cached_block` structs with block pointers set to `NULL` to symbolize not representing anything.
  - will malloc 64 block-sized chunks into memory (to be freed on shutdown) for each of those `cached_block`
  - initialize `global_cache_lock`

`filesys_init`:
  - will call `block_cache_init`
  
### Algorithms
  
cache storage:
  - we store using LRU by position in the `block_cache` list.
  - Since the cache size is 64, time complexity of searching through entire list is very small so LRU works well
  - We initialize the cache to 64 empty `cached_blocks` with only `loc_in_mem` malloced and initialized, and with a guarenteed `block` as `NULL` and `dirty` as `false` so we know that it is unused and will not match anything we write/read to.
  
### Synchronization

We have a global cache lock in order to synchronize evicts
We have a lock on every cache block so that we do not have bad read/writes.

### Rationale

Clock was an alternative to LRU cache, but it would be harder to implement and would not save a lot of time over 64 loops through main memory access.

Using a non-list structure for the cache is possible, but since we need to check presence we would either need a hash-based table or something that will take N time to loop through. Hash based tables is an option that we have available, but the extra complexity in working with it plus messing with hashing feels like more work than we will get out of a simpler implementation that still runs fairly quickly.

## Task 2: Extensible Files

### Data Structures and Functions
All modifications will take place in inode.c, except `free_map_lock`.

#### Modified Data Structures
##### `struct inode_disk`
```C
struct inode_disk {
  off_t length;
  unsigned magic;
  block_sector_t direct_ptrs[125];
  block_sector_t doubly_indirect_ptr;
};
```
- convert `unused` into some direct, some indirect, one doubly-indirect ptrs
- length should reflect length of file?

##### `struct inode`
- remove `data`
- add a lock

#### New Data Structures
##### `struct indirect_disk_block`
```C
struct indirect_disk_block {
  block_sector_t blocks[128];
};
```

#### Modified Functions
##### `inode_write_at`
- extend file if `pos` > EOF
- first fit block allocation (as in FFS)
- locks 

##### `inode_read_at`
- change how we read files to hop around pointers
- locks
- remove bounce buffer

##### `inode_create`
- call `inode_block_allocate` to allocate initial space

##### `inode_length`
- all inodes are now the same length, the exact size of the sector
- remove this function

##### `byte_to_sector`
- edit to follow pointers and return correct block

##### `inode_close`
- free all allocated blocks associated with inode

#### New Functions
##### `inode_block_allocate`
- new function
- takes in some byte number beyond end of file
- checks to see if there is room in `free-map`
- allocates blocks up to that point, adjusting `inode`'s pointers


### Algorithms
The main modification we make to the filesystem is to change the structure of inodes. We model the structure after FFS, with some direct, indirect, and doubly indirect nodes.

#### Reading from files
First check `length` to see if the `pos` is beyond EOF. If it is, return 0 bytes.
We use `byte_to_sector` to get the beginning byte to read. We use `block_read` to read from the disk directly into the buffer. When we reach the end of the sector, we call `byte_to_sector` on the next byte to get the next page and continue reading into the buffer.

#### Writing to files
First, check to see if the ending position (start point + length of data) is beyond the end of the file. Call `inode_block_allocate` to allocate all the blocks you need. Check to make sure allocation worked.
Use `byte_to_sector` to determine the start point, which should be allocated. If start point is beyond EOF, write zeroes up to start point, using `byte_to_sector` as in read to determine the next sector. Then write data.
Upon success, update `length` of file.

#### Allocating blocks
This happens when creating a file, or when writing past the end of a file. It will take in `end_pos` which is the last byte that needs to be written. 
The function will calculate the number of new blocks that need to be allocated, and if it won't fit, abort.
If the next block that needs to be allocated is a direct block, the function will call `free_map_allocate` with a size of 1 to obtain the next free block, then point to the block within the inode. If it is an indirect block, it first needs to check if there already is an indirect block. If not create one, then allocate data block and link it in as above.

#### Byte to sector
This function will use math to figure out whether the byte will be in a direct, indirect, or doubly indirect block. If it is in an indirect block, it will follow the pointers and return the data block that contains the requested byte. This will work for bytes beyond EOF, but NOT those outside of blocks that have been allocated, so should be accompanied with a check.

#### Number of blocks
125 direct pointers, each to 1 block = 125 blocks.

1 doubly-indirect pointer to 1 doubly-indirect disk block (containing only pointers), which points to 128 indirect blocks = 128 * 128 blocks.

(125 + 128 * 128) * 512 > 8 MiB

### Synchronization
We add a lock to the in-memory inodes to ensure that only one thread can read or write from the same inode concurrently. This will not cause deadlock because no thread performs more than one i/o operation at a time, and the lock is released as soon as the read or write is finished. 
We also put a global lock on the `free-map`, in order to make sure that we don't allocate the same block to two different files. We acquire the lock the the beginning of `inode_block_allocate` and free it at the end of that function. This way, we make sure that there are no incosistencies between the time we check that we have enough space, and the time we allocate those blocks and add the pointers to the inode.

### Rationale
While we could have had seperate locks and semaphores for reading and writing (ie allow multiple readers) it's not required by the spec so we went with the simpler solution.
We chose to remove bounce buffers to make writing easier to implement.
We decided to support sparse files because it's also simpler to implement.
The way we use `byte_to_sector` to get the next sector to read is a little inefficient for indirect blocks, because it involves an extra read compared to keeping the pointer to the intermediate block. But we think that it is better to have a single function that can handle the translation of bytes to sectors. Also, the indirect block will likely be in the cache (`block_read` goes through cache), so it won't be that bad.
We debated having a new function to allocate multiple blocks, other than calling `free_map_allocate` with a size of 1, but it would be way more trouble than it's worth to keep track of a list of pointers.
We decided to have only one doubly-indirect pointer and no indirect pointers to make life easier.

## Task 3: Subdirectories

### Data structures and functions
#### Edited Structs
##### 'struct thread'
**added fields:**
dir* working_dir;

##### 'struct file_descriptor'
**added fields:**
bool isdir;

#### Edited Functions
##### `bool chdir(const char *dir)`
- We have to set pwd and working_dir for the thread struct, but we should traverse first to see if given argmuent is valid path.
- check if dir starts with \/
	-if dir starts with \/, the path is absolute, so start folloing loop from root
	-if dir does not start with \/, dir is relative, start loop from current directory at thread struct.
- In a while loop to traverse down,
	- insert a dir struct in lookup() to check the next directory in path
		- if lookup returns false, path is invalid so return false.
		- if lookup returns true, update the while loop and traverse down.
- once we hit last directory, we update the working_dir in thread struct, return true

##### `bool mkdir(const char *dir)`
- determine if path is absolute or relative, parse up path
- as in chdir, traverse down the directories
	- fail if any intermediate directory does not exist
	- fail if last directory exists
- do dir_create()
- Do safety checks along the way to verify path.
- Do dir_create() with 2 entries, add dir_entry to the parent, set in_use to true
- go down to the created directory, dir_add two entries . and .., link the sector number to itself and the parent dir, respectively

#####  `bool isdir(int fd)`
- use `get_file_descr_for_fd` to get the file_descriptor struct.
- check for boolean field isdir.


##### `int open (const char *file)`
- As in chdir, determine if file is relative or absolute
- Traverse to right location, fail if lookup during traveral returns false
- Between getting the fd from current thread and doing filesys_open(filename), we must add a check to see if isdir(fd). 
- If it is a directory we do `dir_open`, but if it is not a directory, we do a filesys_open(file), like we used to.

##### `void close(int fd)`
- we check isdir(fd), and if it is not a directory, se do a file_close.
- if it is a directory, we do dir_close() 

##### `pid_t exec(const char *file)`
- make sure it works for absolute/relative paths
- if file_descriptor's isdir boolean is set true, exec should fail.

##### `bool remove(const char *file)`
- traverse to right directory using appropriate absolute/relative paths
- check if file is directory
- if it is not a directory, do SYS_REMOVE
	- check if there are dir_entries left in the directory other than . and ..
		- set in_use false if dir becomes empty
- if target is a directory, check in_use, do dir_remove from parent only if in_use is false.


##### `int read(int fd, void * buffer, unsigned size)`
- checks file_descriptor using `get_file_descriptor_for_fd`, if it isdir, read fails.


##### `int write(int fd, const void *buffer, unsigned size)`
- checks file_descriptor. If isdir, write fails

##### `int inumber(int fd)`
- use `get_file_descr_for_fd` to get fd -> file_descriptor. Then from file_descriptor, we can get inode, file_descriptor -> file -> inode. We can get sector number inode -> sector, and return it as inode number.

### Algorithms
Absolute/relative path, parsing:
	- Check if input path starts from root directory.
Directory traversal:
	- Given a pointer to a dir struct, we can fetch the inode, and from the inode, we can read and cast as dir_entry, and the casting allows us to access its fields like inode_sector and name. 
	- Traversal is also possible upwards via .. or self loop using . the pointing should be done with appropriate block_sector_t
	

### Synchronization
- No additional synchronization needed for part 3
- lock on in-memory inodes would prevent two threads working on a single file or directory simultaneously. 

### Rationale
I decided to keep current working directory of dir struct inside thread struct because the struct would be used when doing traversal for relative addressing.
It made sense to keep a boolean isdir field inside file_descriptor struct because most functions that check the directory have fd inputs.


## Extra Question
### Read Ahead
- in our code, both `block_read` and `block_write` use a helper function (`get_cache_block`) in order to properly loop through the cache and evict if necessary.
- in read & write, we will call `thread_create` with the argument of a new function, we will call `grab_block` that will try to find nearby block sectors in the cache. This can have 1 thread for a new block, or many threads to grab surrounding blocks. `grab_block` will be two lines of code: a call to `get_cache_block` and then a release of the `block_lock` that the thread owns.

### Write Behind
- assuming alterations from project 1 are valid, you can add to `timer_interrupt` a `create_thread` call that will call a function that will loop through the cache and check if a `cache_block` is not currently used by anyone (`try_acquire` locks). Unused cache blocks that are dirty will be written to disk and their dirty bit will be set to 0. During loop `global_cache_lock` is held, but right before every write it is released and the lock of that `cached_block` is held instead. Right before the write, it double checks that it is dirty (maybe earlier thread doing same job just did this). Then it re-aquires the global lock, releases the block lock, and continues to next block.
  
 
