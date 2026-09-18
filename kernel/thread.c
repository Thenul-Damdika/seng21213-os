#include "thread.h"
#include "scheduler.h"

#define MAX_THREADS 8
#define THREAD_STACK_SIZE 4096

static tcb_t threads[MAX_THREADS];

static unsigned char thread_stacks[MAX_THREADS][THREAD_STACK_SIZE]
    __attribute__((aligned(16)));

static int next_tid = 1;
static int current_tid = -1;


/* Start a new thread */
static void thread_start(void)
{
    tcb_t *thread = thread_get(current_tid);

    if (thread != 0 && thread->function != 0)
    {
        thread->function(thread->arg);
    }

    thread_exit();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}


/* Initialize thread table */
void thread_init(void)
{
    for (int i = 0; i < MAX_THREADS; i++)
    {
        threads[i].tid = -1;
        threads[i].state = THREAD_UNUSED;
        threads[i].esp = 0;
        threads[i].stack_base = 0;
        threads[i].function = 0;
        threads[i].arg = 0;
    }

    next_tid = 1;
    current_tid = -1;
}


/* Get thread by TID */
tcb_t *thread_get(int tid)
{
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (threads[i].tid == tid &&
            threads[i].state != THREAD_UNUSED)
        {
            return &threads[i];
        }
    }

    return 0;
}


/* Create a new kernel thread */
int thread_create(void (*function)(void *), void *arg)
{
    if (function == 0)
    {
        return -1;
    }

    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (threads[i].state == THREAD_UNUSED)
        {
            tcb_t *thread = &threads[i];

            thread->tid = next_tid++;
            thread->state = THREAD_READY;

            thread->stack_base =
                (uint32_t)&thread_stacks[i][0];

            thread->function = function;
            thread->arg = arg;

            uint32_t *sp =
                (uint32_t *)&thread_stacks[i][THREAD_STACK_SIZE];

            /* Initial stack for timer_isr */

            *--sp = 0x202;                    /* EFLAGS */
            *--sp = 0x08;                     /* CS */
            *--sp = (uint32_t)thread_start;  /* EIP */

            *--sp = 0;    /* EAX */
            *--sp = 0;    /* ECX */
            *--sp = 0;    /* EDX */
            *--sp = 0;    /* EBX */
            *--sp = 0;    /* ESP */
            *--sp = 0;    /* EBP */
            *--sp = 0;    /* ESI */
            *--sp = 0;    /* EDI */

            thread->esp = (uint32_t)sp;

            /* Add thread to scheduler */
            if (scheduler_enqueue_thread(thread->tid) != 0)
            {
                thread->tid = -1;
                thread->state = THREAD_UNUSED;
                thread->esp = 0;
                thread->stack_base = 0;
                thread->function = 0;
                thread->arg = 0;

                return -1;
            }

            return thread->tid;
        }
    }

    return -1;
}


/* Terminate current thread */
void thread_exit(void)
{
    if (current_tid != -1)
    {
        tcb_t *thread = thread_get(current_tid);

        if (thread != 0)
        {
            thread->state = THREAD_TERMINATED;
        }
    }
}


/* Set current thread */
void thread_set_current(int tid)
{
    current_tid = tid;
}


/* Get current thread */
int thread_get_current(void)
{
    return current_tid;
}
