#define STANDART_DEPTH 10
#include "types.h"

struct BlocksInfo
{
    // total: uint16
    // free:  uint16
    // free_by_size: uint16[10];

    uint16 total;
    uint16 free;
    uint16 free_by_size[STANDART_DEPTH];
};
