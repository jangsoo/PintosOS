#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <threads/synch.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/block.h"
#include "threads/thread.h"
 #include "threads/malloc.h"
#include "userprog/syscall.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{

  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  block_cache_init();
  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  lock_acquire(&free_map_lock);
  free_map_open ();
  lock_release(&free_map_lock);

}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  flush_block_cache(true);
  lock_acquire(&free_map_lock);
  free_map_close ();
  lock_release(&free_map_lock); //TODO: NOT SURE IF NECESSARY
}


// traverses filesystem path, sets last_name to last inode
// returns second-to-last directory, NULL if failure to traverse
struct dir* traverse(const char* name, char** last_name) {
	struct dir* parent;
	if (name[0] =='/') {
		parent = dir_open_root();
	} else {
		parent = dir_reopen(thread_current()->working_dir);
	}


	char* nodes_left = malloc(strlen(name)+1);
	strlcpy(nodes_left, name, strlen(name)+1);
	char* save_ptr;

	char* cur = strtok_r(nodes_left, "/", &save_ptr);
	char* next = strtok_r(NULL, "/", &save_ptr);

	struct inode* childnode;

	while(next != NULL) {
		if (!dir_lookup(parent, cur, &childnode)) {
			free(nodes_left);
			dir_close(parent);
			return NULL;
		}
		if (!inode_get_isdir(childnode)){
			free(nodes_left);
			dir_close(parent);
			return NULL;
		}
		dir_close(parent);
		parent = dir_open(childnode);

		cur = next;
		next = strtok_r(NULL, "/", &save_ptr);
	}

	strlcpy(*last_name, cur, strlen(cur) + 1); // not sure if this works
	return parent;
	
}





/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  if (strlen(name)== 0) {return false;}

  char* last_name = malloc(strlen(name) + 1);

  struct dir* parent = traverse(name, &last_name); //dir parent should be second to last
  if (parent == NULL) {return false;}

  struct inode* childnode;


  if (dir_lookup(parent, last_name, &childnode)) { // last_name already exists
  	dir_close(parent);
  	inode_close(childnode);
  	return false; 
  	} 

  block_sector_t inode_sector = 0;
  bool success;

  if (is_dir) {
    lock_acquire(&free_map_lock);
    success = parent != NULL&& free_map_allocate (1, &inode_sector);
    lock_release(&free_map_lock);
    success = success && dir_create(inode_sector, 1)
                      && dir_add (parent, last_name, inode_sector);

    if (!success && inode_sector != 0) {
      lock_acquire(&free_map_lock);
      free_map_release (inode_sector, 1);
      lock_release(&free_map_lock);
    }

    childnode = inode_open(inode_sector); //fetch the newly made child

    struct dir* childdir = dir_open(childnode);

//  whien i add . and .., manually, because don't inode write at
    dir_add(childdir, ".", inode_sector);
    dir_add(childdir, "..", inode_get_inumber(dir_get_inode(parent)));
    dir_close(childdir); // we don't want to leave the inode open
  } else { // not a directory
      lock_acquire(&free_map_lock);
      success = parent != NULL &&free_map_allocate(1, &inode_sector);
      lock_release(&free_map_lock);
      success = success && inode_create (inode_sector, initial_size, false)
                        && dir_add (parent, last_name, inode_sector);

    if (!success && inode_sector != 0) {
      lock_acquire(&free_map_lock);
      free_map_release (inode_sector, 1);
      lock_release(&free_map_lock);
    }

  }
  dir_close(parent);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  int fd = open_helper(name);
  if (fd == -1) {return NULL;}


  return  get_file_for_fd(fd, &thread_current()->fds);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  if (strlen(name)== 0) {return false;}
  if (strcmp(name, "/") == 0) {return false;}


  char* last_name = malloc(strlen(name) + 1);

  struct dir* parent = traverse(name, &last_name); //dir parent should be second to last
  if (parent == NULL) {return false;}

  struct inode* childnode;

 // after loop, parent is set to dir at second last, cur set to last string



  if (dir_lookup(parent, last_name, &childnode) == false) {
  	dir_close(parent);
    return false;  // file to remove does not exist
  }

  if (inode_get_open_cnt(childnode) > 2)  {
    dir_close(parent);
    inode_close(childnode);
    return false;
  }


  bool success;
  char* entryname = malloc(NAME_MAX + 1);

  //if directory only remove if not cwd, and is not in use (empty)
  if (inode_get_isdir(childnode)) { 
    if (inode_get_inumber(childnode) == inode_get_inumber(dir_get_inode(thread_current()-> working_dir))) {
    	dir_close(parent);
    	//should I close the child?
      return false; // not allowed removing cwd
    }

    struct dir* childdir = dir_open(childnode);
    while(dir_readdir(childdir, entryname)) { // iterate over directory entries
      if (strcmp(".", entryname) != 0 && strcmp("..", entryname) != 0) {
      	dir_close(parent);
      	dir_close(childdir);
      	free(entryname);
        return false; // cannot remove directories with non-trivial entries
      }
    }
    dir_close(childdir);
    success = dir_remove(parent, last_name);
    dir_close(parent);

  } else {
  	inode_close(childnode);
    success = dir_remove(parent, last_name);
    dir_close(parent);
  }
  return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
