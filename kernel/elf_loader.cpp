#include "elf_loader.h"
#include "paging.h"
#include "kstring.h"

static uint32_t align_down(uint32_t v, uint32_t a) { return v & ~(a - 1); }
static uint32_t align_up  (uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

ElfLoadResult elf_load(const uint8_t* data, uint32_t size) {
    ElfLoadResult res = { false, 0, 0xFFFFFFFF, 0 };

    if (size < sizeof(Elf32Header)) return res;

    const Elf32Header* eh = (const Elf32Header*)data;

    uint32_t magic;
    kmemcpy(&magic, eh->e_ident, 4);
    if (magic != ELF_MAGIC)    return res;
    if (eh->e_type    != ET_EXEC) return res;
    if (eh->e_machine != EM_386)  return res;
    if (eh->e_phnum   == 0)       return res;

    if (eh->e_phoff + (uint32_t)eh->e_phnum * sizeof(Elf32Phdr) > size)
        return res;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf32Phdr* ph = (const Elf32Phdr*)(data + eh->e_phoff
                                                  + i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0)      continue;

        uint32_t seg_start = align_down(ph->p_vaddr, 4096);
        uint32_t seg_end   = align_up(ph->p_vaddr + ph->p_memsz, 4096);

        if (seg_start < res.load_base) res.load_base = seg_start;
        if (seg_end   > res.load_end)  res.load_end  = seg_end;
    }

    if (res.load_base == 0xFFFFFFFF) return res;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf32Phdr* ph = (const Elf32Phdr*)(data + eh->e_phoff
                                                  + i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_memsz == 0)      continue;

        uint32_t vaddr     = ph->p_vaddr;
        uint32_t page_base = align_down(vaddr, 4096);
        uint32_t page_end  = align_up(vaddr + ph->p_memsz, 4096);
        uint32_t map_size  = page_end - page_base;

        uint32_t flags = PAGE_PRESENT | PAGE_USER;
        if (ph->p_flags & PF_W) flags |= PAGE_WRITE;

        if (!paging_alloc_region(page_base, map_size, flags))
            return res;

        if (ph->p_filesz > 0) {
            if (ph->p_offset + ph->p_filesz > size) return res;
            kmemcpy((void*)vaddr, data + ph->p_offset, ph->p_filesz);
        }

        if (ph->p_memsz > ph->p_filesz) {
            uint32_t bss_start = vaddr + ph->p_filesz;
            uint32_t bss_size  = ph->p_memsz - ph->p_filesz;
            kmemset((void*)bss_start, 0, bss_size);
        }
    }

    res.ok    = true;
    res.entry = eh->e_entry;
    return res;
}