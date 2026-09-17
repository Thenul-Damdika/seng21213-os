#include "scheduler.h"

static int ready_queue[READY_QUEUE_SIZE];

static int queue_head;
static int queue_tail;
static int queue_count;

static int current_pid;

void scheduler_init(void)
{
    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    current_pid = -1;

    for (int i = 0; i < READY_QUEUE_SIZE; i++)
    {
        ready_queue[i] = -1;
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

int scheduler_dequeue(void)
{
    if (queue_count == 0)
    {
        return -1;
    }

    int pid = ready_queue[queue_head];

    ready_queue[queue_head] = -1;

    queue_head++;

    if (queue_head >= READY_QUEUE_SIZE)
    {
        queue_head = 0;
    }

    queue_count--;

    return pid;
}

int scheduler_next(void)
{
    int pid;

    while (!scheduler_is_empty())
    {
        pid = scheduler_dequeue();

        process_t *process =
            process_get((uint32_t)pid);

        if (process == 0)
        {
            continue;
        }

        if (process->state != PROCESS_READY)
        {
            continue;
        }

        process->state = PROCESS_RUNNING;

        return pid;
    }

    return -1;
}

void scheduler_tick(void)
{
    /*
     * Put the current running process back into
     * the READY queue.
     *
     * Do not requeue a terminated process.
     */
    if (current_pid != -1)
    {
        process_t *current =
            process_get((uint32_t)current_pid);

        if (current != 0)
        {
            if (current->state == PROCESS_RUNNING)
            {
                current->state = PROCESS_READY;
                scheduler_enqueue(current_pid);
            }
        }
    }

    /*
     * Select the next READY process.
     */
    current_pid = scheduler_next();
}

uint32_t scheduler_switch(uint32_t current_esp)
{
    /*
     * Save the current process stack pointer.
     */
    if (current_pid != -1)
    {
        process_t *current =
            process_get((uint32_t)current_pid);

        if (current != 0)
        {
            current->stack_pointer = current_esp;

            /*
             * If the current process is still running,
             * return it to the READY queue.
             */
            if (current->state == PROCESS_RUNNING)
            {
                current->state = PROCESS_READY;
                scheduler_enqueue(current_pid);
            }
        }
    }

    /*
     * Select the next READY process.
     */
    int next_pid_selected = scheduler_next();

    if (next_pid_selected == -1)
    {
        return current_esp;
    }

    current_pid = next_pid_selected;

    process_t *next =
        process_get((uint32_t)current_pid);

    if (next == 0)
    {
        return current_esp;
    }

    return next->stack_pointer;
}
