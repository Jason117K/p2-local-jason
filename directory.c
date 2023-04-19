// Directory manipulation functions.
//
// Feel free to use as inspiration.

// based on cs3650 starter code

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "blocks.h"
#include "inode.h"
#include "slist.h"
#include "directory.h"

//initializes a directory at a pointer to a block
void directory_init(void* block,  char* name, int this_inum, int parent_inum) { 
    //at the beginning, put header
    dir_header_t *header = (dir_header_t*) block;
    strcpy(header->name, name);
    header->num_entries = 2;
    header->inode_num = this_inum;
    
    //first entry is itself
    header++;
    direntry_t *self = (direntry_t*) header;
    strcpy(self->name, ".");
    self->inum = this_inum;

    //second entry is the parent
    direntry_t *parent = self + 1;
    strcpy(parent->name, "..");
    parent->inum = parent_inum;
    
}

//returns the inode number for some path(only checks the root for now)
//returns -1 upon failure to find the path
//TODO make recursive search
int tree_lookup(const char *path) {
    //root directory at block 2
    fprintf(stderr, "tree lookup for path %s\n", path);
    
    
    char* cpy = malloc(strlen(path));
    //create copy for exploding
    strcpy(cpy, path);
    slist_t *list = s_explode(cpy, '/');
    free(cpy);
    slist_t *temp = list;

    //check for the root, return inode 0
    if(strcmp(path, "/") == 0) {
        return 0;
    }
    inode_t *curr_inode = get_inode(0);
    
    int next_inum = directory_lookup(curr_inode, temp->data);
    
    //if there is a leading /, slist will give an empty entry at the start, 
    //so if couldn't find try again
    if(next_inum == -1) {
        temp = temp->next;
        next_inum = directory_lookup(curr_inode, temp->data);
        if(temp->next == NULL) {
            fprintf(stderr, "exiting here\n");
            return next_inum;
        }
    }

    curr_inode = get_inode(next_inum);

    //until we reach the last name in the path, continue to search through
    //each directory
    while (temp->next != NULL) {
        temp = temp->next;
        //if we reach a file that isn't a directory, we can't search it
        //so return -1 to show an error
        if(curr_inode->type != 1) {
            fprintf(stderr, "tried to recursively lookup into not a directory\n");
            return -1;
        }
        next_inum = directory_lookup(curr_inode, temp->data);
        curr_inode = get_inode(next_inum);
    }
    s_free(list);

    return next_inum;
}

//returns the inode number for some path in the directory
//returns -1 upon failure
int directory_lookup(inode_t *dd, const char *name) {
    dir_header_t *header = blocks_get_block(dd->block);
    int entries = header->num_entries;
    fprintf(stderr, "looking for %s in directory %s\n", name, header->name);

    //if looking for this directory
    if(strcmp(name, "/") == 0) {
        return header->inode_num;
    }

    // increment header by size of the header 
    header++;

    direntry_t *curr_entry = (direntry_t*) header;
    //search every entry in the directory for a name match
    for(int i = 0; i < entries; i++){
        if(strcmp(curr_entry->name, name) == 0){
            return curr_entry->inum;
        }
        //also support entering names as "/name" instead of just "name"
        if(strcmp(curr_entry->name, (name + 1)) == 0 && *name == '/') {
            return curr_entry->inum;
        }
        curr_entry++;		
    }
    return -1;
}

//create a link between a name and inode number within a directory
int directory_put(inode_t *dd, const char *name, int inum) {
    fprintf(stderr, "putting \n");
    dir_header_t *header = blocks_get_block(dd->block);
    int n = header->num_entries;
    header->num_entries++;

    //find the next available spot in the directory
    header++;
    direntry_t *start = (direntry_t*) header;
    direntry_t *end = start += n;

    //set the data
    strcpy(end->name, name);
    end->inum = inum;
    
    //0 on success
    return 0;
}

//delete a name from a directory, return 0 on success and -1 on failure
int directory_delete(inode_t *dd, const char *name) {
    //get the header of the directory
    dir_header_t *header = blocks_get_block(dd->block);
    fprintf(stderr, "deleting %s from %s\n", name, header->name);
    int n = header->num_entries;
    direntry_t *start = (direntry_t*) (header + 1);
    direntry_t *last = start + (n - 1);
    direntry_t *curr = start;

    //iterate through all entries except the last one for a name match
    for(int i=0; i < n - 1; i++) {
        fprintf(stderr, "checking %s\n", curr->name);
        if(strcmp(curr->name, name) == 0) {
            free_inode(curr->inum);
            //to remove, and keep the directory neat, simply replace this reference
            //with the last reference in the directory, and decrement the number of
            //entries
            strcpy(curr->name, last->name);
            curr->inum = last->inum;
            header->num_entries--;
            return 0;
        }
        curr++;
    }
    //if it's the last element that needs to be removed, simply decrement
    //the number of entries in the directory. All searches will stop before
    //reaching this entry
    if(strcmp(last->name, name) == 0 ) {
        header->num_entries--;
        return 0;
    }
    //could not find in the directory, return -1
    return -1;
}

//retruns a linked list with the names of all files in this directory
slist_t *directory_list(const char *path) {
    fprintf(stderr, "dir list for path: %s\n", path);
    int inum = tree_lookup(path);
    inode_t *node = get_inode(inum);
    dir_header_t *header = blocks_get_block(node->block);
    
    //get the header of the directory
    int n = header->num_entries;
    header++;

    //get the first entry
    direntry_t *curr_entry = (direntry_t*) header;
    slist_t *list = NULL;

    //go through each entry, and cons the name onto the list to return
    for(int i = 0; i < n; i++) {
        list = s_cons(curr_entry->name, list);
        curr_entry++;
    }
    return list;
}

//void print_directory(inode_t *dd);

