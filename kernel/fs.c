#include "fs.h"
#include "ramdisk.h"
#include <stdint.h>

/*
 * SENG 21213 - Stage 4
 * Simple RAM Disk Filesystem
 *
 * Layout:
 *
 * Block 0       : Superblock
 * Block 1       : Root directory
 * Block 2       : Block bitmap
 * Block 3       : Inode bitmap
 * Blocks 4 - 7  : Inode table
 * Blocks 8 -255  : File data
 *
 * Each inode has 8 direct blocks.
 * Maximum file size = 8 * 4096 = 32768 bytes.
 */

static fs_superblock_t superblock;

static fs_file_t open_files[FS_MAX_OPEN_FILES];


/* ============================================================
 * Basic memory helpers
 * ============================================================ */

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


/* ============================================================
 * String helpers
 * ============================================================ */

static uint32_t string_length(const char *str)
{
    uint32_t length = 0;

    if (str == 0)
    {
        return 0;
    }

    while (str[length] != '\0')
    {
        length++;
    }

    return length;
}


static int string_equal(
    const char *a,
    const char *b
)
{
    uint32_t i = 0;

    if (a == 0 || b == 0)
    {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
        {
            return 0;
        }

        i++;
    }

    return a[i] == b[i];
}


/* ============================================================
 * Bitmap helpers
 * ============================================================ */

static int bitmap_test(
    const uint8_t *bitmap,
    uint32_t index
)
{
    return
        (bitmap[index / 8] &
        (uint8_t)(1 << (index % 8))) != 0;
}


static void bitmap_set(
    uint8_t *bitmap,
    uint32_t index
)
{
    bitmap[index / 8] |=
        (uint8_t)(1 << (index % 8));
}


static void bitmap_clear(
    uint8_t *bitmap,
    uint32_t index
)
{
    bitmap[index / 8] &=
        (uint8_t)~(1 << (index % 8));
}


/* ============================================================
 * Block bitmap
 * ============================================================ */

static void load_block_bitmap(
    uint8_t *bitmap
)
{
    ramdisk_read_block(
        FS_BLOCK_BITMAP_BLOCK,
        bitmap
    );
}


static void save_block_bitmap(
    const uint8_t *bitmap
)
{
    ramdisk_write_block(
        FS_BLOCK_BITMAP_BLOCK,
        bitmap
    );
}


static int find_free_block(void)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    load_block_bitmap(bitmap);

    for (uint32_t block = FS_DATA_BLOCK_START;
         block < FS_TOTAL_BLOCKS;
         block++)
    {
        if (!bitmap_test(bitmap, block))
        {
            return (int)block;
        }
    }

    return -1;
}


static int allocate_block(void)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    load_block_bitmap(bitmap);

    for (uint32_t block = FS_DATA_BLOCK_START;
         block < FS_TOTAL_BLOCKS;
         block++)
    {
        if (!bitmap_test(bitmap, block))
        {
            bitmap_set(bitmap, block);

            save_block_bitmap(bitmap);

            return (int)block;
        }
    }

    return -1;
}


static void free_block(uint32_t block)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    if (block < FS_DATA_BLOCK_START ||
        block >= FS_TOTAL_BLOCKS)
    {
        return;
    }

    load_block_bitmap(bitmap);

    bitmap_clear(bitmap, block);

    save_block_bitmap(bitmap);
}


/* ============================================================
 * Inode bitmap
 * ============================================================ */

static void load_inode_bitmap(
    uint8_t *bitmap
)
{
    ramdisk_read_block(
        FS_INODE_BITMAP_BLOCK,
        bitmap
    );
}


static void save_inode_bitmap(
    const uint8_t *bitmap
)
{
    ramdisk_write_block(
        FS_INODE_BITMAP_BLOCK,
        bitmap
    );
}


