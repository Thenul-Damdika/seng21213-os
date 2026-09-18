#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"

#define MAX_THREADS 8
#define THREAD_STACK_SIZE 4096

typedef enum
{
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct
{
    int tid;
    thread_state_t state;

    uint32_t esp;
    uint32_t stack_base;

    void (*function)(void *);
    void *arg;
} tcb_t;

void thread_init(void);

int thread_create(void (*function)(void *), void *arg);

tcb_t *thread_get(int tid);

void thread_exit(void);


void thread_set_current(int tid);
int thread_get_current(void);


#endif
