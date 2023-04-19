// based on cs3650 starter code

#include <assert.h>
#include <bsd/string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "blocks.h"
#include "bitmap.h"
#include "directory.h"
#include "inode.h"

#define FUSE_USE_VERSION 26
#include <fuse.h>

#define BLOCK_SIZE 4096

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask) {
  // i think we need to check the mode of the function that is passed in with mask before accessing
  fprintf(stderr, "\nattempting to access %s\n", path);
  int rv = 0;

  //get the inum
  int inum = tree_lookup(path);
  //if doesn't exist, then can't access
  if (inum == -1) {
      rv =  -ENOENT;
  }

  printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
  fprintf(stderr, "\ncalling getattr for %s\n", path);

  //get inum
  int inum = tree_lookup(path);

  if(inum == -1) {
    fprintf(stderr, "could not find file or directory %s\n", path);
    return -ENOENT;
  }

  inode_t* node = get_inode(inum);
  // Return some metadata for the root directory...
  int type = node->type;
  printf("inode number: %d\n", inum);
  fprintf(stderr, "type: %d\n", type);
  fprintf(stderr, "mode: %o\n", node->mode);
  fprintf(stderr, "size: %d\n", node->size);
  fprintf(stderr, "block: %d\n", node->block);

  //if a directory
  if(type == 1) {
    dir_header_t *dir = blocks_get_block(inum);
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time( NULL );
	  st->st_mtime = time( NULL );
    st->st_size = node->size;
    st->st_mode = node->mode; 
    st->st_nlink = dir->num_entries + 2;
  }
  //if a file
  else if (type == 0) {
    st->st_uid = getuid();
    st->st_gid = getgid();
    st->st_atime = time( NULL );
	  st->st_mtime = time( NULL );
    st->st_size = node->size;
    st->st_mode = node->mode; 
    st->st_nlink = 1;
    st->st_ino = 0;
    st->st_blksize = 4096;
    st->st_blocks = 1;
  }
  else {
    return -ENOENT;
  }

  //success
  fprintf(stderr, "done getattr\n\n");
  return 0;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  fprintf(stderr, "\ncalling readdir on %s\n", path);
  struct stat st;
  int rv = 0;

  //get inum
  int dir_inum = tree_lookup(path);
  //get inode
  inode_t *dir_inode = get_inode(dir_inum);
  //check that it's a directory
  if(dir_inode->type == 0) {
    fprintf(stderr, "not a directory\n");
    rv = -ENOENT;
  }
  //if it's a directory, get all the files in it
  else if (dir_inode->type == 1) {
    slist_t *list = directory_list(path);
    slist_t *curr_list = list;
    while(curr_list != NULL) {
      fprintf(stderr, "got filename %s\n", curr_list->data);
      char* my_buf;

      //filler needs just the filename, but to get the attributes need
      //the whole file
      if(strcmp(path, "/") == 0) {
        nufs_getattr(curr_list->data, &st);
      }
      else {
        //add the full path to the beginning of each filename to get attributes
        char* my_buf = malloc(strlen(curr_list->data) + strlen(path) + 1);
        strcpy(my_buf, path);
        strcpy(my_buf + (strlen(path)), "/");
        strcpy(my_buf + strlen(path) + 1, curr_list->data);
        nufs_getattr(my_buf, &st);
        free(my_buf);
      }
      //fill with the name
      filler(buf, curr_list->data, &st, 0);
      curr_list = curr_list->next;
    }
    //remember to free the list
    s_free(list);
  }
  
  printf("readdir(%s) -> %d\n", path, rv);
  return rv;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_create(const char *path, mode_t mode, struct fuse_file_info *) {
  int rv = 0;
  fprintf(stderr, "\nstarting create for %s\n", path);

  //breaks the path into the final filename and the path to get there
  slist_t *list = s_explode(path, '/');
  char* name_of_file = malloc(sizeof(char) * 32);
  while(list != NULL) {
    name_of_file = list->data;
    list = list->next;
  }

  char* directory_path = malloc(strlen(path));
  strcpy(directory_path, path);
  int last_slash = strlen(directory_path) - strlen(name_of_file);

  *(directory_path + last_slash) = '\0';

  fprintf(stderr, "filename: %s\n", name_of_file);
  fprintf(stderr, "directory: %s\n", directory_path);

  //set up new inode, alloc block
  int dir_inum = tree_lookup(directory_path);
  fprintf(stderr, "dir_inum: %d\n", dir_inum);
  inode_t *dir_inode = get_inode(dir_inum);
  int new_inum = alloc_inode();
  inode_t *new_inode = get_inode(new_inum);
  int new_block_num = alloc_block();

  fprintf(stderr, "dir_inum: %d\n new_inum: %d\n new_block: %d\n", dir_inum, new_inum, new_block_num);
  
  //set inode with metadata
  new_inode->type = 0;
  new_inode->mode = S_IFREG | mode;
  new_inode->size = 0;
  new_inode->block = new_block_num;
  
  //put new link in the directory
  directory_put(dir_inode, name_of_file, new_inum);
  dir_header_t *header = blocks_get_block(dir_inode->block);

  fprintf(stderr, "create(%s, %04o) -> %d\n", path, mode, rv);
  free(name_of_file);
  return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
  //int rv = nufs_create(path, mode | 040000, NULL);
  fprintf(stderr, "\nmkdir for %s\n", path);

  //See above comment in create, split into new direcotry name
  //and the path to get there
  slist_t *list = s_explode(path, '/');
  char* new_dir = malloc(sizeof(char) * 32);
  while(list != NULL) {
    new_dir = list->data;
    list = list->next;
  }

  char* directory_path = malloc(strlen(path));
  strcpy(directory_path, path);
  int last_slash = strlen(directory_path) - strlen(new_dir);

  *(directory_path + last_slash) = '\0';

  fprintf(stderr, "new directory name: %s\n", new_dir);
  fprintf(stderr, "directory: %s\n", directory_path);

  //set up new inodes, blocks, alloc space, etc. 
  int parent_inum = tree_lookup(directory_path);
  inode_t *parent_inode = get_inode(parent_inum);
  int new_inum = alloc_inode();
  inode_t *new_inode = get_inode(new_inum);
  int new_block_num = alloc_block();

  //set metadata for a new directory
  new_inode->type = 1;
  new_inode->mode = S_IFDIR | mode;
  new_inode->size = 4096 - sizeof(dir_header_t);
  new_inode->block = new_block_num;

  //set up the header for a new direcotry
  dir_header_t *header = blocks_get_block(parent_inode->block);
  directory_put(parent_inode, new_dir, new_inum);
  directory_init(blocks_get_block(new_block_num), new_dir, new_inum, parent_inum);

  free(new_dir);
  return 0;
}

/// unlink a file, no need to keep track of the total number 
int nufs_unlink(const char *path) {
  fprintf(stderr, "\nunlink path: %s\n", path);
  int rv = 0;

  //break a path into the filename and the path to get there
  slist_t *list = s_explode(path, '/');
  char* name_of_file = malloc(sizeof(char) * 32);
  while(list != NULL) {
    name_of_file = list->data;
    list = list->next;
  }

  char* directory_path = malloc(strlen(path));
  strcpy(directory_path, path);
  int last_slash = strlen(directory_path) - strlen(name_of_file);

  *(directory_path + last_slash) = '\0';

  fprintf(stderr, "filename: %s\n", name_of_file);
  fprintf(stderr, "directory: %s\n", directory_path);

  int parent_inum = tree_lookup(directory_path);
  //simply delete from the directory, will deal with the data
  directory_delete(get_inode(parent_inum), name_of_file);

  //free memory
  free(name_of_file);
  return rv;
}


//remove a directory if it doesn't have any files
int nufs_rmdir(const char *path) {
  fprintf(stderr, "\nrmdir path: %s\n", path);
  int rv = 0;

  //break into final filename and path to get there
  slist_t *list = s_explode(path, '/');
  char* name_of_directory = malloc(sizeof(char) * 32);
  while(list != NULL) {
    name_of_directory = list->data;
    list = list->next;
  }

  char* parent_path = malloc(strlen(path));
  strcpy(parent_path, path);
  int last_slash = strlen(parent_path) - strlen(name_of_directory);
  *(parent_path + last_slash) = '\0';

  fprintf(stderr, "filename: %s\n", name_of_directory);
  fprintf(stderr, "directory: %s\n", parent_path);

  //check that the directory can be deleted: i.e. it has no files
  int inum = tree_lookup(path);
  dir_header_t *header = blocks_get_block(get_inode(inum)->block);
  //empty has 2 files, .. and .
  if(header->num_entries == 2) {
    nufs_unlink(path);
  }
  else {
    rv = -ENOENT;
  }
  
  free(name_of_directory);
  return rv;
}

//stub to use write in rename
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi);

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
  int inum = tree_lookup(from);
  if(inum == -1) {
    return -ENOENT;
  }

  //get the inode for metadata, need the block and size
  inode_t *inode = get_inode(inum);
  void *data = blocks_get_block(inode->block);
  int size = inode->size;
  void* buf = malloc(size);
  memcpy(buf, data, size);

  //create new file
  nufs_create(to, inode->mode, NULL);
  //delete old file
  nufs_unlink(from);
  //write old data to new file
  nufs_write(to, buf, size, 0, NULL);

  free(buf);
  return 0;
}

