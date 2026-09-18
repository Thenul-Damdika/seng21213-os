#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "thread.h"

#define READY_QUEUE_SIZE (MAX_PROCESSES + MAX_THREADS)

void scheduler_init(void);

int scheduler_enqueue(int pid);
int scheduler_enqueue_thread(int tid);

int scheduler_dequeue(void);

int scheduler_is_empty(void);

int scheduler_next(void);

void scheduler_tick(void);

/* Context switching */
uint32_t scheduler_switch(uint32_t current_esp);

#endif
