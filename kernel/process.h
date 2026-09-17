#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 8
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t stack_pointer;
    uint32_t stack_base;
    void (*entry_point)(void);
} process_t;

extern process_t process_table[MAX_PROCESSES];

void process_init(void);
int process_create(void (*entry_point)(void));
process_t* process_get(uint32_t pid);
int process_kill(uint32_t pid);
#endif
