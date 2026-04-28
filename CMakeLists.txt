cmake_minimum_required(VERSION 3.20)
project(SabakaOS ASM_NASM C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")
enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_FLAGS "")

set(KERNEL_CXX_FLAGS
        -m32
        -march=i686
        -ffreestanding
        -fno-exceptions
        -fno-rtti
        -fno-stack-protector
        -fno-pie
        -fno-use-cxa-atexit
        -fno-threadsafe-statics
        -nostdlib
        -nostdinc++
        -std=c++17
        -O2
        -Wall
        -Wextra
        -Wno-unused-parameter
)
string(JOIN " " KERNEL_CXX_FLAGS_STR ${KERNEL_CXX_FLAGS})
set(CMAKE_CXX_FLAGS "${KERNEL_CXX_FLAGS_STR}")

set(SOURCES
        boot/boot.asm

        kernel/gdt_flush.asm
        kernel/idt_flush.asm
        kernel/tss_flush.asm
        kernel/isr.asm
        kernel/isr128.asm
        kernel/context_switch.asm

        kernel/kernel.cpp
        kernel/gdt.cpp
        kernel/idt.cpp
        kernel/tss.cpp
        kernel/pmm.cpp
        kernel/paging.cpp
        kernel/heap.cpp
        kernel/pit.cpp
        kernel/kstring.cpp
        kernel/osmath.cpp
        kernel/scheduler.cpp
        kernel/syscall.cpp
        kernel/usermode.cpp
        kernel/mutex.cpp
        kernel/pipe.cpp
        kernel/env_vars.cpp
        kernel/elf_loader.cpp

        kernel/terminal.cpp
        kernel/fb.cpp
        kernel/vesa.cpp
        kernel/graphics.cpp

        kernel/ata.cpp
        kernel/fat32.cpp
        kernel/vfs.cpp
        kernel/vfs_disk.cpp

        kernel/net.cpp
        kernel/net_queue.cpp
        kernel/rtl8139.cpp
        kernel/dns.cpp
        kernel/tcp.cpp
        kernel/udp_listen.cpp

        kernel/pci.cpp
        kernel/usb/uhci.cpp
        kernel/usb/hid.cpp

        kernel/keyboard.cpp
        kernel/mouse.cpp

        kernel/shell.cpp
        kernel/shell_disk.cpp
        kernel/shell_exec.cpp
        kernel/shell_net.cpp

        graphics/font.cpp
        graphics/surface.cpp
        graphics/painter.cpp
        graphics/gfx.cpp
)

add_executable(sabakaos.bin ${SOURCES})

target_include_directories(sabakaos.bin PRIVATE
        ${CMAKE_SOURCE_DIR}/kernel
        ${CMAKE_SOURCE_DIR}/graphics
        ${CMAKE_SOURCE_DIR}
)

target_compile_options(sabakaos.bin PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:${KERNEL_CXX_FLAGS}>
)

target_link_options(sabakaos.bin PRIVATE
        -m32
        -ffreestanding
        -nostdlib
        -T${CMAKE_SOURCE_DIR}/linker.ld
        -static
        -Wl,--build-id=none
        -lgcc
)

add_custom_target(iso
        COMMAND ${CMAKE_COMMAND} -E make_directory iso/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy sabakaos.bin iso/boot/sabakaos.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub/grub.cfg iso/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o sabakaos.iso iso 2>&1
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Building ISO..."
)

add_custom_target(run
        COMMAND qemu-system-i386
        -drive file=disk.img,format=raw,if=ide,index=0,media=disk
        -drive file=sabakaos.iso,format=raw,if=ide,index=2,media=cdrom
        -boot order=d
        -m 128M
        -serial stdio
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        -device bochs-display
        -display gtk,grab-on-hover=off
        -usb
        -device usb-tablet
        -device usb-kbd
        -no-reboot
        DEPENDS iso
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running SabakaOS (bochs-display + USB tablet/kbd)..."
)

add_custom_target(run_vga
        COMMAND qemu-system-i386
        -drive file=sabakaos.iso,format=raw,media=cdrom,index=0
        -drive file=disk.img,format=raw,media=disk,index=1,if=ide
        -boot order=d
        -m 128M
        -serial stdio
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        -vga std
        -usb
        -device usb-tablet
        -device usb-kbd
        -no-reboot
        DEPENDS iso
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running SabakaOS (std VGA + USB tablet/kbd)..."
)

add_custom_target(run_novesa
        COMMAND qemu-system-i386
        -kernel sabakaos.bin
        -m 64M
        -serial stdio
        -drive file=disk.img,format=raw,index=0,media=disk
        -netdev user,id=net0
        -device rtl8139,netdev=net0
        -usb
        -device usb-tablet
        -device usb-kbd
        -no-reboot
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running SabakaOS (no VESA)..."
)