#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DIRECT_BLOCKS 124
#define NUM_BLOCKS_IN_IND 128


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;
    unsigned magic;
    uint32_t is_dir; //0 is not directory, anything else is directory
    block_sector_t direct_ptrs[NUM_DIRECT_BLOCKS];
    block_sector_t doubly_indirect_ptr;

  };

struct indirect_disk_block {
  block_sector_t blocks[128];
};


bool inode_block_allocate(struct inode *inode, off_t end_pos);
bool inode_block_alloc_helper(struct inode_disk *id, off_t end_len);
uint32_t inode_is_dir (const struct inode *inode);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock inode_lock;
    int isdir;
  };

  int inode_get_isdir(const struct inode *inode) {
    return inode -> isdir;
  }

  int inode_get_open_cnt(const struct inode *inode) {
    return inode-> open_cnt;
  }


static struct inode_disk*
inode_get_data(const struct inode *inode) {
  struct inode_disk *id = malloc(sizeof(struct inode_disk));
  block_cache_read (fs_device, inode->sector, id);
  return id;
}

static block_sector_t
block_num_to_sector(struct inode_disk *id, int block_num) {
  uint32_t answer;
  struct indirect_disk_block data;

  if (block_num < NUM_DIRECT_BLOCKS) {
    answer = id->direct_ptrs[block_num];
  } else {
    if (id->doubly_indirect_ptr == NULL) {
      return (block_sector_t) -1; //no doubly indirect
    }
    // read in doubly indirect block pointers
    block_cache_read(fs_device, id->doubly_indirect_ptr, &data);

    int indirect_block_num = (block_num - NUM_DIRECT_BLOCKS) / NUM_BLOCKS_IN_IND;
    int indirect_block_ind = (block_num - NUM_DIRECT_BLOCKS) % NUM_BLOCKS_IN_IND;


    if (data.blocks[indirect_block_num] == NULL) {
      return (block_sector_t) -1; //no indirect inside of doubly indirect
    }

    // read in correct singly indirect block
    block_cache_read(fs_device, data.blocks[indirect_block_num], &data);

    // get index of sector to read
    answer = data.blocks[indirect_block_ind];

  }

  return answer;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{

  int block_number = pos / BLOCK_SECTOR_SIZE; //TODO: CHECK FOR CORRECTNESS OF THIS FORMULA
  struct inode_disk *id = inode_get_data(inode);

  block_sector_t answer = block_num_to_sector(id, block_number);
  free(id);
  return answer;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = (uint32_t) is_dir;

      if (inode_block_alloc_helper(disk_inode, length)) {

        block_cache_write (fs_device, sector, disk_inode);
        if (sectors > 0)
        {
          static char zeros[BLOCK_SECTOR_SIZE];
          size_t i;

          for (i = 0; i < sectors; i += 1)
            block_cache_write (fs_device, block_num_to_sector(disk_inode, i) , zeros);
        }
        success = true;
      }

      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  inode->isdir = inode_is_dir(inode);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {

          struct inode_disk *data = inode_get_data(inode);
          lock_acquire(&free_map_lock);

          free_map_release (inode->sector, 1);

          size_t sectors = bytes_to_sectors (data->length);
          size_t curr_block = 0;
          struct indirect_disk_block doubly_indirect_block;
          bool doubly_indirect_obtained = false;

          struct indirect_disk_block indirect_block;
          int number_for_current_indirect_block = -1;

          while (curr_block < sectors) {
            int indirect_offset = (curr_block - NUM_DIRECT_BLOCKS) % NUM_BLOCKS_IN_IND;
            int indirect_number = (curr_block - NUM_DIRECT_BLOCKS) / NUM_BLOCKS_IN_IND;


            //dealloc direct blocks
            if (curr_block < NUM_DIRECT_BLOCKS) {
              free_map_release(data->direct_ptrs[curr_block], 1);
              curr_block ++;
              continue; //used to prevent creation of doubly indirect on this go
            }

            //fetch doubly indirect
            if (!doubly_indirect_obtained) {
              block_cache_read(fs_device, data->doubly_indirect_ptr, &doubly_indirect_block);
              doubly_indirect_obtained = true;
            }

            // free indirect block behind us
            if (indirect_offset == 0 && number_for_current_indirect_block != -1) {
              // allocate indirect block
              free_map_release(doubly_indirect_block.blocks[number_for_current_indirect_block], 1);
            }
            // fetch current indirect block
            if (number_for_current_indirect_block != indirect_number) {
              block_cache_read(fs_device, doubly_indirect_block.blocks[indirect_number], &indirect_block);
              number_for_current_indirect_block = indirect_number;
            }
            // free data block
            free_map_release(indirect_block.blocks[indirect_offset], 1);

          }
          //free currently held indirect block
          if (number_for_current_indirect_block != -1) {
            free_map_release(doubly_indirect_block.blocks[number_for_current_indirect_block], 1);
          }

          if (doubly_indirect_obtained) {
            free_map_release(data->doubly_indirect_ptr, 1);

          }

          lock_release(&free_map_lock);
          free(data);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;


    block_cache_read_offset(fs_device, sector_idx, buffer + bytes_read, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
    */
off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
               off_t offset) {

  if (size < 0)
    return 0;

  if (offset < 0)
    return 0;

  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  if (inode_length(inode) < offset + size) {
    //need to allocate more
    if (!inode_block_allocate(inode, offset + size)) {
      return 0; //failed to allocate
    }
  }

  while (size > 0) {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    block_cache_write_offset(fs_device, sector_idx, buffer + bytes_written, sector_ofs, chunk_size);

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;


  }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk *data = inode_get_data(inode);
  off_t answer = data->length;
  free(data);
  return answer;
}

/* Returns whether the underlying disk of inode is a dir */
uint32_t
inode_is_dir (const struct inode *inode) {
  struct inode_disk *data = inode_get_data(inode);
  uint32_t answer = data->is_dir;
  free(data);
  return answer;
}

/*- takes in some byte number beyond end of file
- checks to see if there is room in `free-map`
- allocates blocks up to that point, adjusting `inode`'s pointers

return true if successful else false*/

bool
inode_block_allocate(struct inode *inode, off_t end_pos)
{
  struct inode_disk *id = inode_get_data(inode);
  bool success = inode_block_alloc_helper(id, end_pos);
  block_cache_write(fs_device, inode->sector, id); //write new inode to disk with new length and possibly new doubly ind
  free(id);
  return success;
}

bool
inode_block_alloc_helper(struct inode_disk *id, off_t end_len)
{

  off_t len = id->length;

  off_t free_bytes = BLOCK_SECTOR_SIZE - (len % BLOCK_SECTOR_SIZE);
  free_bytes = free_bytes % BLOCK_SECTOR_SIZE == 0 ? 0: free_bytes;

  int new_length_needed = end_len - len; //wanted len - current len

  if (new_length_needed <= free_bytes) {
    id->length = end_len;
    return true; // dont allocate, there is enough space on last block
  }
  off_t bytes_to_allocate = new_length_needed - free_bytes;
  int blocks_to_allocate = bytes_to_sectors(bytes_to_allocate);


  int last_block = len == 0 ? -1 : (len - 1) / BLOCK_SECTOR_SIZE; //set to -1 so that curr_block is 0 on new file
  int curr_block = last_block + 1;
  int end_block = last_block + blocks_to_allocate;


  //TODO CHECK IF OVERHEAD CALC CORRECT
  int allocation_including_overhead = blocks_to_allocate;
  allocation_including_overhead += end_block >= NUM_DIRECT_BLOCKS && last_block < NUM_DIRECT_BLOCKS ? 2 : 0;
  //above is double indirect and first indirect block
  if (end_block >= NUM_DIRECT_BLOCKS) {
    allocation_including_overhead += (end_block - NUM_DIRECT_BLOCKS) / NUM_BLOCKS_IN_IND; //all the rest of indirect
  }

  lock_acquire(&free_map_lock);
  //lock on free map implemented inside of free-map functions
  if (free_map_num_free() < allocation_including_overhead) {
    lock_release(&free_map_lock);
    return false; //not enough space!!
  }

  //allocate double indirect block if necessary
  // allocate all indirect blocks if necessary
  // allocate all data blocks



  struct indirect_disk_block doubly_indirect_block;
  bool doubly_indirect_obtained = false;

  struct indirect_disk_block indirect_block;
  int number_for_current_indirect_block = -1;

  while (curr_block <= end_block) {
    int indirect_offset = (curr_block - NUM_DIRECT_BLOCKS) % NUM_BLOCKS_IN_IND;
    int indirect_number = (curr_block - NUM_DIRECT_BLOCKS) / NUM_BLOCKS_IN_IND;


    //allocate direct blocks if necessary
    if (curr_block < NUM_DIRECT_BLOCKS) {
      free_map_allocate(1, &id->direct_ptrs[curr_block]);
      curr_block ++;
      continue; //used to prevent creation of doubly indirect on this go
    }
    //allocate double indirect block if necessary
    if (curr_block == NUM_DIRECT_BLOCKS) {
      free_map_allocate(1, &id->doubly_indirect_ptr);
    }
    //fetch block
    if (!doubly_indirect_obtained) {
      block_cache_read(fs_device, id->doubly_indirect_ptr, &doubly_indirect_block);
      doubly_indirect_obtained = true;
    }

    // allocate indirect block if necessary
    if (indirect_offset == 0) {
      // allocate indirect block
      free_map_allocate(1, &doubly_indirect_block.blocks[indirect_number]);
    }
    //fetch indirect block
    if (number_for_current_indirect_block != indirect_number) {
      if (number_for_current_indirect_block != -1) {
        block_cache_write(fs_device, doubly_indirect_block.blocks[number_for_current_indirect_block], &indirect_block);
      }
      block_cache_read(fs_device, doubly_indirect_block.blocks[indirect_number], &indirect_block);
      number_for_current_indirect_block = indirect_number;
    }
    // allocate data block
    free_map_allocate(1, &indirect_block.blocks[indirect_offset]);
    curr_block ++;
  }

  if (number_for_current_indirect_block != -1) {
    block_cache_write(fs_device, doubly_indirect_block.blocks[number_for_current_indirect_block], &indirect_block);
    //update last indirect cache block which may not have been filled
  }
  if (doubly_indirect_obtained) {
    block_cache_write(fs_device, id->doubly_indirect_ptr, &doubly_indirect_block);
    //update doubly indirect cache block if it was necessary for allocation
  }

  id->length = end_len;
  lock_release(&free_map_lock);
  return true;
}

