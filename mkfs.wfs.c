#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wfs.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        return -1;
    }

    char *file_name = argv[1];
    
    FILE *disk = fopen(file_name,"w");

    struct wfs_sb superblock;
    superblock.magic = WFS_MAGIC;
    superblock.head = sizeof(struct wfs_sb) + sizeof(struct wfs_log_entry);

    struct wfs_inode inode;
    inode.inode_number = 0;
    inode.deleted = 0;
    inode.mode = S_IFDIR;
    inode.flags = 0;
    inode.size = 0;
    inode.atime = 0;
    inode.mtime = 0;
    inode.ctime = 0;
    inode.links = 0;

    struct wfs_dentry dentry;
    dentry.inode_number = 0;
    strcpy(dentry.name,"/");


    struct wfs_log_entry *log_entry = malloc(sizeof(struct wfs_log_entry) + sizeof(struct wfs_dentry));
    memset(log_entry, 0, sizeof(*log_entry));

    log_entry->inode = inode;
    memcpy(&log_entry->data[0], &dentry, sizeof(struct wfs_dentry));

    fwrite(&superblock, sizeof(struct wfs_sb), 1, disk);
    fwrite(log_entry, sizeof(struct wfs_log_entry), 1, disk);

    //printf("%ld, %ld\n",sb_written,logentry_written);

    fclose(disk);
    free(log_entry);

    return 0;
}

