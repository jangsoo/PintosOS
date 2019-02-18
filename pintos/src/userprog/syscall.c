#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include <string.h>
#include <threads/malloc.h>
#include <lib/kernel/stdio.h>
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "pagedir.h"
#include "filesys/off_t.h"
//  I added
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/free-map.h"


static void syscall_handler(struct intr_frame *);

static void check_stack_and_number(struct intr_frame *f, uint32_t *reserved_space);

static void check_args(struct intr_frame *f, int num_args, uint32_t *reserved_space);

static char *check_string(char *pointer, struct intr_frame *f);               //returns string w/ null terminator
static char *check_stringl(char *pointer, size_t sz, struct intr_frame *f); //sz does not include null terminator
static void user_exit(int exit_code, struct intr_frame *f);



void seek (int fd, unsigned position);
int write(int fd, void *buffer, unsigned length, struct intr_frame *f);
bool create (const char *file, unsigned initial_size);
int open_helper (const char *file);
unsigned tell (int fd);
int read (int fd, void *buffer, unsigned length);
int filesize (int fd);
void close (int fd);
bool chdir(const char* dir);
bool mkdir(const char *dir);
int isdir(int fd);
int inumber(int fd);

struct lock filesys_lock;






static bool is_valid_pointer(void *pointer) {
  uint32_t *pd = thread_current()->pagedir;
  return pointer != NULL && is_user_vaddr(pointer) && pagedir_get_page(pd, pointer) != NULL;
}

void
syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void check_stack_and_number(struct intr_frame *f, uint32_t *reserved_space) {
  if (!is_valid_pointer(f->esp)) // check on stack pointer
    user_exit(-1, f);
  if (!is_valid_pointer((f->esp) + 3)) //check on passed in call number (ends on 3rd byte)
    user_exit(-1, f);
  reserved_space[0] = *((uint32_t *) (f->esp));
}

static void check_args(struct intr_frame *f, int num_args, uint32_t *reserved_space) {
  int i;
  for (i = 1; i <= num_args; ++i) {
    if (!is_valid_pointer(f->esp + 3 + 4 * i))
      user_exit(-1, f);
    reserved_space[i] = *((uint32_t *) (f->esp) + i);
  }
}

static char *check_stringl(char *pointer, size_t sz, struct intr_frame *f) {
  size_t size = 1;
  char *p = pointer;
  while (is_valid_pointer(p)) {
    if (p[0] == '\0' || sz == 0) {
      char *ret = malloc(size * sizeof(char));
      strlcpy(ret, pointer, size);
      return ret;
    }
    p++;
    size++;
    sz--;
  }
  user_exit(-1, f);
  return 0;
}

static char *check_string(char *pointer, struct intr_frame *f) {
  return check_stringl(pointer, PGSIZE - 1, f);
}

static void user_exit(int exit_code, struct intr_frame *f) {
  f->eax = exit_code;
  printf("%s: exit(%d)\n", &thread_current()->name, exit_code);
  thread_current()->parent_wait_status->exit_code = exit_code;
  thread_exit();  //calls process_exit, which was edited
}



/* This is for part 3 of proj.2 */



int
open_helper(const char *file) {
	if (strlen(file) == 0 ) {return -1;}
	if (strcmp(file, "/") == 0){ // edge case
		struct file_descriptor *file_desc = malloc(sizeof(struct file_descriptor));
		file_desc-> fd = thread_current()->next_descriptor;
    file_desc-> inode = dir_get_inode(dir_open_root());
		file_desc->isdir = true;
		list_push_back(&thread_current()->fds, &file_desc->elem);
 		thread_current() ->next_descriptor++;
  		return file_desc->fd;
	}
  

  char* last_name = malloc(strlen(file) + 1);

  struct dir* parent = traverse(file, &last_name); //dir parent should be second to last
  if (parent == NULL) {return -1;}

  struct inode* childnode;


  if (!dir_lookup(parent, last_name, &childnode)) {
    dir_close(parent);
    return -1; //childptr points to last inode. Last node must be kept opened. 
  }
  dir_close(parent); // close last parent, no longer used.
  // last child still open, should be kept open.

  struct thread *current_thread = thread_current();
  int fd = current_thread->next_descriptor;


  struct file_descriptor *file_desc = malloc(sizeof(struct file_descriptor));
  file_desc->fd = fd;


  if (inode_get_isdir(childnode)) { // if last is directory
    file_desc->inode = childnode;
    file_desc->isdir = true;
  } else { // if last is not directory
    struct file *f = file_open(childnode); // this does not call inode open
    if (f == NULL) { return -1; }
    file_desc->file = f;
    file_desc->inode = file_get_inode(f);
    file_desc->isdir = false;
  }

  list_push_back(&current_thread->fds, &file_desc->elem);
  current_thread -> next_descriptor++;
  return fd;
}


