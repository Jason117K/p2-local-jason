// Directory manipulation functions.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define MAX_NAME_LENGTH 31

#include "blocks.h"
#include "inode.h"
#include "slist.h"

typedef struct direntry {
  char name[MAX_NAME_LENGTH];
  int inum;
} direntry_t;

typedef struct dir_header {
  char name[MAX_NAME_LENGTH];
  int num_entries;
  int inode_num;
} dir_header_t;

void directory_init();
int tree_lookup(const char *path);
int directory_lookup(inode_t *dd, const char *name);
int directory_put(inode_t *dd, const char *name, int inum);
int directory_delete(inode_t *dd, const char *name);
slist_t *directory_list(const char *path);
//void print_directory(inode_t *dd);

#endif