static int find_free_inode(void)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    load_inode_bitmap(bitmap);

    for (uint32_t inode = 1;
         inode < FS_MAX_INODES;
         inode++)
    {
        if (!bitmap_test(bitmap, inode))
        {
            return (int)inode;
        }
    }

    return -1;
}


static int allocate_inode(void)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    load_inode_bitmap(bitmap);

    for (uint32_t inode = 1;
         inode < FS_MAX_INODES;
         inode++)
    {
        if (!bitmap_test(bitmap, inode))
        {
            bitmap_set(bitmap, inode);

            save_inode_bitmap(bitmap);

            return (int)inode;
        }
    }

    return -1;
}


static void free_inode(uint32_t inode)
{
    uint8_t bitmap[FS_BLOCK_SIZE];

    if (inode == 0 ||
        inode >= FS_MAX_INODES)
    {
        return;
    }

    load_inode_bitmap(bitmap);

    bitmap_clear(bitmap, inode);

    save_inode_bitmap(bitmap);
}


/* ============================================================
 * Inode table helpers
 * ============================================================ */

static void inode_location(
    uint32_t inode_number,
    uint32_t *block,
    uint32_t *offset
)
{
    uint32_t byte_offset;

    byte_offset =
        inode_number * sizeof(fs_inode_t);

    *block =
        FS_INODE_TABLE_START +
        (byte_offset / FS_BLOCK_SIZE);

    *offset =
        byte_offset % FS_BLOCK_SIZE;
}


static int read_inode(
    uint32_t inode_number,
    fs_inode_t *inode
)
{
    uint32_t block;
    uint32_t offset;

    uint8_t block_buffer[FS_BLOCK_SIZE];

    if (inode == 0 ||
        inode_number >= FS_MAX_INODES)
    {
        return -1;
    }

    inode_location(
        inode_number,
        &block,
        &offset
    );

    if (offset + sizeof(fs_inode_t) >
        FS_BLOCK_SIZE)
    {
        return -1;
    }

    ramdisk_read_block(
        block,
        block_buffer
    );

    memory_copy(
        inode,
        &block_buffer[offset],
        sizeof(fs_inode_t)
    );

    return 0;
}


static int write_inode(
    uint32_t inode_number,
    const fs_inode_t *inode
)
{
    uint32_t block;
    uint32_t offset;

    uint8_t block_buffer[FS_BLOCK_SIZE];

    if (inode == 0 ||
        inode_number >= FS_MAX_INODES)
    {
        return -1;
    }

    inode_location(
        inode_number,
        &block,
        &offset
    );

    if (offset + sizeof(fs_inode_t) >
        FS_BLOCK_SIZE)
    {
        return -1;
    }

    ramdisk_read_block(
        block,
        block_buffer
    );

    memory_copy(
        &block_buffer[offset],
        inode,
        sizeof(fs_inode_t)
    );

    ramdisk_write_block(
        block,
        block_buffer
    );

    return 0;
}


/* ============================================================
 * Root directory helpers
 * ============================================================ */

#define FS_MAX_DIR_ENTRIES \
    (FS_BLOCK_SIZE / sizeof(fs_dir_entry_t))


static int read_directory(
    fs_dir_entry_t *entries
)
{
    if (entries == 0)
    {
        return -1;
    }

    ramdisk_read_block(
        FS_ROOT_DIR_BLOCK,
        entries
    );

    return 0;
}


static int write_directory(
    const fs_dir_entry_t *entries
)
{
    if (entries == 0)
    {
        return -1;
    }

    ramdisk_write_block(
        FS_ROOT_DIR_BLOCK,
        entries
    );

    return 0;
}


static int find_directory_entry(
    const char *name
)
{
    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];

    if (name == 0)
    {
        return -1;
    }

    read_directory(entries);

    for (uint32_t i = 0;
         i < FS_MAX_DIR_ENTRIES;
         i++)
    {
        if (entries[i].name[0] != '\0')
        {
            if (string_equal(
                    entries[i].name,
                    name))
            {
                return (int)i;
            }
        }
    }

    return -1;
}


