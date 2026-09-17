#include "process.h"
#include "scheduler.h"

process_t process_table[MAX_PROCESSES];

/* One private stack for each process */
static unsigned char process_stacks[MAX_PROCESSES][PROCESS_STACK_SIZE]
    __attribute__((aligned(16)));

static uint32_t next_pid = 1;

void process_init(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_UNUSED;
        process_table[i].stack_pointer = 0;
        process_table[i].stack_base =
            (uint32_t)&process_stacks[i][0];
        process_table[i].entry_point = 0;
    }

    next_pid = 1;
}

int process_create(void (*entry_point)(void))
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_table[i].state == PROCESS_UNUSED)
        {
            process_table[i].pid = next_pid++;
            process_table[i].state = PROCESS_READY;

            process_table[i].stack_base =
                (uint32_t)&process_stacks[i][0];

            process_table[i].entry_point = entry_point;

            /*
             * Build an initial stack that has the same layout
             * that timer_isr expects after pusha.
             *
             * Lowest address:
             *   EDI
             *   ESI
             *   EBP
             *   ESP dummy
             *   EBX
             *   EDX
             *   ECX
             *   EAX
             *   EIP
             *   CS
             *   EFLAGS
             */
            uint32_t *sp =
                (uint32_t *)&process_stacks[i][PROCESS_STACK_SIZE];

            /* Build in reverse because the stack grows downward */
            *--sp = 0x202;                       /* EFLAGS */
            *--sp = 0x08;                        /* CS */
            *--sp = (uint32_t)entry_point;       /* EIP */

            *--sp = 0;                           /* EAX */
            *--sp = 0;                           /* ECX */
            *--sp = 0;                           /* EDX */
            *--sp = 0;                           /* EBX */
            *--sp = 0;                           /* ESP dummy */
            *--sp = 0;                           /* EBP */
            *--sp = 0;                           /* ESI */
            *--sp = 0;                           /* EDI */

            process_table[i].stack_pointer =
                (uint32_t)sp;

            /* Add process to ready queue */
            if (scheduler_enqueue(process_table[i].pid) != 0)
            {
                process_table[i].pid = 0;
                process_table[i].state = PROCESS_UNUSED;
                process_table[i].stack_pointer = 0;
                process_table[i].stack_base = 0;
                process_table[i].entry_point = 0;

                return -1;
            }

            return process_table[i].pid;
        }
    }

    return -1;
}

process_t* process_get(uint32_t pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_table[i].pid == pid &&
            process_table[i].state != PROCESS_UNUSED)
        {
            return &process_table[i];
        }
    }

    return 0;
}

int process_kill(uint32_t pid)
{
    process_t *process = process_get(pid);

    if (process == 0)
    {
        return -1;
    }

    if (process->state == PROCESS_TERMINATED)
    {
        return -1;
    }

    process->state = PROCESS_TERMINATED;

    return 0;
}
