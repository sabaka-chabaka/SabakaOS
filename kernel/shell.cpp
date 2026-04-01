#include "shell.h"

#include <heap.h>
#include <pit.h>
#include <vfs.h>
#include <kstring.h>
#include <pmm.h>
#include <terminal.h>

#include "keyboard.h"
#include "process.h"
#include "scheduler.h"

static const int CMD_MAX = 32;
static ShellCommand cmds[CMD_MAX];
static int cmd_count = 0;

void shell_register(const char *name, const char *help, ShellCmdFn fn) {
    if (cmd_count >= CMD_MAX) return;
    cmds[cmd_count++] = { name, help, fn };
}

static ShellArgs parse(const char* line) {
    ShellArgs args;
    args.argc = 0;
    int i = 0, len = (int)kstrlen(line);

    while (i < len && args.argc < SHELL_MAX_ARGS) {
        while (i < len && line[i] == ' ') i++;
        if (i >= len) break;

        int j = 0;
        bool quoted = (line[i] == '"');
        if (quoted) i++;

        while (i < len && j < SHELL_ARG_LEN-1) {
            if (quoted && line[i] == '"') { i++; break; }
            if (!quoted && line[i] == ' ') break;
            args.argv[args.argc][j++] = line[i++];
        }
        args.argv[args.argc][j] = 0;
        if (j > 0) args.argc++;
    }
    return args;
}

static void cmd_help(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_set_color_fg(11);
        terminal_puts("Available commands (Press any key for next page):\n");
        terminal_reset_color();

        int lines_printed = 0;

        for (int i = 0; i < cmd_count; i++) {
            terminal_set_color_fg(10);
            terminal_puts("  ");
            terminal_puts(cmds[i].name);
            terminal_reset_color();

            int pad = 12 - (int)kstrlen(cmds[i].name);
            while (pad-- > 0) terminal_putchar(' ');

            terminal_set_color_fg(7);
            terminal_puts(cmds[i].help);
            terminal_reset_color();
            terminal_newline();

            lines_printed++;

            if (lines_printed >= HELP_PAGE_SIZE && i < cmd_count - 1) {
                terminal_set_color_fg(14); // Yellow
                terminal_puts("-- More (Any key) --\n");
                terminal_reset_color();

                pit_sleep(1);

                keyboard_wait_key();

                lines_printed = 0;
            }
        }
    } else {
        for (int i = 0; i < cmd_count; i++) {
            if (kstrcmp(cmds[i].name, args.argv[1]) == 0) {
                terminal_set_color_fg(11);
                terminal_puts("\nHelp for ");
                terminal_set_color_fg(10);
                terminal_puts(cmds[i].name);
                terminal_reset_color();
                terminal_puts(": ");
                terminal_reset_color();
                terminal_puts(cmds[i].help);
                terminal_newline();
                return;
            }
        }
    }
}

static void cmd_clear(const ShellArgs&) {
    terminal_clear();
}

static void cmd_echo(const ShellArgs& args) {
    for (int i = 1; i < args.argc; i++) {
        if (i > 1) terminal_putchar(' ');
        terminal_puts(args.argv[i]);
    }
    terminal_newline();
}

static void cmd_mem(const ShellArgs&) {
    uint32_t phys_free  = pmm_free_pages() * 4;
    uint32_t phys_used  = pmm_used_pages() * 4;
    uint32_t phys_total = (phys_free + phys_used);
    uint32_t hfree      = heap_free()  / 1024;
    uint32_t hused      = heap_used()  / 1024;
    uint32_t htotal     = heap_total() / 1024;

    terminal_set_color_fg(14); // YELLOW
    terminal_puts("Memory information:\n");
    terminal_reset_color();

    terminal_puts("  Physical  total: "); terminal_put_uint(phys_total); terminal_puts(" KB\n");
    terminal_set_color_fg(10);
    terminal_puts("  Physical  free:  "); terminal_put_uint(phys_free);  terminal_puts(" KB\n");
    terminal_reset_color();
    terminal_set_color_fg(12);
    terminal_puts("  Physical  used:  "); terminal_put_uint(phys_used);  terminal_puts(" KB\n");
    terminal_reset_color();

    terminal_puts("  Heap      total: "); terminal_put_uint(htotal); terminal_puts(" KB\n");
    terminal_set_color_fg(10);
    terminal_puts("  Heap      free:  "); terminal_put_uint(hfree);  terminal_puts(" KB\n");
    terminal_reset_color();
    terminal_set_color_fg(12);
    terminal_puts("  Heap      used:  "); terminal_put_uint(hused);  terminal_puts(" KB\n");
    terminal_reset_color();
}

