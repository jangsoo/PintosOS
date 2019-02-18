#ifndef FILESYS_FREE_MAP_H
#define FILESYS_FREE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

void free_map_init (void);
void free_map_read (void);
void free_map_create (void);
void free_map_open (void);
void free_map_close (void);

uint32_t free_map_num_free(void);

bool free_map_allocate (size_t, block_sector_t *);
void free_map_release (block_sector_t, size_t);

struct lock  free_map_lock;   /* Free map lock */
uint32_t num_free;            /* Free map number free */

#endif /* filesys/free-map.h */