static int find_free_directory_entry(void)
{
    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];

    read_directory(entries);

    for (uint32_t i = 0;
         i < FS_MAX_DIR_ENTRIES;
         i++)
    {
        if (entries[i].name[0] == '\0')
        {
            return (int)i;
        }
    }

    return -1;
}


/* ============================================================
 * Filesystem initialization
 * ============================================================ */

void fs_init(void)
{
    uint8_t block_bitmap[FS_BLOCK_SIZE];
    uint8_t inode_bitmap[FS_BLOCK_SIZE];

    uint8_t empty_block[FS_BLOCK_SIZE];

    uint8_t inode_table_block[FS_BLOCK_SIZE];

    fs_inode_t root_inode;


    /*
     * Start with a clean RAM disk.
     */
    ramdisk_init();


    /*
     * Clear bitmaps and temporary blocks.
     */
    memory_set(
        block_bitmap,
        0,
        FS_BLOCK_SIZE
    );

    memory_set(
        inode_bitmap,
        0,
        FS_BLOCK_SIZE
    );

    memory_set(
        empty_block,
        0,
        FS_BLOCK_SIZE
    );


    /*
     * Configure superblock.
     */
    superblock.magic =
        FS_MAGIC;

    superblock.total_blocks =
        FS_TOTAL_BLOCKS;

    superblock.total_inodes =
        FS_MAX_INODES;

    superblock.block_bitmap_block =
        FS_BLOCK_BITMAP_BLOCK;

    superblock.inode_bitmap_block =
        FS_INODE_BITMAP_BLOCK;

    superblock.inode_table_start =
        FS_INODE_TABLE_START;

    superblock.data_block_start =
        FS_DATA_BLOCK_START;


    /*
     * Reserve filesystem metadata blocks.
     *
     * Blocks 0-7 are reserved.
     */
    for (uint32_t block = 0;
         block < FS_DATA_BLOCK_START;
         block++)
    {
        bitmap_set(
            block_bitmap,
            block
        );
    }


    /*
     * Inode 0 is the root inode.
     */
    bitmap_set(
        inode_bitmap,
        0
    );


    /*
     * Create root inode.
     */
    memory_set(
        &root_inode,
        0,
        sizeof(fs_inode_t)
    );

    root_inode.used = 1;
    root_inode.size = 0;

    /*
     * Root directory occupies block 1.
     */
    root_inode.direct_blocks[0] =
        FS_ROOT_DIR_BLOCK;


    /*
     * Write superblock.
     */
    ramdisk_write_block(
        FS_SUPERBLOCK_BLOCK,
        &superblock
    );


    /*
     * Write block bitmap.
     */
    ramdisk_write_block(
        FS_BLOCK_BITMAP_BLOCK,
        block_bitmap
    );


    /*
     * Write inode bitmap.
     */
    ramdisk_write_block(
        FS_INODE_BITMAP_BLOCK,
        inode_bitmap
    );


    /*
     * Clear inode table.
     */
    memory_set(
        inode_table_block,
        0,
        FS_BLOCK_SIZE
    );

    for (uint32_t block = FS_INODE_TABLE_START;
         block < FS_DATA_BLOCK_START;
         block++)
    {
        ramdisk_write_block(
            block,
            inode_table_block
        );
    }


    /*
     * Store root inode.
     */
    write_inode(
        0,
        &root_inode
    );


    /*
     * Clear root directory.
     */
    ramdisk_write_block(
        FS_ROOT_DIR_BLOCK,
        empty_block
    );


    /*
     * Clear open file table.
     */
    for (uint32_t i = 0;
         i < FS_MAX_OPEN_FILES;
         i++)
    {
        open_files[i].used = 0;
        open_files[i].inode = 0;
        open_files[i].position = 0;
        open_files[i].mode = 0;
    }
}


