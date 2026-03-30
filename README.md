# SabakaOS

SabakaOS is a bare-metal x86 32-bit hobby operating system written in C++ and NASM Assembly, built from scratch without any standard library or OS support.

---

## Overview

- **Architecture:** x86 32-bit Protected Mode
- **Display:** VGA text mode 80×25
- **Memory:** Physical Memory Manager (PMM) + Paging + Heap
- **Input:** PS/2 Keyboard via IRQ1
- **Filesystem:** In-memory Virtual Filesystem (VFS)
- **Shell:** SabakaShell with command history and keyboard arrow navigation
- **Timer:** PIT (Programmable Interval Timer) at 1000 Hz

---

## Requirements

| Tool | Purpose |
|---|---|
| `nasm` | Assembles `.asm` boot and kernel stubs |
| `gcc` / `g++` (multilib) | Compiles 32-bit freestanding C++ kernel |
| `ld` | Links the kernel ELF binary |
| `cmake` ≥ 3.20 | Build system |
| `make` | Build backend |
| `qemu-system-i386` | Runs and debugs the OS |
| `gdb` (optional) | Remote debugging via QEMU GDB stub |

On Ubuntu/Debian, install dependencies with:

```bash
sudo apt install nasm gcc g++ gcc-multilib g++-multilib cmake make qemu-system-x86
```

---

## Build & Run

### Configure

```bash
cmake -B build
```

### Build the kernel binary

```bash
cmake --build build --target sabakaos.bin
```

### Run in QEMU

```bash
cmake --build build --target run
```

This launches `qemu-system-i386 -kernel sabakaos.bin -m 32M -serial stdio`.

### Debug (GDB remote)

```bash
cmake --build build --target debug
```

QEMU will pause at startup and wait for a GDB connection on `localhost:1234`:

```bash
gdb build/sabakaos.bin
(gdb) target remote :1234
```

---

## Shell Commands

Once booted, the interactive **SabakaShell** is available. Use ↑/↓ arrow keys to navigate command history.

| Command | Description |
|---|---|
| `help` | Show all available commands |
| `clear` | Clear the terminal |
| `echo <text>` | Print arguments to terminal |
| `version` | Show OS version and system info |
| `uptime` | Show system uptime (h/m/s/ms) |
| `sleep <ms>` | Sleep for given milliseconds |
| `mem` | Physical memory and heap statistics |
| `hexdump <addr> <size>` | Hex dump up to 256 bytes from memory address |
| `ls [path]` | List directory contents |
| `pwd` | Print current working directory |
| `cd <path>` | Change directory |
| `mkdir <name>` | Create a directory |
| `touch <name>` | Create an empty file |
| `cat <path>` | Print file contents |
| `write <path> <text>` | Write text to a file (creates if missing) |
| `reboot` | Reboot the system |
| `halt` | Halt the system |

---

## Project Structure

```
SabakaOS/
├── boot/
│   └── boot.asm          # Multiboot entry point, sets up stack, calls kernel_main
├── kernel/
│   ├── kernel.cpp        # kernel_main() — init sequence
│   ├── gdt.cpp / gdt.h   # Global Descriptor Table
│   ├── gdt_flush.asm     # GDT load stub
│   ├── idt.cpp / idt.h   # Interrupt Descriptor Table
│   ├── idt_flush.asm     # IDT load stub
│   ├── isr.asm           # Interrupt Service Routines (assembly stubs)
│   ├── pmm.cpp / pmm.h   # Physical Memory Manager
│   ├── paging.cpp / .h   # Paging (virtual memory)
│   ├── heap.cpp / .h     # Kernel heap allocator
│   ├── pit.cpp / .h      # PIT timer driver
│   ├── keyboard.cpp / .h # PS/2 keyboard driver
│   ├── terminal.cpp / .h # VGA text terminal
│   ├── vfs.cpp / .h      # Virtual Filesystem
│   ├── shell.cpp / .h    # SabakaShell
│   └── kstring.cpp / .h  # Kernel string utilities
├── linker.ld             # Linker script (ELF32 memory layout)
├── CMakeLists.txt        # Build configuration
├── LICENSE.md            # MIT License
└── TODOFILE.md           # Development notes
```

---

## Environment Variables

> **TODO:** No environment variables are currently used. Document any that are added in the future.

---

## Tests

> **TODO:** No automated test suite exists yet. Add unit tests or integration tests as the project matures.

---

## License

MIT License — Copyright (c) 2026 sabaka-chabaka. See [LICENSE.md](LICENSE.md) for full text.
