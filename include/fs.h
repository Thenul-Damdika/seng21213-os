#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_BLOCK_SIZE       4096
#define FS_TOTAL_BLOCKS     64

#define FS_MAX_INODES       128
#define FS_DIRECT_BLOCKS    8

#define FS_FILENAME_LEN     28
#define FS_MAGIC            0x53454E47

#define FS_SUPERBLOCK_BLOCK     0
#define FS_ROOT_DIR_BLOCK       1
#define FS_BLOCK_BITMAP_BLOCK   2
#define FS_INODE_BITMAP_BLOCK   3
#define FS_INODE_TABLE_START    4
#define FS_DATA_BLOCK_START     8

#define FS_MAX_OPEN_FILES      16

#define FS_MODE_READ           1
#define FS_MODE_WRITE          2

typedef struct
{
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t block_bitmap_block;
    uint32_t inode_bitmap_block;
    uint32_t inode_table_start;
    uint32_t data_block_start;
} fs_superblock_t;

typedef struct
{
    uint32_t used;
    uint32_t size;
    uint32_t direct_blocks[FS_DIRECT_BLOCKS];
} fs_inode_t;

typedef struct
{
    char name[FS_FILENAME_LEN];
    uint32_t inode;
} fs_dir_entry_t;

typedef struct
{
    uint32_t used;
    uint32_t inode;
    uint32_t position;
    uint32_t mode;
} fs_file_t;

void fs_init(void);

int fs_create(const char *name);
int fs_open(const char *name, uint32_t mode);
int fs_close(int fd);

int fs_read(
    int fd,
    void *buffer,
    uint32_t size
);

int fs_write(
    int fd,
    const void *buffer,
    uint32_t size
);

int fs_unlink(const char *name);

int fs_list(
    char names[][FS_FILENAME_LEN],
    uint32_t max_names
);

#endif
