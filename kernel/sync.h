#ifndef SYNC_H
#define SYNC_H

typedef struct
{
    volatile int locked;
} mutex_t;

typedef struct
{
    volatile int count;
} semaphore_t;


/* Mutex functions */
void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);


/* Semaphore functions */
void sem_init(semaphore_t *sem, int value);
void sem_wait(semaphore_t *sem);
void sem_signal(semaphore_t *sem);

#endif
