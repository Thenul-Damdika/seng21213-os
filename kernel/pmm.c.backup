#include "../include/types.h"
#include "pmm.h"

#define PAGE_SIZE 4096
#define MAX_MEMORY 0x02000000
#define MAX_FRAMES (MAX_MEMORY / PAGE_SIZE)
#define BITMAP_SIZE (MAX_FRAMES / 8)

static uint8_t frame_bitmap[BITMAP_SIZE];

static uint32_t total_frames = MAX_FRAMES;
static uint32_t used_frames = 0;

static void set_frame(uint32_t frame)
{
    frame_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void clear_frame(uint32_t frame)
{
    frame_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static int test_frame(uint32_t frame)
{
    return frame_bitmap[frame / 8] & (1 << (frame % 8));
}

void pmm_init(void)
{
    for (uint32_t i = 0; i < BITMAP_SIZE; i++)
    {
        frame_bitmap[i] = 0;
    }

    used_frames = 0;
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t i = 0; i < total_frames; i++)
    {
        if (!test_frame(i))
        {
            set_frame(i);
            used_frames++;

            return i * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t paddr)
{
    uint32_t frame = paddr / PAGE_SIZE;

    if (frame < total_frames && test_frame(frame))
    {
        clear_frame(frame);
        used_frames--;
    }
}

uint32_t pmm_get_total_frames(void)
{
    return total_frames;
}

uint32_t pmm_get_used_frames(void)
{
    return used_frames;
}

uint32_t pmm_get_free_frames(void)
{
    return total_frames - used_frames;
}