/* ============================================================
 * Create file
 * ============================================================ */

int fs_create(const char *name)
{
    uint32_t name_length;

    int inode_number;
    int directory_index;

    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];

    fs_inode_t inode;


    if (name == 0)
    {
        return -1;
    }


    name_length =
        string_length(name);


    /*
     * Empty filename is invalid.
     */
    if (name_length == 0)
    {
        return -1;
    }


    /*
     * Filename must fit inside directory entry.
     *
     * One byte is reserved for '\0'.
     */
    if (name_length >= FS_FILENAME_LEN)
    {
        return -1;
    }


    /*
     * Prevent duplicate files.
     */
    if (find_directory_entry(name) >= 0)
    {
        return -1;
    }


    /*
     * Find free inode.
     */
    inode_number =
        allocate_inode();

    if (inode_number < 0)
    {
        return -1;
    }


    /*
     * Find free directory entry.
     */
    directory_index =
        find_free_directory_entry();

    if (directory_index < 0)
    {
        free_inode(
            (uint32_t)inode_number
        );

        return -1;
    }


    /*
     * Create empty inode.
     */
    memory_set(
        &inode,
        0,
        sizeof(fs_inode_t)
    );

    inode.used = 1;
    inode.size = 0;


    /*
     * Save inode.
     */
    if (write_inode(
            (uint32_t)inode_number,
            &inode) != 0)
    {
        free_inode(
            (uint32_t)inode_number
        );

        return -1;
    }


    /*
     * Load directory.
     */
    read_directory(entries);


    /*
     * Clear directory entry.
     */
    memory_set(
        &entries[directory_index],
        0,
        sizeof(fs_dir_entry_t)
    );


    /*
     * Copy filename.
     */
    for (uint32_t i = 0;
         i < name_length;
         i++)
    {
        entries[directory_index].name[i] =
            name[i];
    }

    entries[directory_index].name[name_length] =
        '\0';


    /*
     * Store inode number.
     */
    entries[directory_index].inode =
        (uint32_t)inode_number;


    /*
     * Save directory.
     */
    write_directory(entries);


    return 0;
}


/* ============================================================
 * Open file
 * ============================================================ */

int fs_open(
    const char *name,
    uint32_t mode
)
{
    int directory_index;

    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];


    if (name == 0)
    {
        return -1;
    }


    if (mode != FS_MODE_READ &&
        mode != FS_MODE_WRITE)
    {
        return -1;
    }


    /*
     * Find file.
     */
    directory_index =
        find_directory_entry(name);

    if (directory_index < 0)
    {
        return -1;
    }


    /*
     * Load directory.
     */
    read_directory(entries);


    uint32_t inode_number =
        entries[directory_index].inode;


    /*
     * Find free file descriptor.
     */
    for (uint32_t fd = 0;
         fd < FS_MAX_OPEN_FILES;
         fd++)
    {
        if (!open_files[fd].used)
        {
            open_files[fd].used = 1;

            open_files[fd].inode =
                inode_number;

            open_files[fd].position = 0;

            open_files[fd].mode =
                mode;

            return (int)fd;
        }
    }


    return -1;
}


/* ============================================================
 * Close file
 * ============================================================ */

int fs_close(int fd)
{
    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES)
    {
        return -1;
    }


    if (!open_files[fd].used)
    {
        return -1;
    }


    open_files[fd].used = 0;
    open_files[fd].inode = 0;
    open_files[fd].position = 0;
    open_files[fd].mode = 0;


    return 0;
}


/* ============================================================
 * Read file
 * ============================================================ */

