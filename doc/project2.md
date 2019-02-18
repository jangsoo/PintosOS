Design Document for Project 2: User Programs
============================================

## Group 82 Members

* William Zhuk <williamzhuk@berkeley.edu>
* Link Arneson <arneson@berkeley.edu>
* Heesoo Jang <hejang@berkeley.edu>
* Amir Dargulov <amir_dargulov@berkeley.edu>

## Task 1: Argument Parsing

### 1. Data structures and functions
#### Edited Functions

##### 'Start Process'
- Before load, tokenize filename in to filename and args by parsing
- Calls load, grabs the filename to be run.
- Insert arguments
- using if_esp, push in right-to-left order starting with null (an decrement if_esp val)


### 2. Algorithms
- We use tokenize to parse input

### 3. Synchronization
- No synchronization, since this is inside starting thread, so it happens before the jump. The process starts after start process

### 4. Rationale 
- This part is quite straightfoward with editing start_process
- Rename args for process_execute, start_process. 
- file_name is not a good name; file_name -> input_string

## Task 2: Process Control Syscalls
### 1. Data structures and functions
#### New Structs 
##### `struct wait_status`
- list_elem elem
- struct lock ref_count_lock
- int ref_count
- tid_t tid; // child's tid
- int exit_code //child's exit code
- struct semaphore dead, // 1 if child alive, 0 if child dead

#### Edited Structs
##### 'struct thread'
**added fields:**
- struct parent_wait_status
- struct list childrens_wait_status


#### Edited Functions
##### 'thread_Create'
- initialize childrens_wait_status to empty list
- initialize parent_wait_status to null

#### 'process_wait'
- edit to go through parent's child_list and find correct tid. wait_status and have parent wait on dead sema.

##### 'syscall_handler'
- exec()
	- safely get file and arguments from user process
	- make new thread that runs process_execute correctly
	- before process_Exec actually runs, make the wait_status struct and attach it to calling thread's child list and to child thread's parent_status

- halt()
	- calls shutdown

- practice ()
	- increment argument and return

- wait()
	- safely access stack and t_tid
	- call process_wait

- exit()
	- safely get thread stack and access thread struct
	- go through all children_status structs and decrement ref_count by 1 for all children. If it is 0, free the struct
	- go to parent_status and if not null, decrement ref_count (if ref_count 0 then free), and update exit_code, then we up() dead
	- call file_allow_write()

- check user_vaddr for every character in string.
- change pagefault to ref -1



### 2. Algorithms


#### Safety Check on User Input
- check user stack pointer is not null
- check that it is `is_user_vaddr`
- for all arguments, check that they are `is_user_vaddr`
- for methods with array arguments, loop through array and check each element `is_user_vaddr` (for strings)
- if any of these fail, we terminate child process

#### Exec
- Use safety check, including the check of string passed in
- disable interrupts
- call process execute
- create a wait_struct
- assign wait struct to current process's child list and to new process's parent


#### Halt
- Just call shutdown
- (if we wanted we could safety check the stack pointer to prevent weird shutdowns but whatev)


#### Practice 
- safety check the 1 arg
- increment the arg

#### Process_Wait
- obtain thread struct via thread_current
- loop through thread's children wait_status and find one with matching tid
- down that child dead sema

#### Exit
- use safety check
- check children wait status's and decrement ref_count. If it ends up 0, also free the struct and adapt the list.
- check parent wait status and decrement ref_count. Update exit status and up the child dead sema
- thread_exit


### 3. Synchronization
Both `exec()` and `wait()` syscalls can cause synchronization problems between the parent and the child process. To prevent this, we keep a semaphore called **`dead`** to keep track of the state of the child process.
There is also the `ref_count`. The last thread that decrements the `ref_count` to zero must free the struct to clean up memory. There can be a race condition on this `ref_count`, so we keep a lock called **`ref_count_lock`** for synchronization.

Finally, when attaching these structs to the threads on thread_exec we disable interrupts to make sure those threads do not spend any time without
proper parent/child structs.


### 4. Rationale 
Safety because we need to not fail on malicious / bad user calls
We have the struct for waiting because threads can be freed but we still need to know what happened


## Task 3: File Operation Syscalls

### 1. Data structures and functions

#### Edited Functions

##### `load` in userprog/process.c
- `file_deny_write` to deny writing so no one can write to a running user process
- pass file pointer to thread's struct

##### `process_exit` in userprog/process.c
- call `file_allow_write` to enable writing to the process after exit
- use file pointer from thread struct