static void cmd_version(const ShellArgs&) {
    terminal_set_color_fg(11);
    terminal_puts("SabakaOS v0.0.7\n");
    terminal_reset_color();
    terminal_puts("  Arch:    x86 32-bit Protected Mode\n");
    terminal_puts("  Display: VGA text 80x25\n");
    terminal_puts("  Memory:  PMM + Paging + Heap\n");
    terminal_puts("  Input:   PS/2 Keyboard IRQ1\n");
    terminal_puts("  Shell:   SabakaShell v1.0\n");
}

static void cmd_uptime(const ShellArgs&) {
    uint32_t ms  = pit_uptime_ms();
    uint32_t sec = ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hr  = min / 60;
    sec %= 60; min %= 60;

    terminal_set_color_fg(14);
    terminal_puts("Uptime: ");
    terminal_reset_color();
    terminal_put_uint(hr);  terminal_puts("h ");
    terminal_put_uint(min); terminal_puts("m ");
    terminal_put_uint(sec); terminal_puts("s (");
    terminal_put_uint(ms);  terminal_puts(" ms)\n");
}

static void cmd_sleep(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_puts("Usage: sleep <milliseconds>\n");
        return;
    }
    uint32_t ms = (uint32_t)katoi(args.argv[1]);
    pit_sleep_ms(ms);
}

static void cmd_ls(const ShellArgs& args) {
    VfsNode* dir = (args.argc >= 2) ? vfs_resolve_path(args.argv[1]) : vfs_cwd();
    if (!dir) { terminal_puts("ls: no such directory\n"); return; }
    if (dir->type != VFS_DIR) { terminal_puts("ls: not a directory\n"); return; }
    char name[VFS_MAX_NAME];
    for (uint32_t i = 0; vfs_readdir(dir, i, name) == 0; i++) {
        VfsNode* child = vfs_finddir(dir, name);
        if (child && child->type == VFS_DIR) {
            terminal_set_color_fg(14);
            terminal_puts(name);
            terminal_puts("/");
            terminal_reset_color();
        } else {
            terminal_set_color_fg(15);
            terminal_puts(name);
            terminal_reset_color();
        }
        terminal_putchar('\n');
    }
}

static void cmd_pwd(const ShellArgs&) {
    char buf[256];
    vfs_get_cwd_path(buf, sizeof(buf));
    terminal_puts(buf);
    terminal_putchar('\n');
}

static void cmd_cd(const ShellArgs& args) {
    const char* path = (args.argc >= 2) ? args.argv[1] : "/";
    if (vfs_chdir(path) != 0) {
        terminal_puts("cd: no such directory\n");
    } else {
        char buf[256];
        vfs_get_cwd_path(buf, sizeof(buf));
        terminal_set_prompt_path(buf);
    }
}

static void cmd_mkdir(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: mkdir <name>\n"); return; }
    VfsNode* node = vfs_mkdir(vfs_cwd(), args.argv[1]);
    if (!node) terminal_puts("mkdir: failed (exists or no space)\n");
}

static void cmd_touch(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: touch <name>\n"); return; }
    VfsNode* node = vfs_create(vfs_cwd(), args.argv[1]);
    if (!node) terminal_puts("touch: failed (exists or no space)\n");
}

static void cmd_cat(const ShellArgs& args) {
    if (args.argc < 2) { terminal_puts("Usage: cat <path>\n"); return; }
    VfsNode* node = vfs_resolve_path(args.argv[1]);
    if (!node) { terminal_puts("cat: no such file\n"); return; }
    if (node->type != VFS_FILE) { terminal_puts("cat: not a file\n"); return; }
    if (node->size == 0) return;
    uint8_t buf[VFS_MAX_FILE_DATA + 1];
    int n = vfs_read(node, buf, 0, node->size);
    if (n > 0) {
        buf[n] = 0;
        terminal_puts((char*)buf);
        if (buf[n-1] != '\n') terminal_putchar('\n');
    }
}

static void cmd_write(const ShellArgs& args) {
    if (args.argc < 3) { terminal_puts("Usage: write <path> <text>\n"); return; }
    VfsNode* node = vfs_resolve_path(args.argv[1]);
    if (!node) {
        node = vfs_create(vfs_cwd(), args.argv[1]);
        if (!node) { terminal_puts("write: cannot create file\n"); return; }
    }
    if (node->type != VFS_FILE) { terminal_puts("write: not a file\n"); return; }
    char text[VFS_MAX_FILE_DATA];
    uint32_t tlen = 0;
    for (int i = 2; i < args.argc && tlen < VFS_MAX_FILE_DATA - 1; i++) {
        if (i > 2 && tlen < VFS_MAX_FILE_DATA - 1) text[tlen++] = ' ';
        const char* w = args.argv[i];
        while (*w && tlen < VFS_MAX_FILE_DATA - 1) text[tlen++] = *w++;
    }
    text[tlen] = 0;
    vfs_write(node, (const uint8_t*)text, 0, tlen);
    vfs_write(node, (const uint8_t*)"\n", tlen, 1);
}

