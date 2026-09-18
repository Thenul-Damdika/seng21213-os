#ifndef THREAD_H
#define THREAD_H

typedef void (*thread_fn)(void *arg);

typedef enum {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct {
    int tid;
    thread_state_t state;

    unsigned int esp;
    unsigned int stack_base;

    thread_fn function;
    void *arg;
} tcb_t;


void thread_init(void)
{
    int i;

    for (i = 0; i < MAX_THREADS; i++) {
        threads[i].tid = -1;
        threads[i].state = THREAD_UNUSED;
        threads[i].esp = 0;
        threads[i].stack_base = 0;
        threads[i].function = 0;
        threads[i].arg = 0;
    }
}







int thread_create(thread_fn function, void *arg);

void thread_exit(void);

tcb_t *thread_get(int tid);

#endif
