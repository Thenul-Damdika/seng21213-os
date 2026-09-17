#include "timer.h"

static volatile unsigned int timer_ticks = 0;

static inline void outb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

void timer_init(void)
{
    /*
     * PIT input frequency:
     * 1,193,182 Hz
     *
     * Divisor 11931 gives approximately 100 Hz.
     */
    unsigned int divisor = 11931;

    /* Channel 0, low byte/high byte, square wave mode */
    outb(0x43, 0x36);

    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

int timer_handler(void)
{
    timer_ticks++;

    /*
     * Every 10 timer ticks, request a context switch.
     */
    if (timer_ticks % 10 == 0)
    {
        return 1;
    }

    return 0;
}

unsigned int timer_get_ticks(void)
{
    return timer_ticks;
}
