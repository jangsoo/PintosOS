#include "devices/block.h"
#include <list.h>
#include <string.h>
#include <stdio.h>
#include <threads/synch.h>
#include <filesys/filesys.h>
#include <threads/interrupt.h>
#include "devices/ide.h"
#include "threads/malloc.h"


#define NUM_BLOCKS_IN_CACHE 64


/* A single 512 byte block cached in memory. */
struct cached_block {
  block_sector_t sector;              //sector represented by this cached block
  struct block *block;                //block device represented
  uint8_t cache[BLOCK_SECTOR_SIZE];   //pointer to some location in memory for that block
  bool dirty;                         //whether block is dirty or not
  struct lock block_lock;             //protects buffer at loc_in_mem and dirty bit only. All else is protected by global lock.
  struct list_elem elem;              //for use in block_cache

};


/* A block device. */
struct block {
  struct list_elem list_elem;         /* Element in all_blocks. */

  char name[16];                      /* Block device name. */
  enum block_type type;                /* Type of block device. */
  block_sector_t size;                 /* Size in sectors. */

  const struct block_operations *ops;  /* Driver operations. */
  void *aux;                          /* Extra data owned by driver. */

  unsigned long long read_cnt;        /* Number of sectors read. */
  unsigned long long write_cnt;       /* Number of sectors written. */
};


struct lock global_cache_lock;
struct list block_cache;

void
block_cache_init(void) {
  list_init(&block_cache);
  lock_init(&global_cache_lock);
  lock_acquire(&global_cache_lock);
  int i = NUM_BLOCKS_IN_CACHE;
  while (i --> 0) {
    struct cached_block *new_block = malloc(sizeof(struct cached_block));
    new_block->sector = 0;
    new_block->block = NULL; //works also as a flag that states the block is not currently used when set to NULL
    new_block->dirty = false;
    lock_init(&new_block->block_lock);
    list_push_front(&block_cache, &new_block->elem);
  }
  lock_release(&global_cache_lock);
}

/* returns true if we hold block-lock */
static bool
write_if_dirty(struct cached_block *bl) {

  if (bl->block != NULL && bl->dirty == true) {
    /* NOTE: We do not give up global lock
     * This will still run since we only acquire the inside cache_lock when we no longer need global lock anymore
     */
    if (!lock_held_by_current_thread(&bl->block_lock)) {
      lock_acquire(&bl->block_lock);
    }
    bl->block->ops->write(bl->block->aux, bl->sector, bl->cache);
    bl->dirty = false;
  }
  return lock_held_by_current_thread(&bl->block_lock);
}

void
flush_block_cache(bool free_cached_blocks) {
  lock_acquire(&global_cache_lock);
  struct list_elem *cur_elem = list_begin(&block_cache);
  struct list_elem *end_elem = list_end(&block_cache);
  struct cached_block *cur_block;
  while (cur_elem != end_elem) {
    //loop through cache and write to disk
    cur_block = list_entry(cur_elem, struct cached_block, elem);

    bool holding_lock = write_if_dirty(cur_block);
    if (holding_lock) {
      lock_release(&cur_block->block_lock);
    }

    cur_elem = list_next(cur_elem);
    if (free_cached_blocks) {
      free(cur_block);
    }
  }

  lock_release(&global_cache_lock);
}