static void cmd_reboot(const ShellArgs&) {
    terminal_set_color_fg(12);
    terminal_puts("Rebooting...\n");
    terminal_reset_color();
    __asm__ volatile(
        "1: inb $0x64, %%al\n"
        "   testb $0x02, %%al\n"
        "   jnz 1b\n"
        "   movb $0xFE, %%al\n"
        "   outb %%al, $0x64\n"
        ::: "eax"
    );
}

static void cmd_halt(const ShellArgs&) {
    terminal_set_color_fg(12);
    terminal_puts("System halted. Goodbye.\n");
    terminal_reset_color();
    __asm__ volatile("cli; hlt");
}

static void cmd_hexdump(const ShellArgs& args) {
    if (args.argc < 3) {
        terminal_puts("Usage: hexdump <addr_hex> <size>\n");
        return;
    }
    const char* s = args.argv[1];
    if (s[0]=='0' && (s[1]=='x'||s[1]=='X')) s += 2;
    uint32_t addr = 0;
    while (*s) {
        addr <<= 4;
        if (*s>='0'&&*s<='9') addr += *s-'0';
        else if (*s>='a'&&*s<='f') addr += *s-'a'+10;
        else if (*s>='A'&&*s<='F') addr += *s-'A'+10;
        s++;
    }
    uint32_t size = (uint32_t)katoi(args.argv[2]);
    if (size > 256) size = 256;

    uint8_t* ptr = (uint8_t*)addr;
    for (uint32_t i = 0; i < size; i += 16) {
        terminal_set_color_fg(8);
        char buf[12]; kuitoa(addr+i, buf, 16);
        int pad = 8 - (int)kstrlen(buf);
        while (pad-- > 0) terminal_putchar('0');
        terminal_puts(buf);
        terminal_puts(":  ");
        terminal_reset_color();

        for (uint32_t j = 0; j < 16 && i+j < size; j++) {
            char hb[4]; kuitoa(ptr[i+j], hb, 16);
            if (kstrlen(hb) < 2) terminal_putchar('0');
            terminal_puts(hb);
            terminal_putchar(' ');
        }
        terminal_puts(" |");
        terminal_set_color_fg(10);
        for (uint32_t j = 0; j < 16 && i+j < size; j++) {
            char ch = (char)ptr[i+j];
            terminal_putchar(ch >= 32 && ch < 127 ? ch : '.');
        }
        terminal_reset_color();
        terminal_puts("|\n");
    }
}

static const char* proc_state_str(ProcessState s) {
    switch (s) {
        case PROC_RUNNING: return "RUN  ";
        case PROC_READY:   return "READY";
        case PROC_BLOCKED: return "BLOCK";
        case PROC_SLEEP:   return "SLEEP";
        case PROC_DEAD:    return "DEAD ";
        default:           return "?????";
    }
}

static void cmd_ps(const ShellArgs&) {
    terminal_set_color_fg(14);
    terminal_puts("PID  STATE  PRI  TICKS     NAME\n");
    terminal_puts("---  -----  ---  --------  --------\n");
    terminal_reset_color();

    for (int i = 0; i < PROC_MAX; i++) {
        Process* p = scheduler_get(i);
        if (!p || p->state == PROC_DEAD) continue;

        char buf[12];
        kuitoa(p->pid, buf, 10);
        int pad = 4 - (int)kstrlen(buf);
        while(pad-- > 0) terminal_putchar(' ');
        terminal_puts(buf);
        terminal_putchar(' ');

        if (p->state == PROC_RUNNING) terminal_set_color_fg(10);
        else terminal_set_color_fg(7);
        terminal_puts(proc_state_str(p->state));
        terminal_reset_color();
        terminal_puts("  ");

        kuitoa(p->priority, buf, 10);
        pad = 3 - (int)kstrlen(buf);
        while(pad-- > 0) terminal_putchar(' ');
        terminal_puts(buf);
        terminal_puts("  ");

        kuitoa(p->ticks_total, buf, 10);
        pad = 8 - (int)kstrlen(buf);
        while(pad-- > 0) terminal_putchar(' ');
        terminal_puts(buf);
        terminal_puts("  ");

        terminal_set_color_fg(11);
        terminal_puts(p->name);
        terminal_reset_color();
        terminal_newline();
    }
}

