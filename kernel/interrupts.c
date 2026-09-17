#include "interrupts.h"

/* IDT entry */
typedef struct
{
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_high;
} __attribute__((packed)) idt_entry_t;

/* IDT pointer */
typedef struct
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

/* Timer interrupt handler from assembly */
extern void timer_isr(void);

/* Port I/O */
static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void idt_set_gate(
    int vector,
    unsigned int handler,
    unsigned short selector)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = 0x8E;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
}

static void pic_remap(void)
{
    /* Start initialization */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* New interrupt vector offsets */
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    /* Tell master/slave PIC about each other */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /* 8086 mode */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Mask all IRQs for now */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    /* Unmask only IRQ0 (timer) */
    outb(0x21, 0xFE);
}

void interrupts_init(void)
{
    for (int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    /* CS from our bootloader GDT = 0x08 */
    idt_set_gate(32, (unsigned int)timer_isr, 0x08);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (unsigned int)&idt;

    __asm__ __volatile__(
        "lidt %0"
        :
        : "m"(idt_ptr)
    );

    pic_remap();
}
