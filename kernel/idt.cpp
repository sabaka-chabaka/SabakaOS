#include "idt.h"
#include "keyboard.h"
#include "pit.h"
#include "scheduler.h"
#include "syscall.h"

static IDTEntry   idt[256];
static IDTPointer idt_ptr;

static const char* exception_names[] = {
    "Division By Zero",    "Debug",               "NMI",
    "Breakpoint",          "Overflow",            "Bound Range Exceeded",
    "Invalid Opcode",      "Device Not Available","Double Fault",
    "Coprocessor Overrun", "Invalid TSS",         "Segment Not Present",
    "Stack Fault",         "General Protection",  "Page Fault",
    "Reserved",            "FPU Error",           "Alignment Check",
    "Machine Check",       "SIMD FP Exception",   "Virtualization",
    "Reserved","Reserved","Reserved","Reserved","Reserved",
    "Reserved","Reserved","Reserved","Reserved",
    "Security Exception",  "Reserved"
};

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}
static inline void io_wait() { outb(0x80, 0); }

static void pic_remap() {
    uint8_t m1 = inb(0x21), m2 = inb(0xA1);
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, m1); outb(0xA1, m2);
}

void idt_init() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt;
    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0, 0);
    pic_remap();

    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(32, (uint32_t)irq0,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(33, (uint32_t)irq1,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(34, (uint32_t)irq2,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(35, (uint32_t)irq3,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(36, (uint32_t)irq4,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(37, (uint32_t)irq5,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(38, (uint32_t)irq6,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(39, (uint32_t)irq7,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(40, (uint32_t)irq8,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(41, (uint32_t)irq9,  0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_GATE_INTERRUPT);

    idt_set_gate(0x80, (uint32_t)irq15, 0x08, 0xEE);

    idt_flush((uint32_t)&idt_ptr);
}

extern "C" void isr_handler(Registers* regs) {
    static unsigned short* const VGA = (unsigned short*)0xB8000;
    auto ve = [](char c, uint8_t fg, uint8_t bg) -> unsigned short {
        return (unsigned short)c | ((unsigned short)((bg<<4)|fg)<<8);
    };
    auto pr = [&](const char* s, int row, int col, uint8_t fg) {
        for(int i=0;s[i];i++) VGA[row*80+col+i]=ve(s[i],fg,4);
    };
    auto phex = [&](uint32_t n, int row, int col) {
        const char* h="0123456789ABCDEF";
        char b[9]; b[8]=0;
        for(int i=7;i>=0;i--){b[i]=h[n&0xF];n>>=4;}
        for(int i=0;i<8;i++) VGA[row*80+col+i]=ve(b[i],15,4);
    };
    for(int i=0;i<80*6;i++) VGA[9*80+i]=ve(' ',15,4);
    pr("*** KERNEL EXCEPTION ***", 9,  2, 14);
    if (regs->int_no < 32) pr(exception_names[regs->int_no], 10, 2, 15);
    pr("INT:", 11, 2, 15);  phex(regs->int_no,   11, 7);
    pr("ERR:", 11, 20, 15); phex(regs->err_code, 11, 25);
    pr("EIP:", 12, 2, 15);  phex(regs->eip,      12, 7);
    pr("ESP:", 12, 20, 15); phex(regs->esp,       12, 25);
    pr("CS: ", 13, 2, 15);  phex(regs->cs,        13, 7);
    pr("EFL:", 13, 20, 15); phex(regs->eflags,    13, 25);
    for(;;) __asm__ volatile("cli; hlt");
}

extern "C" void irq_handler(Registers* regs) {
    if (regs->int_no == 0x80) {
        regs->eax = (uint32_t)syscall_dispatch(regs);
        return;
    }

    if (regs->int_no == 32) {
        pit_tick();
        scheduler_tick();
    }
    if (regs->int_no == 33) keyboard_handler();

    if (regs->int_no >= 40) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}