/* Must have block-lock and global-lock before call. Will have block-lock (not global lock) on return  */
void
evict_and_replace(struct cached_block *bl, struct block *new_block_device, block_sector_t new_sector) {
  ASSERT(lock_held_by_current_thread(&bl->block_lock));
  ASSERT(lock_held_by_current_thread(&global_cache_lock));
  write_if_dirty(bl); //always true since we have the block_lock

  bl->sector = new_sector;
  bl->block = new_block_device;
  bl->dirty = false; //not necessary since is already false

  //we already have lock
  bl->block->ops->read(bl->block->aux, new_sector, bl->cache);

  //LRU move to front
  list_remove(&bl->elem);
  list_push_front(&block_cache, &bl->elem);

  lock_release(&global_cache_lock);

  ASSERT(lock_held_by_current_thread(&bl->block_lock));
  ASSERT(!lock_held_by_current_thread(&global_cache_lock));

}
/* needs to have global lock before call, will have block-lock on return */
struct cached_block *
get_cached_block(struct block *block_device, block_sector_t sector) {
  ASSERT(lock_held_by_current_thread(&global_cache_lock)); //Caller should have secured this for us already
  struct list_elem *cur_elem;
  struct list_elem *end_elem = list_end(&block_cache);
  struct cached_block *cur_block = NULL;
  for (cur_elem = list_begin(&block_cache); cur_elem != end_elem; cur_elem = list_next(cur_elem)) {
    cur_block = list_entry(cur_elem, struct cached_block, elem);
    if (cur_block -> block == block_device && cur_block->sector == sector) {
      //is in cache!

      //LRU move to front
      list_remove(&cur_block->elem);
      list_push_front(&block_cache, &cur_block->elem);

      enum intr_level old_level = intr_disable();
      lock_release(&global_cache_lock);
      lock_acquire(&cur_block->block_lock);
      intr_set_level(old_level);

      break;
    }
  }
  if (cur_block -> block != block_device || cur_block->sector != sector) { //no worries about NULL pointer since that would mean cache did not exist
    //EVICTION PROCESS
    bool evicted = false;
    //why tf is list_front and list_begin the exact same thing. whatever
    for (cur_elem = list_back(&block_cache); cur_elem != list_head(&block_cache); cur_elem = list_prev(cur_elem)) {
      //reverse look for unpinned blocks
      cur_block = list_entry(cur_elem, struct cached_block, elem);
      bool unpinned = lock_try_acquire(&cur_block->block_lock);
      if (unpinned) { //WE EVICT THIS BLOCK!
        evict_and_replace(cur_block, block_device, sector);
        evicted = true;
        break;
      }
    }
    if (!evicted) {
      cur_block = list_entry(list_back(&block_cache), struct cached_block, elem);
      lock_acquire(&cur_block->block_lock);
      evict_and_replace(cur_block, block_device, sector);
    }
  }
  //ONCE HERE, OUR SECTOR IS CUR_BLOCK SECTOR, REGARDLESS OF WHICH OF 3 WAYS WE GOT HERE
  ASSERT(cur_block->sector == sector);
  ASSERT(lock_held_by_current_thread(&cur_block->block_lock));
  ASSERT(!lock_held_by_current_thread(&global_cache_lock));
  return cur_block;

}


/* List of all block devices. */
static struct list all_blocks = LIST_INITIALIZER (all_blocks);

/* The block block assigned to each Pintos role. */
static struct block *block_by_role[BLOCK_ROLE_CNT];

static struct block *list_elem_to_block(struct list_elem *);

/* Returns a human-readable name for the given block device
   TYPE. */
const char *
block_type_name(enum block_type type) {
  static const char *block_type_names[BLOCK_CNT] =
      {
          "kernel",
          "filesys",
          "scratch",
          "swap",
          "raw",
          "foreign",
      };

  ASSERT (type < BLOCK_CNT);
  return block_type_names[type];
}

/* Returns the block device fulfilling the given ROLE, or a null
   pointer if no block device has been assigned that role. */
struct block *
block_get_role(enum block_type role) {
  ASSERT (role < BLOCK_ROLE_CNT);
  return block_by_role[role];
}

/* Assigns BLOCK the given ROLE. */
void
block_set_role(enum block_type role, struct block *block) {
  ASSERT (role < BLOCK_ROLE_CNT);
  block_by_role[role] = block;
}

/* Returns the first block device in kernel probe order, or a
   null pointer if no block devices are registered. */
struct block *
block_first(void) {
  return list_elem_to_block(list_begin(&all_blocks));
}

/* Returns the block device following BLOCK in kernel probe
   order, or a null pointer if BLOCK is the last block device. */
struct block *
block_next(struct block *block) {
  return list_elem_to_block(list_next(&block->list_elem));
}

/* Returns the block device with the given NAME, or a null
   pointer if no block device has that name. */
struct block *
block_get_by_name(const char *name) {
  struct list_elem *e;

  for (e = list_begin(&all_blocks); e != list_end(&all_blocks);
       e = list_next(e)) {
    struct block *block = list_entry (e, struct block, list_elem);
    if (!strcmp(name, block->name))
      return block;
  }

  return NULL;
}

/* Verifies that SECTOR is a valid offset within BLOCK.
   Panics if not. */
static void
check_sector(struct block *block, block_sector_t sector) {
  if (sector >= block->size) {
    /* We do not use ASSERT because we want to panic here
       regardless of whether NDEBUG is defined. */
    PANIC ("Access past end of device %s (sector=%"
               PRDSNu
               ", "
               "size=%"
               PRDSNu
               ")\n", block_name(block), sector, block->size);
  }
}


/* Reads sector SECTOR from BLOCK into BUFFER, which must
   have room for BLOCK_SECTOR_SIZE bytes.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_read (struct block *block, block_sector_t sector, void *buffer)
{
  check_sector (block, sector);
  block->ops->read (block->aux, sector, buffer);
  block->read_cnt++;
}

/* Write sector SECTOR to BLOCK from BUFFER, which must contain
   BLOCK_SECTOR_SIZE bytes.  Returns after the block device has
   acknowledged receiving the data.
   Internally synchronizes accesses to block devices, so external
   per-block device locking is unneeded. */
