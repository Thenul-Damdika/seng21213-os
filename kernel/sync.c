#include "sync.h"

void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
}

void mutex_lock(mutex_t *mutex)
{
    while (mutex->locked)
    {
        __asm__ __volatile__("hlt");
    }

    mutex->locked = 1;
}

void mutex_unlock(mutex_t *mutex)
{
    mutex->locked = 0;
}

void sem_init(semaphore_t *sem, int value)
{
    sem->count = value;
}

void sem_wait(semaphore_t *sem)
{
    while (sem->count <= 0)
    {
        __asm__ __volatile__("hlt");
    }

    sem->count--;
}

void sem_signal(semaphore_t *sem)
{
    sem->count++;
}
