// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "budMemAllocStructs.h"


extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct MemNode nodes_mem[CNT_MEM_BLOCKS * (1 << STANDART_DEPTH)];
struct MemNode* free_root = nodes_mem;
struct BlockMem blocks[CNT_MEM_BLOCKS];
uint64 mem_separator;


uint8 check_node_block_counting(struct MemNode *node_ptr) {
    if (node_ptr->maxFreePage == node_ptr->cnt_page) {
        if (node_ptr->depth == 0) {
            return 1;
        }
        else {
            // if (free_debug)
            //     printf("For head: %d, %d\n", node_ptr->head->maxFreePage, node_ptr->head->cnt_page);

            if (node_ptr->head->maxFreePage != node_ptr->head->cnt_page) {
                return 1;
            }
        }
    }
    return 0;
}

// create tree for this block
void
init_block(struct BlockMem* block_ptr) {

    initlock(&block_ptr->lock, "mem_block");
    ++free_root;
    block_ptr->root = free_root;

    free_root->leftChild = free_root + 1; 
    free_root->rightChild = free_root + 2;
    free_root->head = 0;
    free_root->depth = 0;
    free_root->start_ptr = PGSIZE * (free_root - nodes_mem - 1) / 2 + mem_separator;
    free_root->cnt_page = 1 << (STANDART_DEPTH - 1);
    free_root->maxFreePage = free_root->cnt_page;
    free_root->is_used = 0;

    --free_root;
    uint16 depth = 1;
    for (int i = 2; i < 1 << STANDART_DEPTH; ++i) {    
        struct MemNode *local_node = free_root + i;
        if (i == (1 << (depth + 1))) {
            ++depth;
        }
        if (depth < STANDART_DEPTH - 1) {
            local_node->leftChild = free_root + 2 * i;
            local_node->rightChild = free_root + 2 * i + 1;
        }
        else {
            local_node->rightChild = 0;
            local_node->leftChild = 0;
        }
        
        local_node->head = free_root + i / 2;
        local_node->depth = depth;
        local_node->cnt_page = local_node->head->cnt_page / 2;
        local_node->maxFreePage = local_node->cnt_page;
        local_node->start_ptr = local_node->head->start_ptr + (i % 2 == 0 ? 0 : local_node->cnt_page) * PGSIZE;
        local_node->is_used = 0;
    }

    block_ptr->info.free = 1 << (STANDART_DEPTH - 1);
    block_ptr->info.total = 1 << (STANDART_DEPTH - 1);
    for (int i = 0; i < STANDART_DEPTH; ++i) {
        block_ptr->info.free_by_size[i] = (i == 0);
    }
    free_root += (1 << (STANDART_DEPTH));
}

void
update_node_free_cnt(struct MemNode *node_ptr, struct BlockMem *block_ptr) {
    if (node_ptr == 0) {
        return;
    }

    if (node_ptr->head != 0) {
        block_ptr->info.free_by_size[node_ptr->depth] -= check_node_block_counting(node_ptr->head->leftChild);
        block_ptr->info.free_by_size[node_ptr->depth] -= check_node_block_counting(node_ptr->head->rightChild);
    }
    else 
        block_ptr->info.free_by_size[node_ptr->depth] -= check_node_block_counting(node_ptr);
    if (node_ptr->depth != STANDART_DEPTH - 1) {
        uint16 left_cnt, right_cnt;

        left_cnt = node_ptr->leftChild->maxFreePage;
        right_cnt = node_ptr->rightChild->maxFreePage ;
        if (left_cnt + right_cnt == node_ptr->cnt_page) {
            node_ptr->maxFreePage = node_ptr->cnt_page;
        }
        else {
            node_ptr->maxFreePage = left_cnt < right_cnt ? right_cnt : left_cnt;
        }
    }
    

    if (node_ptr->head != 0) {
        update_node_free_cnt(node_ptr->head, block_ptr);
        block_ptr->info.free_by_size[node_ptr->depth] += check_node_block_counting(node_ptr->head->leftChild);
        block_ptr->info.free_by_size[node_ptr->depth] += check_node_block_counting(node_ptr->head->rightChild);
    }
    else 
        block_ptr->info.free_by_size[node_ptr->depth] += check_node_block_counting(node_ptr);
    
}