int fs_read(
    int fd,
    void *buffer,
    uint32_t size
)
{
    fs_inode_t inode;

    uint8_t block_buffer[FS_BLOCK_SIZE];

    uint8_t *destination =
        (uint8_t *)buffer;

    uint32_t remaining;
    uint32_t position;

    uint32_t total_read = 0;


    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES)
    {
        return -1;
    }


    if (!open_files[fd].used)
    {
        return -1;
    }


    if (buffer == 0)
    {
        return -1;
    }


    if (size == 0)
    {
        return 0;
    }


    if (open_files[fd].mode != FS_MODE_READ)
    {
        return -1;
    }


    /*
     * Load inode.
     */
    if (read_inode(
            open_files[fd].inode,
            &inode) != 0)
    {
        return -1;
    }


    position =
        open_files[fd].position;


    /*
     * Already at end of file.
     */
    if (position >= inode.size)
    {
        return 0;
    }


    remaining =
        inode.size - position;


    if (size < remaining)
    {
        remaining = size;
    }


    /*
     * Read block-by-block.
     */
    while (remaining > 0)
    {
        uint32_t block_index =
            position / FS_BLOCK_SIZE;

        uint32_t block_offset =
            position % FS_BLOCK_SIZE;

        uint32_t bytes_to_copy =
            FS_BLOCK_SIZE - block_offset;


        if (bytes_to_copy > remaining)
        {
            bytes_to_copy = remaining;
        }


        /*
         * Check direct block range.
         */
        if (block_index >= FS_DIRECT_BLOCKS)
        {
            break;
        }


        /*
         * File block must exist.
         */
        if (inode.direct_blocks[block_index] == 0)
        {
            break;
        }


        ramdisk_read_block(
            inode.direct_blocks[block_index],
            block_buffer
        );


        memory_copy(
            &destination[total_read],
            &block_buffer[block_offset],
            bytes_to_copy
        );


        position += bytes_to_copy;
        remaining -= bytes_to_copy;
        total_read += bytes_to_copy;
    }


    /*
     * Update file position.
     */
    open_files[fd].position =
        position;


    return (int)total_read;
}


/* ============================================================
 * Write file
 * ============================================================ */

int fs_write(
    int fd,
    const void *buffer,
    uint32_t size
)
{
    fs_inode_t inode;

    uint8_t block_buffer[FS_BLOCK_SIZE];

    const uint8_t *source =
        (const uint8_t *)buffer;

    uint32_t position;

    uint32_t remaining;

    uint32_t total_written = 0;


    if (fd < 0 ||
        fd >= FS_MAX_OPEN_FILES)
    {
        return -1;
    }


    if (!open_files[fd].used)
    {
        return -1;
    }


    if (buffer == 0)
    {
        return -1;
    }


    if (open_files[fd].mode != FS_MODE_WRITE)
    {
        return -1;
    }


    if (size == 0)
    {
        return 0;
    }


    /*
     * Load inode.
     */
    if (read_inode(
            open_files[fd].inode,
            &inode) != 0)
    {
        return -1;
    }


    position =
        open_files[fd].position;


    /*
     * Maximum file size:
     *
     * 8 direct blocks * 4096 bytes
     */
    if (position >=
        FS_DIRECT_BLOCKS * FS_BLOCK_SIZE)
    {
        return -1;
    }


    if (size >
        (FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) - position)
    {
        size =
            (FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) - position;
    }


    remaining = size;


    /*
     * Write block-by-block.
     */
    while (remaining > 0)
    {
        uint32_t block_index =
            position / FS_BLOCK_SIZE;

        uint32_t block_offset =
            position % FS_BLOCK_SIZE;

        uint32_t bytes_to_copy =
            FS_BLOCK_SIZE - block_offset;


        if (bytes_to_copy > remaining)
        {
            bytes_to_copy = remaining;
        }


        /*
         * Check direct block limit.
         */
        if (block_index >= FS_DIRECT_BLOCKS)
        {
            break;
        }


        /*
         * Allocate block if needed.
         */
        if (inode.direct_blocks[block_index] == 0)
        {
            int new_block =
                allocate_block();

            if (new_block < 0)
            {
                break;
            }

            inode.direct_blocks[block_index] =
                (uint32_t)new_block;

            /*
             * New blocks start empty.
             */
            memory_set(
                block_buffer,
                0,
                FS_BLOCK_SIZE
            );
        }
        else
        {
            /*
             * Existing block must be read
             * because this may be a partial write.
             */
            ramdisk_read_block(
                inode.direct_blocks[block_index],
                block_buffer
            );
        }


        /*
         * Copy user data into block.
         */
        memory_copy(
            &block_buffer[block_offset],
            &source[total_written],
            bytes_to_copy
        );


        /*
         * Write block back.
         */
        ramdisk_write_block(
            inode.direct_blocks[block_index],
            block_buffer
        );


        position += bytes_to_copy;
        remaining -= bytes_to_copy;
        total_written += bytes_to_copy;


        /*
         * Increase file size.
         */
        if (position > inode.size)
        {
            inode.size = position;
        }
    }


    /*
     * Save inode.
     */
    write_inode(
        open_files[fd].inode,
        &inode
    );


    /*
     * Update current position.
     */
    open_files[fd].position =
        position;


    return (int)total_written;
}