##### `syscall_handler` in userprog/syscall.c
- add to switch statement on `args[0]` and handle cases `SYS_CREATE`, `SYS_REMOVE`, `SYS_OPEN`, `SYS_FILESIZE`, `SYS_READ`, `SYS_WRITE`, `SYS_SEEK`

#### New Global Variables
##### `struct lock` in userprog/syscall.c
- global lock variable to prevent multiple concurrent syscalls

#### Edited Structs
##### `struct thread` in threads/thread.h
- add a file pointer to thread struct to facililitate process_exit. 

### 2. Algorithms

#### System Calls
For every file system call:
1. Check user stack validity and pointer validity using the same process as in part 2 
2. Acquire global lock on file system
3. Invoke correct function for syscall with correct arguments (see below for individual cases)
4. Release lock on file system

-`SYS_CREATE`: `filesys_create()` in filesys.c
-`SYS_REMOVE`: `filesys_remove()` 
-`SYS_OPEN`: `filesys_open()`
-`SYS_FILESIZE`: `file_length()` in file.c
-`SYS_READ`: `file_read()`
-`SYS_WRITE`: `file_write()`
-`SYS_SEEK`: `file_seek()`


#### Disallowing reads and writes
In order to disallow writes on a currently running program, we call `file_deny_write()` on that file when the process is loaded. We pass the pointer to that file into the thread struct so we can keep track of it in order to 
allow writes again when the process is complete. We do this by calling `file_allow_write()` on the pointer in the thread struct in `process_exit`.

### 3. Synchronization
We use a global lock on the file system in order to make sure that only one syscall operation happens at a time.

### 4. Rationale 
We follow the suggestion to use a global lock because it seems the easiest way to do it.  Most of the filesystem syscalls are already implemented so we don't have to do a lot of work. We chose to use the already implemented `file_deny_write` when the program is loaded and allow writes again when it exits. We didn't have a good way to access the file name once it was running though so we added a member to thread to store it. A thread will only ever have one executable that it corresponds to. Two executables can correspond to the same file, which we initially thought would cause a bug if two or more threads run the same executable, because when you exit out of the first file, it will allow writes on the file while the second is still running. However then we looked in the underlying implementation in inodes and it handles multiple files.


## Design Document Additional Questions:
1. Name of the test : bad-jump.c
In line 14, we set the address of the functionptr fp to address 0, which is not mapped. Generally, ESP will change during function calls as the function pointed to by fp is called, but our test must exit with 0 in the test case because of null pointer.

2. Name of the test: boundary.c
In line 30 with strlcpy(),  we copy over data that would go beyond one virtual page to another. We do this by first fetching data size of a page, and then we shift p until copying over will cross the page boundaries.

3. Part 3 has a requirement that currently executing files cannot be written to. This is not tested. A test case would involve calling exec on a file, and then attempting to write to that file. The executed file will just tell the thread to sleep for a while. We pass if bytes written is 0, fail if anything else.

4.
	1. 
	```
	(gdb) print thread_current()
	$1 = (struct thread *) 0xc000e000
	(gdb) print *thread_current()
	$2 = {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee0c "
	\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0
	104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, mag
	ic = 3446325067}

	(gdb) dumplist &all_list thread allelem
	pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeat
	s 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0
	xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <r
	eady_list+8>}, pagedir = 0x0, magic = 3446325067}
	pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeat
	s 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all
	_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, m
	agic = 3446325067}
	```
	2.
	```
	(gdb) bt                    
	#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
	tid_t process_execute (const char *file_name) {
	#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
	process_wait (process_execute (task));
	#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
	a->function (argv);
	#3  main () at ../../threads/init.c:133
	run_actions (argv);
	```
	3.
	```
	(gdb) print thread_current()
	$3 = (struct thread *) 0xc010a000
	(gdb) print *thread_current()
	$4 = {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4
	 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0
	034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

	(gdb) dumplist &all_list thread allelem
	pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeat
	s 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next =
	 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>}, pagedir = 0x
	0, magic = 3446325067}
	pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeat
	s 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, el
	em = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 344632
	5067}
	pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000
	\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <
	all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0
	, magic = 3446325067}

	```
	4. Created in thread.c at line 424, but the actual thing being run is args.c with no arguments passed in, I think from make file.
	` function (aux);       /* Execute the thread function. */ `

	5. 0x0804870c according to btpagefault

	6.
	```
	(gdb) btpagefault
	#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading varia
	ble: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
	```

	7. Pintos does not know where to look on the stack for the arguments passed in.
