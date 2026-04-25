cmake_minimum_required(VERSION 3.20)
project(SabakaOS ASM_NASM C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")
enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_FLAGS "")

set(SOURCES
        boot/boot.asm
        kernel/gdt_flush.asm
        kernel/gdt.cpp
        kernel/idt_flush.asm
        kernel/isr.asm
        kernel/idt.cpp
        kernel/keyboard.cpp
        kernel/pmm.cpp
        kernel/paging.cpp
        kernel/heap.cpp
        kernel/kstring.cpp
        kernel/osmath.cpp
        kernel/terminal.cpp
        kernel/syscall.cpp
        kernel/mutex.cpp
        kernel/pipe.cpp
        kernel/isr128.asm
        kernel/pit.cpp
        kernel/vfs.cpp
        kernel/shell.cpp
        kernel/tss_flush.asm
        kernel/tss.cpp
        kernel/context_switch.asm
        kernel/usermode.cpp
        kernel/scheduler.cpp
        kernel/kernel.cpp
        kernel/ata.cpp
        kernel/net_queue.cpp
        kernel/tcp.cpp
        kernel/dns.cpp
        kernel/udp_listen.cpp
        kernel/fat32.cpp
        kernel/vfs_disk.cpp
        kernel/shell_disk.cpp
        kernel/shell_exec.cpp
        kernel/elf_loader.cpp
        kernel/rtl8139.cpp
        kernel/net.cpp
        kernel/shell_net.cpp
        kernel/env_vars.cpp
        kernel/fb.cpp
        graphics/font.cpp
        graphics/surface.cpp
        graphics/painter.cpp
        graphics/gfx.cpp
)

add_executable(sabakaos.bin ${SOURCES})
target_include_directories(sabakaos.bin PRIVATE kernel)

target_compile_options(sabakaos.bin PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:
        -m32 -ffreestanding -fno-exceptions -fno-rtti
        -fno-stack-protector -fno-pie -nostdlib -nostdinc++
        -O2 -Wall -Wextra
        >
)

target_link_options(sabakaos.bin PRIVATE
        -m32 -ffreestanding -nostdlib
        -T${CMAKE_SOURCE_DIR}/linker.ld
        -static -Wl,--build-id=none
)

add_custom_target(iso
        COMMAND ${CMAKE_COMMAND} -E make_directory iso/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy sabakaos.bin iso/boot/sabakaos.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub/grub.cfg iso/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o sabakaos.iso iso 2>&1
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT ">>> Собираем ISO с GRUB..."
)

add_custom_target(run
        COMMAND qemu-system-i386
        -drive file=disk.img,format=raw,if=ide,index=0,media=disk
        -drive file=sabakaos.iso,format=raw,if=ide,index=2,media=cdrom
        -boot order=d
        -m 64M
        -serial stdio
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        -device bochs-display
        DEPENDS iso
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT ">>> Running SabakaOS (VESA via bochs-display)..."
)

add_custom_target(run_vga
        COMMAND qemu-system-i386
        -drive file=sabakaos.iso,format=raw,media=cdrom,index=0
        -drive file=disk.img,format=raw,media=disk,index=1,if=ide
        -boot order=d
        -m 64M
        -serial stdio
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        -vga std
        DEPENDS iso
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
         COMMENT ">>> Running SabakaOS (std VGA)..."
)

add_custom_target(run_novesa
        COMMAND qemu-system-i386
        -kernel sabakaos.bin
        -m 32M
        -serial stdio
        -drive file=disk.img,format=raw,index=0,media=disk
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT ">>> Running without VESA (VGA text fallback)..."
)