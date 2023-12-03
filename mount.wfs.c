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


static int wfs_getattr(const char *path, struct stat *stbuf) {
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = time(NULL);
    stbuf->st_mtime = time(NULL);
    if(strcmp(path,"/")==0) {
        stbuf->st_mode = S_IFDIR | 0755; //Root directory with r/w/x
        return 0; // Return 0 on success
    }
    else {
        //Need to expand for other paths
        stbuf->st_mode = S_IFREG | 0644; //Regular file with r/w
        return 0; // Return 0 on success
    }
    return -1; // Return error if path doesn't exist
}


//https://www.cs.hmc.edu/~geoff/classes/hmc.cs135.201001/homework/fuse/fuse_doc.html
static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode) {
    return 0;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    struct wfs_state *wfs_data = ( struct wfs_state *) fuse_get_context()->private_data;

    // Start at first inode
    int cur_offset = sizeof(struct wfs_sb) + sizeof(struct wfs_log_entry);

    printf("here\n");

    struct wfs_log_entry *log_entry = (struct wfs_log_entry *) &(wfs_data->mmap_pointer[cur_offset]);

    printf("log entry inode: %d\n", log_entry->inode.deleted);

    return size;
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
    
    wfs_data->logfile = fopen(logfile_name, "wr");

    printf("%s\n", logfile_name);
    printf("%p\n", (void *) wfs_data->logfile);

    struct stat st;
    stat(logfile_name, &st);

    wfs_data->mmap_pointer = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(wfs_data->logfile),0);

    wfs_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    return fuse_main(argc, argv, &ops, wfs_data);
}