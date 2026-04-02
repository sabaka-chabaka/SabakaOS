#pragma once
#include <stdint.h>

#define FAT32_OK            0
#define FAT32_ERR_IO       -1
#define FAT32_ERR_NOENT    -2
#define FAT32_ERR_NOTDIR   -3
#define FAT32_ERR_ISDIR    -4
#define FAT32_ERR_NOSPACE  -5
#define FAT32_ERR_EXIST    -6
#define FAT32_ERR_INVAL    -7

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

#define FAT32_MAX_NAME 256
#define FAT32_EOF      0x0FFFFFFF

struct Fat32Entry {
    char     name[FAT32_MAX_NAME];
    uint8_t  attr;
    uint32_t cluster;
    uint32_t size;
    uint32_t dir_cluster;
    uint32_t dir_index;
};

bool fat32_init();

int fat32_find(uint32_t dir_cluster, const char* name, Fat32Entry* out);
int fat32_resolve(const char* path, Fat32Entry* out);

int fat32_readdir(uint32_t dir_cluster, uint32_t index, Fat32Entry* out);

int fat32_read(const Fat32Entry* entry, uint8_t* buf, uint32_t offset, uint32_t size);

int fat32_write(Fat32Entry* entry, const uint8_t* buf, uint32_t offset, uint32_t size);

int fat32_create(uint32_t dir_cluster, const char* name, Fat32Entry* out);
int fat32_mkdir (uint32_t dir_cluster, const char* name, Fat32Entry* out);

int fat32_unlink(Fat32Entry* entry);
int fat32_rmdir (Fat32Entry* entry);

int fat32_rename(Fat32Entry* entry, uint32_t new_dir_cluster, const char* new_name);

bool fat32_is_mounted();
uint32_t fat32_root_cluster();