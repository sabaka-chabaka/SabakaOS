#include "shell.h"

#include <heap.h>
#include <pit.h>
#include <kstring.h>
#include <pmm.h>
#include <terminal.h>

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

static void cmd_help(const ShellArgs&) {
    terminal_set_color_fg(11);
    terminal_puts("Available commands:\n");
    terminal_reset_color();
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

void shell_init() {
    shell_register("help",    "Show this help",           cmd_help);
    shell_register("clear",   "Clear terminal",           cmd_clear);
    shell_register("echo",    "Print arguments",          cmd_echo);
    shell_register("mem",     "Memory statistics",        cmd_mem);
    shell_register("version", "OS version info",          cmd_version);
    shell_register("uptime",  "Show system uptime",        cmd_uptime);
    shell_register("sleep",   "sleep <ms>",                cmd_sleep);
    shell_register("reboot",  "Reboot the system",        cmd_reboot);
    shell_register("halt",    "Halt the system",          cmd_halt);
    shell_register("hexdump", "hexdump <addr> <size>",    cmd_hexdump);
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