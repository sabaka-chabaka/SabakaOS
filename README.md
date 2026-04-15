# SabakaOS

SabakaOS is a bare-metal x86 32-bit hobby operating system written in C++ and NASM Assembly, built from scratch without any standard library or OS support.

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
cd SabakaOS
cmake ..
```

### Build the kernel binary

```bash
make
```

### Run in QEMU

```bash
make run
```

---
## License

MIT License — Copyright (c) 2026 sabaka-chabaka. See [LICENSE.md](LICENSE.md) for full text.
