// Physical memory allocator for user processes, kernel stacks, page-table pages and pipe buffers. 
// Allocates 2^n 4k-bytes pages.
// Manages half of all memory ranging from HALF_PHYSTOP to PHYSTOP (the first half is still managed by kalloc.c)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// ------------- metadata structures -------------

#define TOTAL_PAGES (1 << 15)  // 2^15
#define TOTAL_LEVELS 16 // 0 - 15

#define GOOD_LEVELS 10    // 0 - 9 (6 - 15 of total)
#define MAX_ALLOC (1 << (GOOD_LEVELS - 1))    // 512

#define BLOCK_SIZE_IN_LEVEL(x) (1 << (TOTAL_LEVELS - 1 - x))  // how many pages are in one block on level x
#define BLOCKS_IN_LEVEL(x) (1 << x)  // how many blocks are on level x 

struct meta_block { 
  int free;    // 1 - free, 0 - taken
};

struct {
  struct spinlock lock;
  struct meta_block meta[2 * TOTAL_PAGES]; // = 1 << (TOTAL_LEVELS + 1) = 2^15
  // meta is a binary tree representing free blocks of memory
  // k-th level is meta[2^k : 2^(k + 1) - 1], its node has info about (TOTAL_PAGES / 2^k) pages block 
  // meta[0] is not used, one-page nodes are on the last level
  // meta[k] has two kids: meta[2k] and meta[2k + 1],
  // each one level lower than the parent
} buddy_mem;

// ------------------------------------------------


// ------------- auxiliary functions -------------

struct mem_info {
  uint64 total;                            // total memory in pages
  uint64 free;                             // total free memory in pages
  uint64 free_by_size_full[TOTAL_LEVELS];
};

struct mem_info
get_mem_info(void) {

  struct mem_info res;

  acquire(&buddy_mem.lock);

  res.total = TOTAL_PAGES;

  res.free = 0;
  for (int i = 0; i < TOTAL_LEVELS; ++i) {
    res.free_by_size_full[i] = 0;
  }

  for (int i = 0; i < TOTAL_LEVELS; ++i) {
    for (uint64 j = 1 << i; j < (1 << (i + 1)); ++j) {
      if (buddy_mem.meta[j].free == 1) {
        res.free += (TOTAL_PAGES / (1 << i));
        res.free_by_size_full[i] += 1;
      }
    }
  }

  release(&buddy_mem.lock);

  return res;
}

void
print_mem_info(struct mem_info m) {
  printf(" ---- mem_info ----\n");
  printf("total pages: %d\n", m.total);
  printf("free pages: %d\n", m.free);
  printf("free_by_size_full:\n");
  for (int i = 0; i < TOTAL_LEVELS; ++i) {
    printf("%d ", m.free_by_size_full[i]);
  }
  printf("\n");
  /*
  printf("free_by_size (last ten levels):\n");
  for (int i = TOTAL_LEVELS - GOOD_LEVELS; i < TOTAL_LEVELS; ++i) {
    printf("%d ", m.free_by_size_full[i]);
  }
  printf("\n");
  */
  printf("------------\n");
}

// ------------------------------------------------



// ------------- memory initialization -------------

// initializes the lock and frees all memory
// called in main() right after kinit()
void
buddy_init()
{
  initlock(&buddy_mem.lock, "buddy_mem");
  acquire(&buddy_mem.lock);
  for (int i = 0; i < TOTAL_LEVELS; ++i) {
    for (uint64 j = 1 << i; j < (1 << (i + 1)); ++j) {
      buddy_mem.meta[j].free = 0;
    }
  }
  buddy_mem.meta[1].free = 1;
  release(&buddy_mem.lock);

  printf("==== buddy init ====\n");
  print_mem_info(get_mem_info());
  printf("\n");
}

// ------------------------------------------------


// ------------- buddy memory freeing -------------


// merge brothers into parent to avoid fragmentation
// should be called with buddy_mem.lock held
void
merge_brothers(int level, uint64 block_in_level) {

  if (level == 0) return;

  uint64 brother_in_level = block_in_level + 1;
  if (block_in_level % 2 == 1) {
    brother_in_level = block_in_level - 1;
  }

  uint64 block = (1 << level) + block_in_level;
  uint64 brother = (1 << level) + brother_in_level;
  uint64 parent = block >> 1;

  if (buddy_mem.meta[block].free == 1 && buddy_mem.meta[brother].free == 1) {
    buddy_mem.meta[block].free = 0;
    buddy_mem.meta[brother].free = 0;
    buddy_mem.meta[parent].free = 1;
    merge_brothers(level - 1, parent);
  }

}