unsigned
tell(int fd) {
  if (fd > STDOUT_FILENO) {
    struct file *fp = get_file_for_fd(fd, &thread_current()->fds);
    if (fp == NULL) return 0;
    return file_tell(fp);
  }
  return 0;
}

bool
create(const char *file, unsigned initial_size) {
  return filesys_create(file, (off_t) initial_size, false);
}

void
seek(int fd, unsigned position) {
  if (fd > STDOUT_FILENO) {
    struct file *fp = get_file_for_fd(fd, &thread_current()->fds);
    if (fp == NULL) return;
    file_seek(fp, (off_t) position);
  }
}

int write(int fd, void *buffer, unsigned length, struct intr_frame *f) {

  if (fd == STDOUT_FILENO) {
    putbuf(buffer, length);
    return length;
  }
  if (fd > STDOUT_FILENO) {
    struct file *fp = get_file_for_fd(fd, &thread_current()->fds);
    if (fp == NULL)
      user_exit(-1, f);
    if (get_file_descr_for_fd(fd, &thread_current()->fds)-> isdir) {return -1;}
    return file_write(fp, buffer, length);
  }
  user_exit(-1, f);
  return -1;
}

int
read(int fd, void *buffer, unsigned length) {

  off_t bytes_read = 0;

  if (fd == STDIN_FILENO) {
    uint8_t *read_buf = (uint8_t *) buffer;
    unsigned i;
    for (i = 0; i < length; i++) {
      *(read_buf + i) = input_getc();
    }
    bytes_read = length;
    return bytes_read;
  }

  if (fd > STDOUT_FILENO) {
    struct file *fp = get_file_for_fd(fd, &thread_current()->fds);
    if (fp == NULL) return -1;
    return file_read(fp, buffer, (off_t) length);
  }
  return -1;
}


int
filesize(int fd) {
  if (fd > STDOUT_FILENO) {
    struct file *fp = get_file_for_fd(fd, &thread_current()->fds);
    if (fp == NULL) return -1;
    return file_length(fp);
  }
  return -1;
}


// Needed for persistence
void
close(int fd) {
  if (fd > STDOUT_FILENO) {
    struct file_descriptor *file_desc = get_file_descr_for_fd(fd, &thread_current()->fds);
    if (file_desc == NULL) return;

    if (file_desc -> isdir) { //just close the directory
    	dir_close(dir_open(file_get_inode(file_desc->file)));
      list_remove(&file_desc->elem);
      free(file_desc);
      return;
    }


    struct file *fp = file_desc -> file;
    if (fp == NULL) return;

    file_close(fp);
    list_remove(&file_desc->elem);
    free(file_desc);
  }
}


/* 
Reads a directory entry from file descriptor fd,
which must represent a directory. If successful, stores the null-terminated file name in name, which
must have room for READDIR MAX LEN + 1 bytes, and returns true. If no entries are left in the
directory, returns false.
. and .. should not be returned by readdir
*/
bool 
filesys_readdir(int fd, char* name) {
   return false;
  struct file_descriptor* file_desc = get_file_descr_for_fd(fd, &(thread_current()->fds));
  ASSERT(file_desc->isdir);
  struct dir* dir = dir_open(file_get_inode(file_desc->file));
  while(dir_readdir(dir, name)){ // while readdir has entires left
    if ((strcmp(name, ".") != 0) && strcmp(name, "..") != 0) {
      return true;
    }
  }
  return false;
}


