#include "scheduler.h"

/*
 * Positive values represent processes.
 * Negative values represent threads.
 *
 * Example:
 *   3  = process PID 3
 *  -1  = thread TID 1
 *  -2  = thread TID 2
 */
static int ready_queue[READY_QUEUE_SIZE];

static int queue_head;
static int queue_tail;
static int queue_count;

/* Current running object */
static int current_id;
static int current_is_thread;

void scheduler_init(void)
{
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;

    current_id = -1;
    current_is_thread = 0;

    for (int i = 0; i < READY_QUEUE_SIZE; i++)
    {
        ready_queue[i] = 0;
    }
}

int scheduler_is_empty(void)
{
    return queue_count == 0;
}

int scheduler_enqueue(int pid)
{
    if (queue_count >= READY_QUEUE_SIZE)
    {
        return -1;
    }

    ready_queue[queue_tail] = pid;

    queue_tail++;

    if (queue_tail >= READY_QUEUE_SIZE)
    {
        queue_tail = 0;
    }

    queue_count++;

    return 0;
}

int scheduler_enqueue_thread(int tid)
{
    if (queue_count >= READY_QUEUE_SIZE)
    {
        return -1;
    }

    /*
     * Store thread TID as negative value.
     */
    ready_queue[queue_tail] = -tid;

    queue_tail++;

    if (queue_tail >= READY_QUEUE_SIZE)
    {
        queue_tail = 0;
    }

    queue_count++;

    return 0;
}

int scheduler_dequeue(void)
{
    if (queue_count == 0)
    {
        return -1;
    }

    int id = ready_queue[queue_head];

    ready_queue[queue_head] = 0;

    queue_head++;

    if (queue_head >= READY_QUEUE_SIZE)
    {
        queue_head = 0;
    }

    queue_count--;

    return id;
}

int scheduler_next(void)
{
    while (!scheduler_is_empty())
    {
        int id = scheduler_dequeue();

        /*
         * Process
         */
        if (id > 0)
        {
            process_t *process =
                process_get((uint32_t)id);

            if (process == 0)
            {
                continue;
            }

            if (process->state != PROCESS_READY)
            {
                continue;
            }

            process->state = PROCESS_RUNNING;

            current_id = id;
            current_is_thread = 0;

            return id;
        }

        /*
         * Thread
         */
        if (id < 0)
        {
            int tid = -id;

            tcb_t *thread = thread_get(tid);

            if (thread == 0)
            {
                continue;
            }

            if (thread->state != THREAD_READY)
            {
                continue;
            }

            thread->state = THREAD_RUNNING;

            current_id = tid;
            current_is_thread = 1;

            return id;
        }
    }

    return -1;
}

void scheduler_tick(void)
{
    /*
     * Put current running object back into
     * the READY queue.
     */
    if (current_id != -1)
    {
        if (current_is_thread)
        {
            tcb_t *thread =
                thread_get(current_id);

            if (thread != 0 &&
                thread->state == THREAD_RUNNING)
            {
                thread->state = THREAD_READY;
                scheduler_enqueue_thread(current_id);
            }
        }
        else
        {
            process_t *process =
                process_get((uint32_t)current_id);

            if (process != 0 &&
                process->state == PROCESS_RUNNING)
            {
                process->state = PROCESS_READY;
                scheduler_enqueue(current_id);
            }
        }
    }

    scheduler_next();
}

uint32_t scheduler_switch(uint32_t current_esp)
{
    /*
     * Save current stack pointer.
     */
    if (current_id != -1)
    {
        if (current_is_thread)
        {
            tcb_t *thread =
                thread_get(current_id);

            if (thread != 0)
            {
                thread->esp = current_esp;

                if (thread->state == THREAD_RUNNING)
                {
                    thread->state = THREAD_READY;
                    scheduler_enqueue_thread(current_id);
                }
            }
        }
        else
        {
            process_t *process =
                process_get((uint32_t)current_id);

            if (process != 0)
            {
                process->stack_pointer = current_esp;

                if (process->state == PROCESS_RUNNING)
                {
                    process->state = PROCESS_READY;
                    scheduler_enqueue(current_id);
                }
            }
        }
    }

    /*
     * Select next object.
     */
    int next = scheduler_next();

    if (next == -1)
    {
        return current_esp;
    }

    /*
     * Next process.
     */
    if (next > 0)
    {
        process_t *process =
            process_get((uint32_t)next);

        if (process == 0)
        {
            return current_esp;
        }

        return process->stack_pointer;
    }

    /*
     * Next thread.
     */
    int tid = -next;

    tcb_t *thread = thread_get(tid);

    if (thread == 0)
    {
        return current_esp;
    }

    return thread->esp;
}
