#ifndef TIMER_H
#define TIMER_H

void timer_init(void);

/* Returns 1 when a process switch is due */
int timer_handler(void);

unsigned int timer_get_ticks(void);

#endif
