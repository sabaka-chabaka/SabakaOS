#include "shell_exec.h"
#include "terminal.h"
#include "kstring.h"
#include "vfs.h"
#include "heap.h"
#include "elf_loader.h"
#include "scheduler.h"
#include "fat32.h"

static void cmd_exec(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_puts("Usage: exec <path>\n");
        terminal_puts("  Example: exec /disk/bin/hello\n");
        return;
    }

    const char* path = args.argv[1];

    VfsNode* node = vfs_resolve_path(path);
    if (!node || node->type != VFS_FILE) {
        terminal_set_color_fg(12);
        terminal_puts("exec: not found: ");
        terminal_puts(path);
        terminal_putchar('\n');
        terminal_reset_color();
        return;
    }

    if (node->data) {
        Fat32Entry* fe = (Fat32Entry*)node->data;
        node->size = fe->size;
    }

    if (node->size == 0) {
        terminal_set_color_fg(12);
        terminal_puts("exec: file is empty\n");
        terminal_reset_color();
        return;
    }

    uint8_t* buf = (uint8_t*)kmalloc(node->size);
    if (!buf) {
        terminal_puts("exec: out of memory\n");
        return;
    }

    int r = vfs_read(node, buf, 0, node->size);
    if (r <= 0) {
        kfree(buf);
        terminal_puts("exec: read error\n");
        return;
    }

    ElfLoadResult elf = elf_load(buf, (uint32_t)r);
    kfree(buf);

    if (!elf.ok) {
        terminal_set_color_fg(12);
        terminal_puts("exec: not a valid ELF32 executable\n");
        terminal_reset_color();
        return;
    }

    const char* name = path;
    for (const char* p = path; *p; p++)
        if (*p == '/') name = p + 1;

    Process* proc = process_create_user(elf.entry, name, 5);
    if (!proc) {
        terminal_set_color_fg(12);
        terminal_puts("exec: failed to create process\n");
        terminal_reset_color();
        return;
    }

    proc->brk_start = elf.load_end;
    proc->brk_curr  = elf.load_end;

    terminal_set_color_fg(10);
    terminal_puts("exec: started '");
    terminal_puts(name);
    terminal_puts("' pid=");
    char buf2[12];
    kuitoa(proc->pid, buf2, 10);
    terminal_puts(buf2);
    terminal_putchar('\n');
    terminal_reset_color();
}

void shell_exec_register() {
    shell_register("exec", "exec <path>  Run ELF binary from disk", cmd_exec);
}