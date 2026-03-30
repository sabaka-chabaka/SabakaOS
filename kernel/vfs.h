#pragma once
#include <stdint.h>

#define VFS_MAX_NAME     64
#define VFS_MAX_CHILDREN 16
#define VFS_MAX_NODES    128
#define VFS_MAX_FILE_DATA 4096

typedef enum {
    VFS_FILE = 1,
    VFS_DIR  = 2,
} VfsNodeType;

struct VfsNode;

struct VfsOps {
    int      (*read)   (VfsNode* node, uint8_t* buf, uint32_t offset, uint32_t size);
    int      (*write)  (VfsNode* node, const uint8_t* buf, uint32_t offset, uint32_t size);
    VfsNode* (*finddir)(VfsNode* node, const char* name);
    VfsNode* (*mkdir)  (VfsNode* node, const char* name);
    VfsNode* (*create) (VfsNode* node, const char* name);
    int      (*readdir)(VfsNode* node, uint32_t index, char* name_out);
};

struct VfsNode {
    char        name[VFS_MAX_NAME];
    VfsNodeType type;
    uint32_t    size;
    uint8_t*    data;
    VfsOps*     ops;
    VfsNode*    children[VFS_MAX_CHILDREN];
    uint32_t    child_count;
    VfsNode*    parent;
};

void     vfs_init();
VfsNode* vfs_root();
VfsNode* vfs_cwd();
int      vfs_chdir(const char* path);

VfsNode* vfs_finddir(VfsNode* node, const char* name);
VfsNode* vfs_mkdir  (VfsNode* node, const char* name);
VfsNode* vfs_create (VfsNode* node, const char* name);
int      vfs_read   (VfsNode* node, uint8_t* buf, uint32_t offset, uint32_t size);
int      vfs_write  (VfsNode* node, const uint8_t* buf, uint32_t offset, uint32_t size);
int      vfs_readdir(VfsNode* node, uint32_t index, char* name_out);

VfsNode* vfs_resolve_path(const char* path);
void     vfs_get_cwd_path(char* buf, uint32_t bufsize);
