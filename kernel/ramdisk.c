#include "ramdisk.h"
#include <stdint.h>

static uint8_t ramdisk[RAMDISK_SIZE];

/*
 * Simple memory set for freestanding kernel.
 */
static void memory_set(
    void *destination,
    uint8_t value,
    uint32_t size
)
{
    uint8_t *dest = (uint8_t *)destination;

    for (uint32_t i = 0; i < size; i++)
    {
        dest[i] = value;
    }
}

/*
 * Simple memory copy for freestanding kernel.
 */
static void memory_copy(
    void *destination,
    const void *source,
    uint32_t size
)
{
    uint8_t *dest = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;

    for (uint32_t i = 0; i < size; i++)
    {
        dest[i] = src[i];
    }
}

void ramdisk_init(void)
{
    memory_set(
        ramdisk,
        0,
        RAMDISK_SIZE
    );
}

void ramdisk_read_block(
    uint32_t block,
    void *buffer
)
{
    if (block >= RAMDISK_BLOCKS || buffer == 0)
    {
        return;
    }

    memory_copy(
        buffer,
        &ramdisk[block * RAMDISK_BLOCK_SIZE],
        RAMDISK_BLOCK_SIZE
    );
}

void ramdisk_write_block(
    uint32_t block,
    const void *buffer
)
{
    if (block >= RAMDISK_BLOCKS || buffer == 0)
    {
        return;
    }

    memory_copy(
        &ramdisk[block * RAMDISK_BLOCK_SIZE],
        buffer,
        RAMDISK_BLOCK_SIZE
    );
}

uint8_t *ramdisk_data(void)
{
    return ramdisk;
}
