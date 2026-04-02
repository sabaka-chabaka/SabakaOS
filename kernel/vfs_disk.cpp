#include "vfs_disk.h"
#include "vfs.h"
#include "fat32.h"
#include "heap.h"
#include "kstring.h"

static bool s_fat32_mounted = false;

static Fat32Entry* get_entry(VfsNode* node) {
    return (Fat32Entry*)node->data;
}

static int disk_read(VfsNode* node, uint8_t* buf, uint32_t offset, uint32_t size) {
    Fat32Entry* e = get_entry(node);
    if (!e) return -1;
    return fat32_read(e, buf, offset, size);
}

static int disk_write(VfsNode* node, const uint8_t* buf, uint32_t offset, uint32_t size) {
    Fat32Entry* e = get_entry(node);
    if (!e) return -1;
    int r = fat32_write(e, buf, offset, size);
    if (r > 0) node->size = e->size;
    return r;
}

static VfsNode* make_disk_node(const char* name, Fat32Entry* entry, VfsOps* ops);

static VfsNode* disk_finddir(VfsNode* node, const char* name) {
    Fat32Entry* dir_e = get_entry(node);
    uint32_t dir_cluster = dir_e ? dir_e->cluster : fat32_root_cluster();

    Fat32Entry found;
    if (fat32_find(dir_cluster, name, &found) != FAT32_OK) return nullptr;

    Fat32Entry* stored = (Fat32Entry*)kmalloc(sizeof(Fat32Entry));
    if (!stored) return nullptr;
    *stored = found;
    return make_disk_node(found.name, stored, node->ops);
}

static VfsNode* disk_mkdir(VfsNode* node, const char* name) {
    Fat32Entry* dir_e = get_entry(node);
    uint32_t dir_cluster = dir_e ? dir_e->cluster : fat32_root_cluster();

    Fat32Entry new_e;
    if (fat32_mkdir(dir_cluster, name, &new_e) != FAT32_OK) return nullptr;

    Fat32Entry* stored = (Fat32Entry*)kmalloc(sizeof(Fat32Entry));
    if (!stored) return nullptr;
    *stored = new_e;
    return make_disk_node(new_e.name, stored, node->ops);
}

static VfsNode* disk_create(VfsNode* node, const char* name) {
    Fat32Entry* dir_e = get_entry(node);
    uint32_t dir_cluster = dir_e ? dir_e->cluster : fat32_root_cluster();

    Fat32Entry new_e;
    if (fat32_create(dir_cluster, name, &new_e) != FAT32_OK) return nullptr;

    Fat32Entry* stored = (Fat32Entry*)kmalloc(sizeof(Fat32Entry));
    if (!stored) return nullptr;
    *stored = new_e;
    return make_disk_node(new_e.name, stored, node->ops);
}

static int disk_readdir(VfsNode* node, uint32_t index, char* name_out) {
    Fat32Entry* dir_e = get_entry(node);
    uint32_t dir_cluster = dir_e ? dir_e->cluster : fat32_root_cluster();

    uint32_t real_index = 0;
    Fat32Entry e;
    for (uint32_t i = 0; ; i++) {
        int r = fat32_readdir(dir_cluster, i, &e);
        if (r != FAT32_OK) return -1;
        if (kstrcmp(e.name, ".") == 0 || kstrcmp(e.name, "..") == 0) continue;
        if (real_index == index) {
            kstrcpy(name_out, e.name);
            return 0;
        }
        real_index++;
    }
}

static VfsOps disk_ops = {
    disk_read,
    disk_write,
    disk_finddir,
    disk_mkdir,
    disk_create,
    disk_readdir,
};

static VfsNode* make_disk_node(const char* name, Fat32Entry* entry, VfsOps* ops) {
    VfsNode* n = (VfsNode*)kmalloc(sizeof(VfsNode));
    if (!n) return nullptr;
    kstrcpy(n->name, name);
    n->type        = (entry && (entry->attr & FAT32_ATTR_DIRECTORY)) ? VFS_DIR : VFS_FILE;
    n->size        = entry ? entry->size : 0;
    n->data        = (uint8_t*)entry;
    n->ops         = ops ? ops : &disk_ops;
    n->child_count = 0;
    n->parent      = nullptr;
    for (int i = 0; i < VFS_MAX_CHILDREN; i++) n->children[i] = nullptr;
    return n;
}

bool vfs_mount_fat32(const char* mount_point) {
    if (!fat32_is_mounted()) return false;

    VfsNode* root = vfs_root();

    const char* name = mount_point;
    if (name[0] == '/') name++;

    Fat32Entry* root_entry = (Fat32Entry*)kmalloc(sizeof(Fat32Entry));
    if (!root_entry) return false;
    kmemset(root_entry, 0, sizeof(Fat32Entry));
    kstrcpy(root_entry->name, name);
    root_entry->attr    = FAT32_ATTR_DIRECTORY;
    root_entry->cluster = fat32_root_cluster();

    VfsNode* disk_root = make_disk_node(name, root_entry, &disk_ops);
    if (!disk_root) { kfree(root_entry); return false; }
    disk_root->parent = root;

    if (root->child_count >= VFS_MAX_CHILDREN) {
        kfree(root_entry);
        kfree(disk_root);
        return false;
    }
    root->children[root->child_count++] = disk_root;

    s_fat32_mounted = true;
    return true;
}

bool vfs_fat32_mounted() { return s_fat32_mounted; }