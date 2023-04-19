#include <stdint.h>
#include <stdio.h>

#include "inode.h"
#include "blocks.h"
#include "bitmap.h"

//the second block is the inode block

void print_inode(inode_t *node) {
    return;
}

//gets the inode at an inum
inode_t *get_inode(int inum) {
    return (inode_t*) blocks_get_block(1) + (inum * sizeof(inode_t));
}

//find a free inode, set it as taken, and return the inum. If can't find,
//return -1
int alloc_inode() {
    int inum = 0;
    for(int i = 0; i < 256; i++) {
        int bit = bitmap_get(get_inode_bitmap(), i);
        if(bit == 0) {
            bitmap_put(get_inode_bitmap(), i, 1);
            return i;
        }
    }
    return -1;
}

//given a taken inum, free it
void free_inode(int inode_num) {
    inode_t *node = get_inode(inode_num);
    free_block(node->block);
    bitmap_put(get_inode_bitmap(), inode_num, 0);
}

int grow_inode(inode_t *node, int size);
int shrink_inode(inode_t *node, int size);
int inode_get_bnum(inode_t *node, int fbnum);