uint64
alloc_in_node(struct MemNode *node_ptr, uint16 size, struct BlockMem *block_ptr) {

    if (node_ptr->maxFreePage == size && node_ptr->cnt_page == size) {
        if (check_node_block_counting(node_ptr)) 
            --block_ptr->info.free_by_size[node_ptr->depth];
        else
            ++block_ptr->info.free_by_size[node_ptr->depth];


        block_ptr->info.free -= node_ptr->maxFreePage;
        node_ptr->maxFreePage = 0;
        node_ptr->is_used = 1;

        update_node_free_cnt(node_ptr->head, block_ptr);
        return node_ptr->start_ptr;
    }
    if (node_ptr->leftChild->maxFreePage >= size) {
        return alloc_in_node(node_ptr->leftChild, size, block_ptr);
    }
    else {
        return alloc_in_node(node_ptr->rightChild, size, block_ptr);
    }
}

uint64
alloc_in_block(struct BlockMem *block_ptr, uint16 size) {


    acquire(&block_ptr->lock);
    if (block_ptr->root->maxFreePage < size) {
        release(&block_ptr->lock);
        return 0;
    }
    uint64 pa = alloc_in_node(block_ptr->root, size, block_ptr);
    release(&block_ptr->lock);
    return pa;
}

void
free_in_node(struct MemNode *node_ptr, uint64 start_ptr, struct BlockMem *block_ptr) {
    if (node_ptr->start_ptr == start_ptr && node_ptr->is_used) {

        node_ptr->is_used = 0;
        node_ptr->maxFreePage = node_ptr->cnt_page;
        block_ptr->info.free += node_ptr->maxFreePage;
        update_node_free_cnt(node_ptr->head, block_ptr);
        if (check_node_block_counting(node_ptr)) 
            ++block_ptr->info.free_by_size[node_ptr->depth];
        else
            --block_ptr->info.free_by_size[node_ptr->depth];
    }
    if (node_ptr->leftChild == 0 || node_ptr->rightChild == 0) 
        return;
    if (node_ptr->start_ptr + node_ptr->cnt_page * PGSIZE / 2 <= start_ptr) {
        free_in_node(node_ptr->rightChild, start_ptr, block_ptr);
    }
    else {
        free_in_node(node_ptr->leftChild, start_ptr, block_ptr);
    }
}

void 
free_in_block(struct BlockMem *block_ptr, uint64 start_ptr) {
    acquire(&block_ptr->lock);
    free_in_node(block_ptr->root, start_ptr, block_ptr);
    release(&block_ptr->lock);
}

uint8 
checker_cnt_page(uint32 cnt) {
    if (2 * cnt > (1 << STANDART_DEPTH))
        return 0;
    while (cnt % 2 == 0) 
        cnt /= 2;
    return cnt == 1;
}

uint64 
check_addr_in_block(uint64 pa, struct BlockMem *blcok_ptr) {
    return (blcok_ptr->root->start_ptr <= pa && blcok_ptr->root->start_ptr + blcok_ptr->root->cnt_page  * PGSIZE > pa);
}

struct BlockMem used_blocks[CNT_MEM_BLOCKS];

void
buddy_init()
{  
  for (int i = 0; i < CNT_MEM_BLOCKS; ++i) {
    init_block(blocks + i);
    if ((blocks + i)->root->start_ptr + (blocks + i)->root->cnt_page * PGSIZE > PHYSTOP + 1) {
        panic("buddy_alloc_use_kalloc_mem");
    }
  }

}

void
buddy_free(void *pa)
{
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("buddy_free_pa");

    uint64 ptr_pa = (uint64)pa;
    for (int i = 0; i < CNT_MEM_BLOCKS; ++i) {
        if (check_addr_in_block(ptr_pa, blocks + i)) {
            free_in_block(blocks + i, ptr_pa);
        }
    }
}

void *
buddy_alloc(uint32 cnt)
{
    if (! checker_cnt_page(cnt))
        return 0;

    uint64 ptr;
    for (int i = 0; i < CNT_MEM_BLOCKS; ++i) {
        if ((ptr = alloc_in_block(blocks + i, cnt))) {
            return (void*)ptr;
        }
    }
    return 0;
}

void
mem_dump() {

    // for case, when we change our blocks (maybe we want change depth of one tree)
    
    struct BlocksInfo dump;
    dump.total = 0;
    dump.free = 0;
    for (int i = 0; i < STANDART_DEPTH; ++i) {
        dump.free_by_size[i] = 0;
    }
    for (int i = 0; i < CNT_MEM_BLOCKS; ++i) {
        acquire(&(blocks + i)->lock);
        dump.free += blocks[i].info.free;
        dump.total += blocks[i].info.total;
        for (int j = 0; j < STANDART_DEPTH; ++j) {
            dump.free_by_size[j] += blocks[i].info.free_by_size[j];
        }
        release(&(blocks + i)->lock);
    }

    printf("Total = %d, Free = %d\n", dump.total, dump.free);
    for (int i = 0; i < STANDART_DEPTH; ++i) { 
        printf("%d ", dump.free_by_size[i]);
    }
    printf("\n");
}