#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES 8

#define PROCESS_UNUSED 0
#define PROCESS_READY 1
#define PROCESS_RUNNING 2
#define PROCESS_BLOCKED 3
#define PROCESS_TERMINATED 4

typedef struct
{
    int pid;
    int state;

    unsigned int esp;
    unsigned int ebp;

    unsigned int stack_base;
    unsigned int stack_top;

} pcb_t;

extern pcb_t process_table[MAX_PROCESSES];

void process_init(void);
int process_create(void);
pcb_t* process_get(int pid);

#endif