/* ============================================================
 * Delete file
 * ============================================================ */

int fs_unlink(const char *name)
{
    int directory_index;

    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];

    fs_inode_t inode;


    if (name == 0)
    {
        return -1;
    }


    /*
     * Never delete root.
     */
    if (string_equal(name, "/"))
    {
        return -1;
    }


    /*
     * Find directory entry.
     */
    directory_index =
        find_directory_entry(name);

    if (directory_index < 0)
    {
        return -1;
    }


    /*
     * Load directory.
     */
    read_directory(entries);


    uint32_t inode_number =
        entries[directory_index].inode;


    /*
     * Check whether file is open.
     */
    for (uint32_t fd = 0;
         fd < FS_MAX_OPEN_FILES;
         fd++)
    {
        if (open_files[fd].used &&
            open_files[fd].inode ==
                inode_number)
        {
            return -1;
        }
    }


    /*
     * Load inode.
     */
    if (read_inode(
            inode_number,
            &inode) != 0)
    {
        return -1;
    }


    /*
     * Free file data blocks.
     */
    for (uint32_t i = 0;
         i < FS_DIRECT_BLOCKS;
         i++)
    {
        if (inode.direct_blocks[i] != 0)
        {
            free_block(
                inode.direct_blocks[i]
            );

            inode.direct_blocks[i] = 0;
        }
    }


    /*
     * Clear inode.
     */
    memory_set(
        &inode,
        0,
        sizeof(fs_inode_t)
    );


    write_inode(
        inode_number,
        &inode
    );


    /*
     * Free inode number.
     */
    free_inode(
        inode_number
    );


    /*
     * Clear directory entry.
     */
    memory_set(
        &entries[directory_index],
        0,
        sizeof(fs_dir_entry_t)
    );


    /*
     * Save directory.
     */
    write_directory(entries);


    return 0;
}


/* ============================================================
 * List files
 * ============================================================ */

int fs_list(
    char names[][FS_FILENAME_LEN],
    uint32_t max_names
)
{
    fs_dir_entry_t entries[FS_MAX_DIR_ENTRIES];

    uint32_t count = 0;


    if (names == 0 ||
        max_names == 0)
    {
        return 0;
    }


    read_directory(entries);


    for (uint32_t i = 0;
         i < FS_MAX_DIR_ENTRIES;
         i++)
    {
        if (entries[i].name[0] != '\0')
        {
            if (count >= max_names)
            {
                break;
            }


            memory_set(
                names[count],
                0,
                FS_FILENAME_LEN
            );


            memory_copy(
                names[count],
                entries[i].name,
                FS_FILENAME_LEN
            );


            count++;
        }
    }


    return (int)count;
}