//TODO, or not?
int nufs_chmod(const char *path, mode_t mode) {
  int rv = -1;
  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

//set the size to a smalle size
int nufs_truncate(const char *path, off_t size) {
  int rv = 0;
  fprintf(stderr, "truncate: %s to size %ld\n", path, size);
  int inum = tree_lookup(path);
  inode_t *inode = get_inode(inum);
  inode->size = size;
  return 0;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi) {
  return nufs_access(path, 0777);
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {

  fprintf(stderr, "\nreading from %s\n", path);
  //get the inode for the file
  int inum = tree_lookup(path);
  inode_t *file_node = get_inode(inum);
  void* cur_block = blocks_get_block(file_node->block);

  //copy data, using either minimum of the given size or the 
  //size of the file
  int data_size = file_node->size;
  int to_read;
  if((data_size - offset) > size) {
    to_read = size;
  }
  else {
    to_read = data_size - offset;
  }

  memcpy(buf,cur_block + offset, size);
  return strlen(buf);
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  fprintf(stderr, "\nwrite\n");

  //get the inode
  int inum = tree_lookup(path);
  inode_t *file_node = get_inode(inum);
  void* cur_block = blocks_get_block(file_node->block);
  // create the file if it does not exist with the appropriate permissions.
  if(inum == -1){
      nufs_create(path, 0777, NULL);
      inum = tree_lookup(path);
      file_node = get_inode(inum);
      cur_block = blocks_get_block(file_node->block);
  }
  //copy data into the block
  memcpy(cur_block + offset, buf, size);
  fprintf(stderr, "writing: %s\n", buf);
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, (int) size);

  //set size
  file_node->size = size;
  return (int) size;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2]) {
  //don't care, but stuff like "touch" needs this to work
  return 0;
}

// Extended operations
//idk man I didn't even do anything to this
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  int rv = -1;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

void nufs_init_ops(struct fuse_operations *ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->create = nufs_create;
  // ops->create   = nufs_create; // alternative to mknod
  ops->mkdir = nufs_mkdir;
  //ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);
  argc--;
  nufs_init_ops(&nufs_ops);
  //initalize blocks from the data image
  blocks_init(argv[4]);
  //format disk, if needed
  disk_init();
  //start fuse
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
