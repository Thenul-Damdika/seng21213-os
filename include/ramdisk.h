#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>

#define RAMDISK_SIZE       (256 * 1024)
#define RAMDISK_BLOCK_SIZE 4096
#define RAMDISK_BLOCKS     (RAMDISK_SIZE / RAMDISK_BLOCK_SIZE)

/*
 * SENG 21213 - Stage 4
 * L12 §1 - RAM Disk
 *
 * 1 MB fixed-size RAM disk.
 */

void ramdisk_init(void);

void ramdisk_read_block(
    uint32_t block,
    void *buffer
);

void ramdisk_write_block(
    uint32_t block,
    const void *buffer
);

uint8_t *ramdisk_data(void);

#endif
