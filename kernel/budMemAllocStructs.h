#define CNT_MEM_BLOCKS 62

#include "spinlock.h"
#include "budMemInfo.h"

struct MemNode {
    // start_ptr; uint64 - adress (physical) of first ptr avalible for this pages
    // maxFreePage: uint16  - maximal count consecutive free pages
    // leftChild, rightChild: MemNode*

    struct MemNode *leftChild, *rightChild, *head;
    uint16 depth;
    uint64 start_ptr;
    uint16 maxFreePage;
    uint16 cnt_page;
    uint8 is_used;
};


struct BlockMem
{
    // root: MemNode
    // info: BlocksInfo

    struct spinlock lock;
    struct MemNode *root;
    struct BlocksInfo info;
};

// ask here. If no - ask child
// if have found - update free space for this node and upper
uint64
alloc_in_node(struct MemNode *node_ptr, uint16 size, struct BlockMem *block_ptr);


void 
update_node_free_cnt(struct MemNode *node_ptr, struct BlockMem *block_ptr);

// find this block nby ptr - then clean 
// when have found - update free space for this node and upper
void
free_in_node(struct MemNode *node_ptr, uint64 start_ptr, struct BlockMem *block_ptr);

// call free_in_node for root
void 
free_in_block(struct BlockMem *block_ptr, uint64 start_ptr);

// create tree for this block
void
init_block(struct BlockMem* block_ptr);

// call alloc_in_node for root
uint64 
alloc_in_block(struct BlockMem *block_ptr, uint16 size);