void
block_write (struct block *block, block_sector_t sector, const void *buffer)
{
  check_sector (block, sector);
  ASSERT (block->type != BLOCK_FOREIGN);
  block->ops->write (block->aux, sector, buffer);
  block->write_cnt++;
}


void block_cache_read(struct block *block, block_sector_t sector, void *buffer) {
  check_sector(block, sector);
  lock_acquire(&global_cache_lock);
  struct cached_block *cb = get_cached_block(block, sector);
  memcpy(buffer, cb->cache, BLOCK_SECTOR_SIZE);
  lock_release(&cb->block_lock);
  block->read_cnt++;

  ASSERT(!lock_held_by_current_thread(&global_cache_lock))
}

void
block_cache_write(struct block *block, block_sector_t sector, const void *buffer) {

  check_sector(block, sector);
  lock_acquire(&global_cache_lock);
  struct cached_block *cb = get_cached_block(block, sector);
  memcpy(cb->cache, buffer, BLOCK_SECTOR_SIZE);
  cb->dirty = true;
  lock_release(&cb->block_lock);
  block->read_cnt++;

  ASSERT(!lock_held_by_current_thread(&global_cache_lock))
}


void
block_cache_read_offset(struct block *block, block_sector_t sector, void *buffer, off_t offset, size_t num_bytes) {

  ASSERT(offset >= 0)
  ASSERT(offset + num_bytes <= BLOCK_SECTOR_SIZE)

  check_sector(block, sector);
  lock_acquire(&global_cache_lock);
  struct cached_block *cb = get_cached_block(block, sector);
  memcpy(buffer, cb->cache + offset, num_bytes);
  lock_release(&cb->block_lock);
  block->read_cnt++;

  ASSERT(!lock_held_by_current_thread(&global_cache_lock))
}


void
block_cache_write_offset(struct block *block, block_sector_t sector, const void *buffer, off_t offset,
                         size_t num_bytes) {

  ASSERT(offset >= 0)
  ASSERT(offset + num_bytes <= BLOCK_SECTOR_SIZE)

  check_sector(block, sector);
  lock_acquire(&global_cache_lock);
  struct cached_block *cb = get_cached_block(block, sector);
  memcpy(cb->cache + offset, buffer, num_bytes);
  cb->dirty = true;
  lock_release(&cb->block_lock);
  block->read_cnt++;

  ASSERT(!lock_held_by_current_thread(&global_cache_lock))
}

/* Returns the number of sectors in BLOCK. */
block_sector_t
block_size(struct block *block) {
  return block->size;
}

/* Returns BLOCK's name (e.g. "hda"). */
const char *
block_name(struct block *block) {
  return block->name;
}

/* Returns BLOCK's type. */
enum block_type
block_type(struct block *block) {
  return block->type;
}

/* Prints statistics for each block device used for a Pintos role. */
void
block_print_stats(void) {
  int i;

  for (i = 0; i < BLOCK_ROLE_CNT; i++) {
    struct block *block = block_by_role[i];
    if (block != NULL) {
      printf("%s (%s): %llu reads, %llu writes\n",
             block->name, block_type_name(block->type),
             block->read_cnt, block->write_cnt);
    }
  }
}

/* Registers a new block device with the given NAME.  If
   EXTRA_INFO is non-null, it is printed as part of a user
   message.  The block device's SIZE in sectors and its TYPE must
   be provided, as well as the it operation functions OPS, which
   will be passed AUX in each function call. */
struct block *
block_register(const char *name, enum block_type type,
               const char *extra_info, block_sector_t size,
               const struct block_operations *ops, void *aux) {
  struct block *block = malloc(sizeof *block);
  if (block == NULL)
    PANIC ("Failed to allocate memory for block device descriptor");

  list_push_back(&all_blocks, &block->list_elem);
  strlcpy(block->name, name, sizeof block->name);
  block->type = type;
  block->size = size;
  block->ops = ops;
  block->aux = aux;
  block->read_cnt = 0;
  block->write_cnt = 0;

  printf("%s: %'"PRDSNu" sectors (", block->name, block->size);
  print_human_readable_size((uint64_t) block->size * BLOCK_SECTOR_SIZE);
  printf(")");
  if (extra_info != NULL)
    printf(", %s", extra_info);
  printf("\n");

  return block;
}

/* Returns the block device corresponding to LIST_ELEM, or a null
   pointer if LIST_ELEM is the list end of all_blocks. */
static struct block *
list_elem_to_block(struct list_elem *list_elem) {
  return (list_elem != list_end(&all_blocks)
          ? list_entry (list_elem, struct block, list_elem)
          : NULL);
}

