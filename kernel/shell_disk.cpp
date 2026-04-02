#include "shell_disk.h"
#include "vfs.h"
#include "vfs_disk.h"
#include "fat32.h"
#include "ata.h"
#include "terminal.h"
#include "kstring.h"
#include "heap.h"
#include "shell.h"

static void print_err(const char* msg) {
    terminal_set_color_fg(12);
    terminal_puts(msg);
    terminal_puts("\n");
    terminal_reset_color();
}

static void print_ok(const char* msg) {
    terminal_set_color_fg(10);
    terminal_puts(msg);
    terminal_puts("\n");
    terminal_reset_color();
}

static const char* path_basename(const char* path) {
    const char* last = path;
    for (const char* p = path; *p; p++)
        if (*p == '/') last = p + 1;
    return last;
}

static void cmd_cp(const ShellArgs& args) {
    if (args.argc < 3) {
        print_err("Usage: cp <src> <dst>");
        return;
    }

    const char* src_path = args.argv[1];
    const char* dst_path = args.argv[2];

    VfsNode* src = vfs_resolve_path(src_path);
    if (!src) { print_err("cp: source not found"); return; }
    if (src->type == VFS_DIR) { print_err("cp: source is a directory"); return; }

    VfsNode* dst_dir_node = vfs_resolve_path(dst_path);
    const char* new_name  = path_basename(src_path);
    VfsNode* dst_parent   = nullptr;

    if (dst_dir_node && dst_dir_node->type == VFS_DIR) {
        dst_parent = dst_dir_node;
    } else {
        char parent_buf[256];
        kstrcpy(parent_buf, dst_path);
        const char* base = path_basename(dst_path);
        uint32_t parent_len = (uint32_t)(base - dst_path);
        if (parent_len == 0) {
            dst_parent = vfs_cwd();
        } else {
            char p[256];
            kstrncpy(p, dst_path, parent_len);
            p[parent_len] = 0;
            dst_parent = vfs_resolve_path(p);
        }
        new_name = base;
    }

    if (!dst_parent || dst_parent->type != VFS_DIR) {
        print_err("cp: destination directory not found");
        return;
    }

    VfsNode* dst = vfs_create(dst_parent, new_name);
    if (!dst) { print_err("cp: cannot create destination file"); return; }

    const uint32_t BUF_SIZE = 512;
    uint8_t* buf = (uint8_t*)kmalloc(BUF_SIZE);
    if (!buf) { print_err("cp: out of memory"); return; }

    uint32_t offset = 0;
    while (true) {
        int n = vfs_read(src, buf, offset, BUF_SIZE);
        if (n <= 0) break;
        int w = vfs_write(dst, buf, offset, (uint32_t)n);
        if (w != n) { print_err("cp: write error"); break; }
        offset += (uint32_t)n;
    }
    kfree(buf);
    dst->size = src->size;
    print_ok("cp: done");
}

static void cmd_rm(const ShellArgs& args) {
    if (args.argc < 2) {
        print_err("Usage: rm <path>  (или rm -r <dir> для директорий)");
        return;
    }

    bool recursive = false;
    const char* target_path = args.argv[1];
    if (kstrcmp(args.argv[1], "-r") == 0) {
        if (args.argc < 3) { print_err("rm -r: укажи директорию"); return; }
        recursive = true;
        target_path = args.argv[2];
    }

    VfsNode* node = vfs_resolve_path(target_path);
    if (!node) { print_err("rm: not found"); return; }

    if (node->type == VFS_DIR) {
        if (!recursive) {
            print_err("rm: это директория. Используй rm -r");
            return;
        }

        char name_buf[VFS_MAX_NAME];
        for (uint32_t i = 0; ; i++) {
            if (vfs_readdir(node, i, name_buf) != 0) break;
            VfsNode* child = vfs_finddir(node, name_buf);
            if (!child) continue;
            if (child->type == VFS_FILE) {
                Fat32Entry* e = (Fat32Entry*)child->data;
                if (e) fat32_unlink(e);
            }
        }
        Fat32Entry* dir_e = (Fat32Entry*)node->data;
        if (dir_e) {
            int r = fat32_rmdir(dir_e);
            if (r != FAT32_OK) { print_err("rm -r: directory not empty or error"); return; }
        }
        print_ok("rm: removed directory");
    } else {
        // Файл
        Fat32Entry* e = (Fat32Entry*)node->data;
        if (e) {
            int r = fat32_unlink(e);
            if (r != FAT32_OK) { print_err("rm: error deleting file"); return; }
        }
        print_ok("rm: removed");
    }
}