static void cmd_kill(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_puts("Usage: kill <pid>\n");
        return;
    }
    uint32_t pid = (uint32_t)katoi(args.argv[1]);
    if (pid == 0) {
        terminal_set_color_fg(12);
        terminal_puts("Cannot kill kernel process (pid 0)\n");
        terminal_reset_color();
        return;
    }
    for (int i = 0; i < PROC_MAX; i++) {
        Process* p = scheduler_get(i);
        if (p && p->pid == pid && p->state != PROC_DEAD) {
            p->state = PROC_DEAD;
            terminal_set_color_fg(10);
            terminal_puts("Killed process ");
            terminal_puts(p->name);
            terminal_putchar('\n');
            terminal_reset_color();
            return;
        }
    }
    terminal_set_color_fg(12);
    terminal_puts("No process with pid ");
    char buf[12]; kuitoa(pid, buf, 10);
    terminal_puts(buf);
    terminal_putchar('\n');
    terminal_reset_color();
}

static void cmd_spawn(const ShellArgs& args) {
    if (args.argc < 2) {
        terminal_puts("Usage: spawn <name>\n");
        terminal_puts("  Spawns a test counter process\n");
        return;
    }
    Process* p = process_create(
        [](void*) {
            uint32_t n = 0;
            while(true) { n++; process_sleep(1000); }
        },
        nullptr,
        args.argv[1],
        5
    );
    if (p) {
        terminal_set_color_fg(10);
        terminal_puts("Spawned: ");
        terminal_puts(p->name);
        terminal_puts(" (pid ");
        char buf[12]; kuitoa(p->pid, buf, 10);
        terminal_puts(buf);
        terminal_puts(")\n");
        terminal_reset_color();
    } else {
        terminal_set_color_fg(12);
        terminal_puts("Failed to spawn process\n");
        terminal_reset_color();
    }
}

static void cmd_dog(const ShellArgs&) {
    terminal_puts(R"(
      __      _
    o'')}____//
     `_/      )
     (_(_/-(_/
)");
}

static void cmd_dogsay(const ShellArgs& args) {
    const char* message = (args.argc > 1) ? args.argv[1] : "Woof!";

    uint32_t len = kstrlen(message);

    terminal_puts(" ");
    for (int i = 0; i < len + 2; i++) terminal_puts("_");
    terminal_puts("\n");

    terminal_puts("< ");
    terminal_puts(message);
    terminal_puts(" >\n");

    terminal_puts(" ");
    for (int i = 0; i < len + 2; i++) terminal_puts(" ");
    terminal_puts(" ");

    terminal_puts(R"(
      __      _
    o'')}____//
     `_/      )
     (_(_/-(_/
)");
}

void shell_init() {
    shell_register("help",    "Show this help",            cmd_help);
    shell_register("clear",   "Clear terminal",            cmd_clear);
    shell_register("echo",    "Print arguments",           cmd_echo);
    shell_register("mem",     "Memory statistics",         cmd_mem);
    shell_register("version", "OS version info",           cmd_version);
    shell_register("ls",      "List directory",            cmd_ls);
    shell_register("pwd",     "Print working directory",   cmd_pwd);
    shell_register("cd",      "Change directory",          cmd_cd);
    shell_register("mkdir",   "Create directory",          cmd_mkdir);
    shell_register("touch",   "Create empty file",         cmd_touch);
    shell_register("cat",     "Print file contents",       cmd_cat);
    shell_register("write",   "write <file> <text>",       cmd_write);
    shell_register("uptime",  "Show system uptime",        cmd_uptime);
    shell_register("sleep",   "sleep <ms>",                cmd_sleep);
    shell_register("reboot",  "Reboot the system",         cmd_reboot);
    shell_register("halt",    "Halt the system",           cmd_halt);
    shell_register("hexdump", "hexdump <addr> <size>",     cmd_hexdump);
    shell_register("spawn",   "spawn <name>",              cmd_spawn);
    shell_register("kill",    "kill <pid>",                cmd_kill);
    shell_register("ps",      "List processes",            cmd_ps);
    shell_register("dog",     "Shows a dog",               cmd_dog);
    shell_register("dogsay",  "dogsay <message>",          cmd_dogsay);
}

void shell_execute(const char* line) {
    uint32_t i = 0;
    while (line[i] == ' ') i++;
    if (!line[i]) return;

    ShellArgs args = parse(line);
    if (args.argc == 0) return;

    for (int j = 0; j < cmd_count; j++) {
        if (kstrcmp(cmds[j].name, args.argv[0]) == 0) {
            cmds[j].fn(args);
            return;
        }
    }

    terminal_set_color_fg(12);
    terminal_puts("Unknown command: ");
    terminal_puts(args.argv[0]);
    terminal_puts("  (type 'help' for list)\n");
    terminal_reset_color();
}