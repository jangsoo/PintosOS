#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


#include <lib/kernel/list.h>
#include "threads/thread.h"

void syscall_init (void);

int open_helper (const char *file);


static struct file_descriptor*
get_file_descr_for_fd(int fd, struct list *fds) {
  struct file_descriptor *current_descriptor;
  struct list_elem *cur;
  for (cur = list_begin(fds); cur != list_end(fds); cur = list_next(cur)) {
    current_descriptor = list_entry(cur, struct file_descriptor, elem);
    if (current_descriptor->fd == fd) {
      return current_descriptor;
    }
  }
  return NULL;
}

static struct file*
get_file_for_fd(int fd, struct list *fds) {
  struct file_descriptor* file_descriptor = get_file_descr_for_fd(fd, fds);
  return file_descriptor == NULL ? NULL: file_descriptor->file;
}


// bool mkdir(const char *dir);
#endif /* userprog/syscall.h */