// free the block of pages in physical memory pointed at by pa
void
buddy_free(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < PGROUNDUP((uint64)end) || (uint64)pa >= PHYSTOP)
    panic("buddy_free");

  printf("==== buddy free ====\n");

  acquire(&buddy_mem.lock);

  int found = 0;

  uint64 page_num = (uint64) pa - PGROUNDUP((uint64)end);
  for (int level = 0; level < TOTAL_LEVELS; ++level) {
    if (page_num % BLOCKS_IN_LEVEL(level) != 0) {
      continue;
    } 
    else {
      uint64 block_in_level = page_num / BLOCKS_IN_LEVEL(level);
      uint64 block = (1 << level) + block_in_level;
      if (buddy_mem.meta[block].free == 0) {
        buddy_mem.meta[block].free = 1;
        found = 1;
        merge_brothers(level, block_in_level);
        break;
      }
    }
  }

  if (found == 0) {
    panic("buddy_free");
  }

  release(&buddy_mem.lock);

  print_mem_info(get_mem_info());
  printf("\n");
  return;
}
// ------------------------------------------------


// ------------- buddy memory allocation -------------

// checks whether pgcnt is 2^n, where 1 <= 2^n <= MAX_ALLOC
// returns n or -1
int
process_pgcnt(uint64 pgcnt) {
  int n = 0;
  for (uint64 pow_2 = 1; pow_2 <= MAX_ALLOC; pow_2 <<= 1) {
    if (pgcnt == pow_2) {
      return n;
    }
    ++n;
  }
  return -1;
}

// allocate pgcnt pages of physical memory
// pgcnt should be one of 1, 2, ..., 512
// returns a pointer that the kernel can use 
// or 0 if the memory cannot be allocated or pgcnt is not a power of 2
void*
buddy_alloc(uint64 pgcnt)
{

  printf("==== buddy alloc ====\n");

  int pow_2 = process_pgcnt(pgcnt);
  if (pow_2 == -1) return 0;
  int needed_level = TOTAL_LEVELS - 1 - pow_2;
  
  printf("called for %d pages\n", pgcnt);
  printf("needed_level = %d\n", needed_level);

  acquire(&buddy_mem.lock);

  // searching for empty block on levels from n to 0

  int found_level = -1;
  uint64 block = 0;
  uint64 block_in_level = 0;

  for (int k = needed_level; k >= 0; --k) {
    // printf("~~~~ searching on level %d\n", k);
    uint64 j0 = 0;
    for (uint64 j = (1 << k); j < (1 << (k + 1)); ++j) {
      // printf("~~ searching at block_in_level %d\n", j0);
      if (buddy_mem.meta[j].free == 1) {
        // found free block on k-th level
        found_level = k;
        block = j;
        block_in_level = j0;
      }
      if (found_level > -1) break;
      j0++;
    }
    if (found_level > -1) break;
  }

  // didn't find enough memory
  if (found_level == -1) {
    release(&buddy_mem.lock);
    return 0;
  }

  printf("found_level = %d, block = %d, block_in_level = %d\n", found_level, block, block_in_level);

  // split nodes on the way to the needed level
  // [... 1, 0, 0, 0, ...] --split--> [... 0, 1, 1, 2, ...] --give--> [... 0, 1, 1, 1, ...]
  /*
              1
          0       0         
        0   0   0   0

              |
              
              0
          0       1         
        #   1   0   0
  */

  buddy_mem.meta[block].free = 0;
  for (int k = found_level + 1; k <= needed_level; ++k) {
    block <<= 1;
    buddy_mem.meta[block].free = 0;
    buddy_mem.meta[block + 1].free = 1;
  }

  /*
  buddy_mem.meta[block].free = 0;
  int diff_levels = needed_level - found_level;
  block <<= diff_levels;

  printf("block on needed level = %d\n", block);

  for (uint64 j = block; j < block + (1 << diff_levels); ++j) {
    buddy_mem.meta[j].free = 1;
  }
  buddy_mem.meta[block].free = 0;
  */

  uint64 res = PGROUNDUP((uint64)end) + block_in_level * BLOCK_SIZE_IN_LEVEL(found_level);
  printf("res address % 2^8 = %d\n", res % (1 << 8));

  release(&buddy_mem.lock);

  print_mem_info(get_mem_info());
  printf("\n");

  return (void*) res;
}
// ------------------------------------------------