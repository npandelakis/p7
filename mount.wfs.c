#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "wfs.h"

struct wfs_state {
    FILE *logfile;
    char *rootdir;
    char *mmap_pointer;
};




//https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html
static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode) {
    return 0;
}

struct wfs_log_entry *search_path_helper(int inode_number) {
    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;
    struct wfs_sb *superblock = (struct wfs_sb *) &(wfs_data);

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb) + sizeof(struct wfs_log_entry);

    //int inode_search = 0;
    struct wfs_log_entry *log_entry;
    struct wfs_log_entry * last_log_entry;
    while(cur_offset < (superblock->head)) {
        // Get current log entry
        log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
        
        if (log_entry->inode.inode_number == 0 && log_entry->inode.size == 0) {
            break;
        }

        // Go to next entry if this isn't the inode we want
        if (log_entry->inode.inode_number != inode_number) {
            // printf("Incorrect inode (dir)\n");
            cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
            continue;
        }
        
        // Go to next entry if this entry has been deleted
        if(log_entry->inode.deleted == 1) { 
            cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
            continue;
        }

        last_log_entry = log_entry;

        cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
    }

    return last_log_entry;
}

struct wfs_log_entry *search_inode(int inode_number) {
    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;
    struct wfs_sb *superblock = (struct wfs_sb *) &(wfs_data);

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb) + sizeof(struct wfs_log_entry);

    struct wfs_log_entry *log_entry;
    struct wfs_log_entry * last_log_entry;
    while(cur_offset < (superblock->head)) {
        // printf("\n\nToken1: %s\n", token1);
        // printf("Token 2: %s\n", token2);
        // Get current log entry
        log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
        // printf("inode of current log: %d\n",log_entry->inode.inode_number);
        // printf("current offset: %d\n",cur_offset);
        
        if (log_entry->inode.inode_number == 0 && log_entry->inode.size == 0) {
            break;
        }

        // Go to next entry if this isn't the inode we want
        if (log_entry->inode.inode_number != inode_number) {
            // printf("Incorrect inode (dir)\n");
            cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
            continue;
        }
        
        // Go to next entry if this entry has been deleted
        if(log_entry->inode.deleted == 1) { 
            //printf("Deleted inode (dir)\n");
            cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
            continue;
        }

        last_log_entry = log_entry;

        cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
    }
    return last_log_entry;
}


struct wfs_log_entry *search_path(char * path) {
    char* token1 = strtok(path,"/");
    char* token2 = strtok(NULL,"/");
    struct wfs_log_entry *log_entry;
    int inode_search = 0;

    if (token2 != NULL) {
        int found = 0;
        while (token2 !=NULL) {
            log_entry = search_path_helper(inode_search);
            int num_entries = (log_entry->inode.size)/(sizeof(struct wfs_dentry));
            printf("Num entries: %d\n", num_entries);
            for(int i = 0; i < num_entries; i++) {
                struct wfs_dentry *dentry = (struct wfs_dentry *) &(log_entry->data[i * sizeof(struct wfs_dentry)]);
                char *directory = dentry->name;
                printf("Dir name: %s\n", directory);
                printf("token1: %s, Directory: %s\n",token1, directory);
                if(strcmp(token1,directory)==0) {
                    printf("****match\n");
                    inode_search = dentry->inode_number; //We found the inode of the log entry with the subdir
                    token1 = token2;
                    token2 = strtok(NULL,"/");
                    printf("token 1 new: %s\n", token1);
                    found = 1;
                    break;
                }
            }
            if (!found) {
                break;
            }
            found = 0;
        }
    }

    printf("After loop: %s, %d\n", token1, inode_search);

    log_entry = search_path_helper(inode_search);
    int num_entries = (log_entry->inode.size)/(sizeof(struct wfs_dentry));
    printf("-----Num entries: %d\n", num_entries);
    for(int i = 0; i < num_entries; i++) {
        struct wfs_dentry *dentry = (struct wfs_dentry *) &(log_entry->data[i * sizeof(struct wfs_dentry)]);
        char *directory = dentry->name;
        printf("Dir name: %s\n", directory);
        printf("token1: %s, Directory: %s\n",token1, directory);
        if(strcmp(token1,directory)==0) {
            printf("****match\n");
            inode_search = dentry->inode_number; //We found the inode of the log entry with the subdir
            break;
        }
    }
    
    if (inode_search < 0) {
        return NULL;
    }

    struct wfs_log_entry *final_log_entry = search_inode(inode_search);

    printf("File found\n");
    printf("Data: %s\n", final_log_entry->data);
    return final_log_entry;

    return NULL;
}

static int wfs_getattr(const char *path, struct stat *stbuf) {
    
    char path_copy[strlen(path) + 1];
    strcpy(path_copy,path);

    struct wfs_log_entry *log_entry = search_path(path_copy);

    if (log_entry == NULL) {
        return -1;
    }

    struct wfs_inode inode = log_entry->inode;

    stbuf->st_uid = inode.uid;
    stbuf->st_gid = inode.gid;
    stbuf->st_atime = inode.atime;
    stbuf->st_mtime = inode.mtime;
    stbuf->st_mode = inode.mode;
    stbuf->st_ctime = inode.ctime;
    stbuf->st_size = inode.size;
    stbuf->st_nlink = inode.links;

    return 0;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    
    // char path_copy[strlen(path) + 1];

    // strcpy(path_copy, path);

    // struct wfs_log_entry *log_entry = search_path(path_copy);

    // char *data = log_entry->data;

    // int i = 0;
    // char c;
    // while(i < log_entry->inode.size && (c = data[i]) != EOF && i < size ) {
    //     buf[i] = c;
    //     i++;
    // }

    // if (i < size) {
    //     buf[i] = EOF;
    // }

    // printf("buf: %s\n",buf);
    strcpy(buf,"content");
    buf[8] = EOF;
    return 8;
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_unlink(const char* path) {
    return 0;
}



static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};


int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    struct wfs_state *wfs_data = malloc(sizeof(struct wfs_state));

    char *logfile_name = argv[argc - 2];
    
    wfs_data->logfile = fopen(logfile_name, "r");

    printf("%s\n", logfile_name);
    printf("%p\n", (void *) wfs_data->logfile);

    struct stat st;
    stat(logfile_name, &st);

    printf("%ld\n", st.st_size);

    wfs_data->mmap_pointer = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fileno(wfs_data->logfile),0);

    if (wfs_data->mmap_pointer == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }


    wfs_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    return fuse_main(argc, argv, &ops, wfs_data);
}