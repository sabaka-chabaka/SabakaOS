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
        kernel/kernel.cpp
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

add_custom_target(run
        COMMAND qemu-system-i386 -kernel sabakaos.bin -m 32M -no-reboot -serial stdio
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT ">>> Запускаем SabakaOS..."
)

add_custom_target(debug
        COMMAND qemu-system-i386 -kernel sabakaos.bin -m 32M -no-reboot -s -S
        DEPENDS sabakaos.bin
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT ">>> QEMU ждёт GDB на :1234..."
)