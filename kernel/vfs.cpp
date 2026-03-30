#include "vfs.h"
#include "heap.h"
#include "kstring.h"

static VfsNode node_pool[VFS_MAX_NODES];
static uint32_t node_pool_used = 0;

static VfsNode* alloc_node() {
    if (node_pool_used >= VFS_MAX_NODES) return nullptr;
    VfsNode* n = &node_pool[node_pool_used++];
    for (uint32_t i = 0; i < VFS_MAX_NAME; i++)    n->name[i] = 0;
    n->type        = VFS_FILE;
    n->size        = 0;
    n->data        = nullptr;
    n->ops         = nullptr;
    n->child_count = 0;
    n->parent      = nullptr;
    for (uint32_t i = 0; i < VFS_MAX_CHILDREN; i++) n->children[i] = nullptr;
    return n;
}

static int ramfs_read(VfsNode* node, uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!node || node->type != VFS_FILE) return -1;
    if (offset >= node->size) return 0;
    uint32_t avail = node->size - offset;
    if (size > avail) size = avail;
    if (!node->data) return 0;
    for (uint32_t i = 0; i < size; i++) buf[i] = node->data[offset + i];
    return (int)size;
}

static int ramfs_write(VfsNode* node, const uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!node || node->type != VFS_FILE) return -1;
    uint32_t end = offset + size;
    if (end > VFS_MAX_FILE_DATA) {
        if (offset >= VFS_MAX_FILE_DATA) return -1;
        size = VFS_MAX_FILE_DATA - offset;
        end  = VFS_MAX_FILE_DATA;
    }
    if (!node->data) {
        node->data = (uint8_t*)kmalloc(VFS_MAX_FILE_DATA);
        if (!node->data) return -1;
        for (uint32_t i = 0; i < VFS_MAX_FILE_DATA; i++) node->data[i] = 0;
    }
    for (uint32_t i = 0; i < size; i++) node->data[offset + i] = buf[i];
    if (end > node->size) node->size = end;
    return (int)size;
}

static VfsNode* ramfs_finddir(VfsNode* node, const char* name) {
    if (!node || node->type != VFS_DIR) return nullptr;
    for (uint32_t i = 0; i < node->child_count; i++) {
        if (kstrcmp(node->children[i]->name, name) == 0)
            return node->children[i];
    }
    return nullptr;
}

static VfsNode* ramfs_mkdir(VfsNode* node, const char* name) {
    if (!node || node->type != VFS_DIR) return nullptr;
    if (node->child_count >= VFS_MAX_CHILDREN) return nullptr;
    if (ramfs_finddir(node, name)) return nullptr;
    VfsNode* child = alloc_node();
    if (!child) return nullptr;
    kstrcpy(child->name, name);
    child->type   = VFS_DIR;
    child->ops    = node->ops;
    child->parent = node;
    node->children[node->child_count++] = child;
    return child;
}

static VfsNode* ramfs_create(VfsNode* node, const char* name) {
    if (!node || node->type != VFS_DIR) return nullptr;
    if (node->child_count >= VFS_MAX_CHILDREN) return nullptr;
    if (ramfs_finddir(node, name)) return nullptr;
    VfsNode* child = alloc_node();
    if (!child) return nullptr;
    kstrcpy(child->name, name);
    child->type   = VFS_FILE;
    child->ops    = node->ops;
    child->parent = node;
    node->children[node->child_count++] = child;
    return child;
}

static int ramfs_readdir(VfsNode* node, uint32_t index, char* name_out) {
    if (!node || node->type != VFS_DIR) return -1;
    if (index >= node->child_count) return -1;
    kstrcpy(name_out, node->children[index]->name);
    return 0;
}

static VfsOps ramfs_ops = {
    ramfs_read,
    ramfs_write,
    ramfs_finddir,
    ramfs_mkdir,
    ramfs_create,
    ramfs_readdir,
};


static VfsNode* root_node = nullptr;
static VfsNode* cwd_node  = nullptr;

void vfs_init() {
    node_pool_used = 0;
    root_node = alloc_node();
    kstrcpy(root_node->name, "/");
    root_node->type   = VFS_DIR;
    root_node->ops    = &ramfs_ops;
    root_node->parent = root_node;
    cwd_node = root_node;
}

VfsNode* vfs_root() { return root_node; }
VfsNode* vfs_cwd()  { return cwd_node; }

VfsNode* vfs_finddir(VfsNode* node, const char* name) {
    if (!node || !node->ops || !node->ops->finddir) return nullptr;
    return node->ops->finddir(node, name);
}

VfsNode* vfs_mkdir(VfsNode* node, const char* name) {
    if (!node || !node->ops || !node->ops->mkdir) return nullptr;
    return node->ops->mkdir(node, name);
}

VfsNode* vfs_create(VfsNode* node, const char* name) {
    if (!node || !node->ops || !node->ops->create) return nullptr;
    return node->ops->create(node, name);
}

int vfs_read(VfsNode* node, uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, buf, offset, size);
}

int vfs_write(VfsNode* node, const uint8_t* buf, uint32_t offset, uint32_t size) {
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, buf, offset, size);
}

int vfs_readdir(VfsNode* node, uint32_t index, char* name_out) {
    if (!node || !node->ops || !node->ops->readdir) return -1;
    return node->ops->readdir(node, index, name_out);
}

VfsNode* vfs_resolve_path(const char* path) {
    if (!path || !path[0]) return cwd_node;

    VfsNode* cur;
    uint32_t i = 0;

    if (path[0] == '/') {
        cur = root_node;
        i = 1;
    } else {
        cur = cwd_node;
    }

    char component[VFS_MAX_NAME];
    while (path[i]) {
        while (path[i] == '/') i++;
        if (!path[i]) break;

        uint32_t j = 0;
        while (path[i] && path[i] != '/' && j < VFS_MAX_NAME - 1)
            component[j++] = path[i++];
        component[j] = 0;

        if (kstrcmp(component, ".") == 0) continue;
        if (kstrcmp(component, "..") == 0) {
            if (cur->parent && cur->parent != cur)
                cur = cur->parent;
            continue;
        }

        VfsNode* next = vfs_finddir(cur, component);
        if (!next) return nullptr;
        cur = next;
    }
    return cur;
}

void vfs_get_cwd_path(char* buf, uint32_t bufsize) {
    if (!bufsize) return;

    VfsNode* stack[64];
    uint32_t depth = 0;
    VfsNode* n = cwd_node;
    while (n && n != root_node && depth < 63) {
        stack[depth++] = n;
        n = n->parent;
    }

    if (depth == 0) {
        buf[0] = '/';
        buf[1] = 0;
        return;
    }

    uint32_t pos = 0;
    for (int32_t d = (int32_t)depth - 1; d >= 0; d--) {
        if (pos + 1 < bufsize) buf[pos++] = '/';
        const char* nm = stack[d]->name;
        for (uint32_t k = 0; nm[k] && pos + 1 < bufsize; k++)
            buf[pos++] = nm[k];
    }
    buf[pos] = 0;
}

int vfs_chdir(const char* path) {
    VfsNode* target = vfs_resolve_path(path);
    if (!target) return -1;
    if (target->type != VFS_DIR) return -1;
    cwd_node = target;
    return 0;
}