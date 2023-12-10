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
    int next_inode;
};


struct wfs_log_entry *search_path_helper(int inode_number) {
    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;
    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb);// + sizeof(struct wfs_log_entry);

    //int inode_search = 0;
    struct wfs_log_entry *log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
    struct wfs_log_entry * last_log_entry = log_entry;
    while(cur_offset < (superblock->head)) {
        // Get current log entry

        log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
        
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
    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb);// + sizeof(struct wfs_log_entry);

    struct wfs_log_entry *log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
    struct wfs_log_entry * last_log_entry = log_entry;
    while(cur_offset < (superblock->head)) {
        // Get current log entry

        log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);

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

int get_next_inode(struct wfs_state *wfs_data) {

    int biggest_inode = 0;
    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb);// + sizeof(struct wfs_log_entry);

    struct wfs_log_entry *log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);
    while(cur_offset < (superblock->head)) {
        // Get current log entry
        log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);

        // Go to next entry if this entry has been deleted
        if(log_entry->inode.deleted == 1) {
            //printf("Deleted inode (dir)\n");
            cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
            continue;
        }
        if(log_entry->inode.inode_number > biggest_inode) {
            biggest_inode = log_entry->inode.inode_number;
        }

        cur_offset += log_entry->inode.size + sizeof(struct wfs_inode); //recalculate log_entry offset to find next log_entry
    }

    return biggest_inode + 1;
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
            for(int i = 0; i < num_entries; i++) {
                struct wfs_dentry *dentry = (struct wfs_dentry *) &(log_entry->data[i * sizeof(struct wfs_dentry)]);
                char *directory = dentry->name;
                if(strcmp(token1,directory)==0) {
                    inode_search = dentry->inode_number; //We found the inode of the log entry with the subdir
                    token1 = token2;
                    token2 = strtok(NULL,"/");
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

    int last_inode = -1;
    if (token1 != NULL) {
        log_entry = search_path_helper(inode_search);
        int num_entries = (log_entry->inode.size)/(sizeof(struct wfs_dentry));
        for(int i = 0; i < num_entries; i++) {
            struct wfs_dentry *dentry = (struct wfs_dentry *) &(log_entry->data[i * sizeof(struct wfs_dentry)]);
            char *directory = dentry->name;
            if(strcmp(token1,directory)==0) {
                last_inode = dentry->inode_number; //We found the inode of the log entry with the subdir
                break;
            }
        }
    }

    if (last_inode < 0) {
        return NULL;
    }

    struct wfs_log_entry *final_log_entry = search_inode(last_inode);

    return final_log_entry;

    return NULL;
}

static int wfs_getattr(const char *path, struct stat *stbuf) {

    char path_copy[strlen(path) + 1];
    strcpy(path_copy,path);

    struct wfs_log_entry *log_entry = search_path(path_copy);

    if (log_entry == NULL) {
        return -ENOENT;
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

    char path_copy[strlen(path) + 1];

    strcpy(path_copy, path);

    struct wfs_log_entry *log_entry = search_path(path_copy);

    char *data = log_entry->data;

    int i = 0;
    char c;
    while(i < log_entry->inode.size && (c = data[i]) != EOF && i < size ) {
        buf[i] = c;
        i++;
    }

    i++;
    return i;
}

struct wfs_log_entry *get_parent_log(const char * path) {
    int str_len = strlen(path);

    int index = 0;

    char c;
    for (int i = 0; i < str_len; i++) {
        c = path[i];
        if (c == '/') {
            index = i;
        }
    }

    char *path_copy = malloc(sizeof(char)*index);

    memcpy(path_copy, path, index);
    path_copy[index] = '\0';

    struct wfs_log_entry *log_entry = search_path(path_copy);
    if (strcmp(path_copy,"") == 0) {
	    log_entry = search_inode(0);
    }

    return log_entry;
}

char *get_path_leaf(const char *path) {
    char *path_copy = malloc(sizeof(path)*(strlen(path) + 1));

    strcpy(path_copy, path);

    char *token1 = strtok(path_copy,"/");
    char *token2 = strtok(NULL,"/");

    while (token2 != NULL) {
        token1 = token2;
	token2 = strtok(NULL, "/");
    }

    return token1;
}

static int mk_private(const char* path, mode_t mode, mode_t type) {
    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;

    //Grab log entry of directory above last directory in path
    struct wfs_log_entry *parent_log_entry = get_parent_log(path);

    struct wfs_log_entry *new_parent_log_entry = malloc(sizeof(struct wfs_log_entry) + parent_log_entry->inode.size + sizeof(struct wfs_dentry));

    //create new data entry for parent log entry and add data
    struct wfs_dentry new_parent_dentry;

    new_parent_log_entry->inode.inode_number = parent_log_entry->inode.inode_number;
    new_parent_log_entry->inode.mode = parent_log_entry->inode.mode;
    new_parent_log_entry->inode.deleted=0;
    new_parent_log_entry->inode.flags = parent_log_entry->inode.flags;
    new_parent_log_entry->inode.atime = time(NULL);
    new_parent_log_entry->inode.mtime = time(NULL);
    new_parent_log_entry->inode.ctime = time(NULL);
    new_parent_log_entry->inode.links = parent_log_entry->inode.links;
    new_parent_log_entry->inode.uid = getuid();
    new_parent_log_entry->inode.gid = getgid();

    new_parent_dentry.inode_number = wfs_data->next_inode;
    strcpy(new_parent_dentry.name,get_path_leaf(path));
    memcpy(&new_parent_log_entry->data, parent_log_entry->data, parent_log_entry->inode.size);
    memcpy(&new_parent_log_entry->data[parent_log_entry->inode.size], &new_parent_dentry, sizeof(struct wfs_dentry));

    //update inode size after writing new dentry
    new_parent_log_entry->inode.size = parent_log_entry->inode.size + sizeof(struct wfs_dentry);

    //write our new parent log to our mmap
    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);
    memcpy(&(wfs_data->mmap_pointer[superblock->head]), new_parent_log_entry, sizeof(struct wfs_log_entry) + new_parent_log_entry->inode.size);

    superblock->head += sizeof(struct wfs_log_entry) + new_parent_log_entry->inode.size;

    //create new log entry for new node
    struct wfs_log_entry new_log_entry;

    new_log_entry.inode.inode_number = wfs_data->next_inode;
    new_log_entry.inode.deleted = 0;
    new_log_entry.inode.mode = type | mode;
    new_log_entry.inode.flags = 0;
    new_log_entry.inode.size = 0;
    new_log_entry.inode.atime = time(NULL);
    new_log_entry.inode.mtime = time(NULL);
    new_log_entry.inode.ctime = time(NULL);
    new_log_entry.inode.links = 1;
    new_log_entry.inode.uid = getuid();
    new_log_entry.inode.uid = getgid();

    //write our new log entry to our mmap
    memcpy(&(wfs_data->mmap_pointer[superblock->head]), &new_log_entry, sizeof(struct wfs_log_entry));

    superblock->head += sizeof(struct wfs_log_entry);

    wfs_data->next_inode++;

    free(new_parent_log_entry);
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return mk_private(path, mode, S_IFREG);
}

static int wfs_mkdir(const char* path, mode_t mode) {
    return mk_private(path, mode, S_IFDIR);
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {

    char *path_copy = malloc(sizeof(path)*(strlen(path) + 1));

    strcpy(path_copy, path);

    path_copy[strlen(path) + 1] = '\0';

    struct wfs_log_entry *log_entry = search_path(path_copy);

    int new_size = log_entry->inode.size;

    int val = (offset + size) - new_size;

    new_size = new_size + val;

    struct wfs_log_entry *new_log_entry = malloc(sizeof(struct wfs_log_entry) + new_size);

    new_log_entry->inode = log_entry->inode;

    memcpy(new_log_entry->data, log_entry->data, log_entry->inode.size);

    new_log_entry->inode.size = new_size;

    memcpy(new_log_entry->data + offset, buf, size);

    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;
    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);
    memcpy(&(wfs_data->mmap_pointer[superblock->head]), new_log_entry, sizeof(struct wfs_log_entry) + new_log_entry->inode.size);

    superblock->head += sizeof(struct wfs_log_entry) + new_log_entry->inode.size;

    return size;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_unlink(const char* path) {
    char *path_copy = malloc(sizeof(path)*(strlen(path) + 1));

    strcpy(path_copy, path);

    path_copy[strlen(path) + 1] = '\0';

    struct wfs_log_entry *log_entry = search_path(path_copy);

    log_entry->inode.deleted = 1;
    log_entry->inode.links = 0;

    struct wfs_state *wfs_data = (struct wfs_state *) fuse_get_context()->private_data;

    //Grab log entry of directory above last directory in path
    struct wfs_log_entry *parent_log_entry = get_parent_log(path);
    
    struct wfs_log_entry *new_parent_log_entry = malloc(sizeof(struct wfs_log_entry) + parent_log_entry->inode.size + sizeof(struct wfs_dentry));

    new_parent_log_entry->inode.inode_number = parent_log_entry->inode.inode_number;
    new_parent_log_entry->inode.mode = parent_log_entry->inode.mode;
    new_parent_log_entry->inode.deleted=0;
    new_parent_log_entry->inode.flags = parent_log_entry->inode.flags;
    new_parent_log_entry->inode.atime = time(NULL);
    new_parent_log_entry->inode.mtime = time(NULL);
    new_parent_log_entry->inode.ctime = time(NULL);
    new_parent_log_entry->inode.links = parent_log_entry->inode.links;
    new_parent_log_entry->inode.uid = getuid();
    new_parent_log_entry->inode.gid = getgid();

    char *deleted_path = get_path_leaf(path);
    struct wfs_dentry *dentry = (struct wfs_dentry *) (parent_log_entry->data);
    char *new_data = malloc(parent_log_entry->inode.size - sizeof(struct wfs_dentry));
    char *cur_data = new_data;
    for (int i = 0; i < parent_log_entry->inode.size; i += sizeof(struct wfs_dentry)) {
        if (strcmp(deleted_path, dentry->name) != 0) {
            memcpy(cur_data, dentry, sizeof(struct wfs_dentry));
            cur_data += sizeof(struct wfs_dentry);
        }
        dentry = (struct wfs_dentry *) (parent_log_entry->data + i + sizeof(struct wfs_dentry));
    }


    new_parent_log_entry->inode.size = parent_log_entry->inode.size - sizeof(struct wfs_dentry);


    memcpy(&new_parent_log_entry->data, new_data, new_parent_log_entry->inode.size);

    struct wfs_sb *superblock = (struct wfs_sb *) (wfs_data->mmap_pointer);
    memcpy(&(wfs_data->mmap_pointer[superblock->head]), new_parent_log_entry, sizeof(struct wfs_log_entry) + new_parent_log_entry->inode.size);

    superblock->head += sizeof(struct wfs_log_entry) + new_parent_log_entry->inode.size;

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

    wfs_data->logfile = fopen(logfile_name, "r+");

    struct stat st;
    stat(logfile_name, &st);

    wfs_data->mmap_pointer = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(wfs_data->logfile),0);

    if (wfs_data->mmap_pointer == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    wfs_data->next_inode = get_next_inode(wfs_data);
    
    wfs_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    return fuse_main(argc, argv, &ops, wfs_data);
}