static void cmd_mv(const ShellArgs& args) {
    if (args.argc < 3) {
        print_err("Usage: mv <src> <dst>");
        return;
    }

    const char* src_path = args.argv[1];
    const char* dst_path = args.argv[2];

    VfsNode* src = vfs_resolve_path(src_path);
    if (!src) { print_err("mv: source not found"); return; }

    Fat32Entry* src_e = (Fat32Entry*)src->data;
    if (!src_e) {
        print_err("mv: только disk-файлы можно перемещать");
        return;
    }

    VfsNode* dst_node = vfs_resolve_path(dst_path);
    uint32_t new_dir_cluster = 0;
    const char* new_name     = nullptr;

    if (dst_node && dst_node->type == VFS_DIR) {
        Fat32Entry* dst_e = (Fat32Entry*)dst_node->data;
        new_dir_cluster = dst_e ? dst_e->cluster : fat32_root_cluster();
        new_name        = path_basename(src_path);
    } else {
        const char* base = path_basename(dst_path);
        uint32_t plen = (uint32_t)(base - dst_path);
        if (plen == 0) {
            new_dir_cluster = src_e->dir_cluster;
        } else {
            char ppath[256];
            kstrncpy(ppath, dst_path, plen);
            ppath[plen] = 0;
            VfsNode* parent = vfs_resolve_path(ppath);
            if (!parent || parent->type != VFS_DIR) {
                print_err("mv: destination directory not found");
                return;
            }
            Fat32Entry* pe = (Fat32Entry*)parent->data;
            new_dir_cluster = pe ? pe->cluster : fat32_root_cluster();
        }
        new_name = base;
    }

    int r = fat32_rename(src_e, new_dir_cluster, new_name);
    if (r != FAT32_OK) {
        print_err("mv: error");
    } else {
        kstrcpy(src->name, new_name);
        print_ok("mv: done");
    }
}

static void cmd_disk(const ShellArgs&) {
    char buf[32];

    if (!ata_is_present()) {
        print_err("disk: ATA disk not found");
        return;
    }

    terminal_set_color_fg(11);
    terminal_puts("=== ATA Disk ===\n");
    terminal_reset_color();

    terminal_puts("Status:  ");
    terminal_puts(ata_is_present() ? "present\n" : "not found\n");

    terminal_puts("Sectors: ");
    kuitoa(ata_sectors_count(), buf, 10);
    terminal_puts(buf);
    terminal_puts("\n");

    terminal_puts("Size:    ");
    kuitoa(ata_sectors_count() / 2, buf, 10);
    terminal_puts(buf);
    terminal_puts(" KB (");
    kuitoa(ata_sectors_count() / 2048, buf, 10);
    terminal_puts(buf);
    terminal_puts(" MB)\n");

    terminal_puts("FAT32:   ");
    terminal_puts(fat32_is_mounted() ? "mounted at /disk\n" : "not mounted\n");
    terminal_puts("VFS:     ");
    terminal_puts(vfs_fat32_mounted() ? "disk mounted in VFS\n" : "not in VFS\n");
}

void shell_disk_register() {
    shell_register("cp",   "cp <src> <dst>       Copy file",           cmd_cp);
    shell_register("rm",   "rm [-r] <path>       Remove file/dir",     cmd_rm);
    shell_register("mv",   "mv <src> <dst>       Move/rename file",    cmd_mv);
    shell_register("disk", "disk                 Show disk info",      cmd_disk);
}