static void
syscall_handler(struct intr_frame *f) {

  uint32_t reserved_space[4]; //TODO: MUST CHANGE THIS IF > 3 ARGS PASSED IN
  f->eax = -1;
  check_stack_and_number(f, reserved_space); //base safety check
  //printf("System call number: %d\n", reserved_space[0]);

  if (reserved_space[0] == SYS_EXIT) {
    check_args(f, 1, reserved_space);
    user_exit(reserved_space[1], f);
  }
  if (reserved_space[0] == SYS_HALT) {
    shutdown_power_off();
  }
  if (reserved_space[0] == SYS_PRACTICE) {
    check_args(f, 1, reserved_space);
    f->eax = reserved_space[1] + 1;
    return;
  }
  if (reserved_space[0] == SYS_EXEC) {
    check_args(f, 1, reserved_space);

    char *_cmd_line = (char *) reserved_space[1];
    char *cmd_line = check_string(_cmd_line, f);

    lock_acquire(&filesys_lock);
    //TODO add filesys lock acquiring.
    tid_t tid = process_execute(cmd_line);
    f->eax = tid;
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_WAIT) {
    check_args(f, 1, reserved_space);
    f->eax = process_wait(reserved_space[1]); //edited by us
    return;
  }

  //FILE SYSTEM

  if (reserved_space[0] == SYS_CREATE) {
    check_args(f, 2, reserved_space);

    char *_file = (char *) reserved_space[1];
    char *file = check_string(_file, f);

    lock_acquire(&filesys_lock);
    f->eax = create(_file, (unsigned) reserved_space[2]);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_CHDIR) {
    check_args(f, 1, reserved_space);
    char* path = (char*) reserved_space[1];

    lock_acquire(&filesys_lock);
    f->eax = chdir(path);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_MKDIR) {
    check_args(f,1, reserved_space);
    char* path = (char*) reserved_space[1];

    lock_acquire(&filesys_lock);
    f->eax = filesys_create(path, 1, true);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_ISDIR) {
    check_args(f, 1, reserved_space);
    lock_acquire(&filesys_lock);
    f->eax = isdir((int) reserved_space[1]);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0]== SYS_INUMBER) {
    check_args(f, 1, reserved_space);
    lock_acquire(&filesys_lock);
    f->eax = inumber((int) reserved_space[1]);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_REMOVE) {  ///TODO!
    check_args(f, 1, reserved_space);
    char *_file = (char *) reserved_space[1];
    char *file = check_string(_file, f);

    lock_acquire(&filesys_lock);
    f->eax = filesys_remove (file);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_OPEN) {
    check_args(f, 1, reserved_space);
    char *_file = (char *) reserved_space[1];
    char *file = check_string(_file, f);

    lock_acquire(&filesys_lock);
    int fd = open_helper((char *) _file);
    f->eax = fd;
    lock_release(&filesys_lock);
    //free(file);
    return;
  }
  if (reserved_space[0] == SYS_FILESIZE) {
    check_args(f, 1, reserved_space);

    lock_acquire(&filesys_lock);
    f->eax = filesize ((int) reserved_space[1]);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_READ) {
    check_args(f, 3, reserved_space);
    void* buffer = (void*) reserved_space[2];
    uint32_t sz = reserved_space[3];

    if(!is_valid_pointer(buffer) || !is_valid_pointer(buffer + sz)) {
      user_exit(-1, f); //TODO: Maybe not check full sz away but just how long we do it
    }

    lock_acquire(&filesys_lock);

    f->eax = read ((int) reserved_space[1], (void *) reserved_space[2], (unsigned) reserved_space[3]);

    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_READDIR) {
  	check_args(f,2, reserved_space);
  	lock_acquire(&filesys_lock);
  	f->eax = filesys_readdir((int) reserved_space[1], (char*) reserved_space[2]);
  	lock_release(&filesys_lock);
  	return;
  }


  if (reserved_space[0] == SYS_WRITE) {
    check_args(f, 3, reserved_space);
    char *_buffer = (char *) reserved_space[2];
    size_t sz = reserved_space[3] + 1; //keep track of null pointer
    char* buffer = check_stringl(_buffer, sz, f);
    lock_acquire(&filesys_lock);
    f->eax = write((int) reserved_space[1], (void *) reserved_space[2], (unsigned) reserved_space[3], f);
    lock_release(&filesys_lock);
    //free(buffer);
    return;
  }

  if (reserved_space[0] == SYS_SEEK) {
    check_args(f, 2, reserved_space);

    lock_acquire(&filesys_lock);
    seek ((int) reserved_space[1], (unsigned) reserved_space[2]);
    lock_release(&filesys_lock);
    return;
  }
  if (reserved_space[0] == SYS_TELL) {
    check_args(f, 1, reserved_space);

    lock_acquire(&filesys_lock);

    f->eax = tell ((int) reserved_space[1]);
    lock_release(&filesys_lock);

    return;
  }
  if (reserved_space[0] == SYS_CLOSE) {
    check_args(f, 1, reserved_space);

    lock_acquire(&filesys_lock);

    close ((int) reserved_space[1]);

    lock_release(&filesys_lock);

    return;
  }
}



bool chdir(const char* dir) {
  if(strlen(dir) == 0) {return false;}

  struct dir* parent;
  if (dir[0] == '/') {
    parent = dir_open_root();
  } else {
    parent = dir_reopen(thread_current() -> working_dir);
  }

  char* nodesleft = malloc(strlen(dir)+1);
  strlcpy(nodesleft, dir, strlen(dir)+1);
  char* save_ptr;


  char* cur = strtok_r(nodesleft, "/", &save_ptr);
  struct inode* childptr;

  while (cur != NULL) {
    if (dir_lookup(parent, cur, &childptr) == false) {
      free(nodesleft); 
      dir_close(parent);
      return false;
    }
    if (inode_get_isdir(childptr) == 0) {
      free(nodesleft);
      dir_close(parent);
      return false;
    }
    dir_close(parent);
    parent = dir_open(childptr);
    cur = strtok_r(NULL, "/", &save_ptr);
  } // After loop, parent becomes last directory

  thread_current() -> working_dir = parent; // we musn't close this
  return true;
}


int isdir(int fd) {
  return get_file_descr_for_fd(fd, &(thread_current() -> fds)) -> isdir;
}

int inumber(int fd) {
  struct file_descriptor* descriptor = get_file_descr_for_fd(fd, &(thread_current()-> fds));
  return inode_get_inumber(descriptor -> inode);
}


