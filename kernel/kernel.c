
/* =============================================================================
 * SENG21213-OS :: Main Kernel
 * File   : kernel/kernel.c
 *
 * Stage 1 - Process Management
 *   - PCB / process table
 *   - Process creation
 *   - Ready queue
 *   - Round-robin scheduler
 *   - PIT timer
 *   - Timer interrupt
 *   - Context switching
 *   - Process listing (ps)
 *   - Process termination (kill)
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "process.h"
#include "scheduler.h"
#include "interrupts.h"
#include "timer.h"
#include "thread.h"
#include "sync.h"
#include "pmm.h"
#include "fs.h"
#include "ramdisk.h"


/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_threads(void);
/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }

    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n)
{
    while (n-- && *a && (*a == *b))
    {
        a++;
        b++;
    }

    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
    {
        n++;
    }

    return n;
}

static const char *k_ltrim(const char *s)
{
    while (*s == ' ')
    {
        s++;
    }

    return s;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void)
{
    vga_clear(VGA_BLACK);

    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color(
        "  SENG21213-OS  |  Computer Architecture and Operating Systems",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_set_cursor(2, 2);
    vga_puts_color(
        "  Stage 1: Process Management",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_set_cursor(3, 2);
    vga_puts_color(
        "  Faculty of Engineering - Department of Software Engineering",
        VGA_LIGHT_GREY,
        VGA_BLACK
    );

    vga_set_cursor(4, 2);
    vga_puts_color(
        "  Built by students, for students.  Type 'help' to begin.",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_set_cursor(5, 2);
    vga_puts_color(
        "  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
        VGA_DARK_GREY,
        VGA_BLACK
    );

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_puts(
        "  Welcome! This kernel was compiled from source and booted entirely\n"
    );
    vga_puts(
        "  from bare metal. There is no Linux or Windows underneath - only\n"
    );
    vga_puts(
        "  the code you and your team write.\n"
    );

    vga_puts("  Assignment milestones to implement:\n");

    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Process Management  - PCB, ready queue, round-robin scheduler\n"
    );

    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Threads & Sync      - kernel threads, mutex, semaphore\n"
    );

    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Memory Management   - physical page allocator, virtual memory\n"
    );

    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "File System         - RAM disk, FAT-like directory structure\n"
    );

    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Timer command
 * --------------------------------------------------------------------------*/
void cmd_ticks(void)
{
    char num[12];
    unsigned int ticks = timer_get_ticks();
    int pos = 0;

    if (ticks == 0)
    {
        num[pos++] = '0';
    }
    else
    {
        char temp[12];
        int t = 0;

        while (ticks > 0)
        {
            temp[t++] = '0' + (ticks % 10);
            ticks /= 10;
        }

        while (t > 0)
        {
            num[pos++] = temp[--t];
        }
    }

    num[pos] = '\0';

    vga_puts_color(
        "\nTimer ticks: ",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(num);
    vga_puts("\n\n");
}

/* ---------------------------------------------------------------------------
 * Process listing command
 * --------------------------------------------------------------------------*/
void cmd_ps(void)
{
    char num[12];

    vga_puts_color(
        "\nPID   STATE\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_table[i].state != PROCESS_UNUSED)
        {
            int pid = process_table[i].pid;
            int pos = 0;

            if (pid == 0)
            {
                num[pos++] = '0';
            }
            else
            {
                char temp[12];
                int t = 0;

                while (pid > 0)
                {
                    temp[t++] = '0' + (pid % 10);
                    pid /= 10;
                }

                while (t > 0)
                {
                    num[pos++] = temp[--t];
                }
            }

            num[pos] = '\0';

            vga_puts("  ");
            vga_puts(num);
            vga_puts("    ");

            switch (process_table[i].state)
            {
                case PROCESS_READY:
                    vga_puts("READY");
                    break;

                case PROCESS_RUNNING:
                    vga_puts("RUNNING");
                    break;

                case PROCESS_BLOCKED:
                    vga_puts("BLOCKED");
                    break;

                case PROCESS_TERMINATED:
                    vga_puts("TERMINATED");
                    break;

                default:
                    vga_puts("UNKNOWN");
                    break;
            }

            vga_puts("\n");
        }
    }

    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Process termination command
 * --------------------------------------------------------------------------*/
void cmd_kill(const char *args)
{
    int pid = 0;

    while (*args == ' ')
    {
        args++;
    }

    if (*args < '0' || *args > '9')
    {
        vga_puts_color(
            "\nUsage: kill <PID>\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    while (*args >= '0' && *args <= '9')
    {
        pid = pid * 10 + (*args - '0');
        args++;
    }

    if (pid <= 0)
    {
        vga_puts_color(
            "\nInvalid PID.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    process_t *process = process_get((uint32_t)pid);

    if (process == 0)
    {
        vga_puts_color(
            "\nProcess not found.\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    if (process_kill((uint32_t)pid) != 0)
    {
        vga_puts_color(
            "\nProcess is already terminated.\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    vga_puts_color(
        "\nProcess terminated successfully.\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );
}

/* ---------------------------------------------------------------------------
 * Standard shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void)
{
    vga_puts_color(
        "\n  SENG21213-OS Shell Commands\n",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts("  help     - Show this help message\n");
    vga_puts("  clear    - Clear the screen\n");
    vga_puts("  about    - About this OS and course\n");
    vga_puts("  echo     - Echo text to screen\n");
    vga_puts("  mem      - Memory map (stub)\n");

    vga_puts_color(
        "\n  Stage 1 - Process Management:\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts("  ps       - List processes\n");
    vga_puts("  kill PID - Terminate a process\n");
    vga_puts("  ticks    - Show timer ticks\n");

    vga_puts_color(
        "\n  Future milestones:\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts("  threads  - [L10] List kernel threads\n");
    vga_puts("  free     - [L11] Show free memory\n");
    vga_puts("  ls       - [L12] List files\n");
    vga_puts("  cat      - [L12] Print file contents\n\n");
}

static void cmd_clear(void)
{
    vga_clear(VGA_BLACK);
}

static void cmd_about(void)
{
    vga_puts_color(
        "\n  About SENG21213-OS\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts(
        "  Architecture : x86 (i686), 32-bit Protected Mode\n"
    );

    vga_puts(
        "  Bootloader   : Custom MBR (NASM)\n"
    );

    vga_puts(
        "  Kernel       : Freestanding C (GCC, no libc)\n"
    );

    vga_puts(
        "  VM Target    : QEMU (qemu-system-i386)\n"
    );

    vga_puts(
        "  Course       : SENG 21213 - Sem 2\n"
    );

    vga_puts(
        "  Reference    : Stallings, OS: Internals & Design Principles\n\n"
    );
}

static void cmd_echo(const char *args)
{
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void)
{
    uint32_t total = pmm_get_total_frames();
    uint32_t used = pmm_get_used_frames();
    uint32_t free = pmm_get_free_frames();

    vga_puts_color(
        "\n  Physical Memory Manager\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_printf("  Total Frames : %u\n", total);
    vga_printf("  Used Frames  : %u\n", used);
    vga_printf("  Free Frames  : %u\n", free);

    vga_puts(
        "  Page Size    : 4096 bytes\n"
    );

    vga_puts(
        "  Total Memory : 32 MB\n\n"
    );
}


static void cmd_threads(void)
{
    vga_puts_color(
        "\n  Kernel Threads\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts("  TID     State\n");
    vga_puts("  ---------------------------------------------\n");

    int found = 0;

    for (int tid = 1; tid <= 32; tid++)
    {
        tcb_t *thread = thread_get(tid);

        if (thread != 0)
        {
            vga_printf("  %d       ", thread->tid);

            if (thread->state == THREAD_READY)
            {
                vga_puts("READY\n");
            }
            else if (thread->state == THREAD_RUNNING)
            {
                vga_puts("RUNNING\n");
            }
            else if (thread->state == THREAD_BLOCKED)
            {
                vga_puts("BLOCKED\n");
            }
            else if (thread->state == THREAD_TERMINATED)
            {
                vga_puts("TERMINATED\n");
            }
            else
            {
                vga_puts("UNKNOWN\n");
            }

            found = 1;
        }
    }

    if (found == 0)
    {
        vga_puts("  No threads found.\n");
    }

    vga_puts("\n");
}

static void cmd_ls(void);
static void cmd_touch(const char *args);
static void cmd_cat(const char *args);
static void cmd_write(const char *args);
static void cmd_rm(const char *args);

/* ---------------------------------------------------------------------------
 * Stage 4 - Filesystem commands
 * --------------------------------------------------------------------------*/

static void cmd_ls(void)
{
    char names[32][FS_FILENAME_LEN];

    int count = fs_list(
        names,
        32
    );

    vga_puts_color(
        "\n  Files\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    if (count <= 0)
    {
        vga_puts("  No files found.\n\n");
        return;
    }

    for (int i = 0; i < count; i++)
    {
        vga_puts("  ");
        vga_puts(names[i]);
        vga_puts("\n");
    }

    vga_puts("\n");
}


static void cmd_touch(const char *args)
{
    const char *name = k_ltrim(args);

    if (k_strlen(name) == 0)
    {
        vga_puts_color(
            "\n  Usage: touch <filename>\n\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    if (fs_create(name) == 0)
    {
        vga_puts_color(
            "\n  File created successfully.\n\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );
    }
    else
    {
        vga_puts_color(
            "\n  Failed to create file.\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}


static void cmd_cat(const char *args)
{
    const char *name = k_ltrim(args);

    if (k_strlen(name) == 0)
    {
        vga_puts_color(
            "\n  Usage: cat <filename>\n\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    int fd = fs_open(
        name,
        FS_MODE_READ
    );

    if (fd < 0)
    {
        vga_puts_color(
            "\n  File not found.\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    char buffer[128];

    int bytes_read;

    vga_puts("\n");

    while (1)
    {
        bytes_read = fs_read(
            fd,
            buffer,
            sizeof(buffer) - 1
        );

        if (bytes_read <= 0)
        {
            break;
        }

        buffer[bytes_read] = '\0';

        vga_puts(buffer);
    }

    vga_puts("\n");

    fs_close(fd);
}


static void cmd_write(const char *args)
{
    char filename[FS_FILENAME_LEN];
    char text[256];

    uint32_t i = 0;
    uint32_t j = 0;

    const char *input = k_ltrim(args);

    if (k_strlen(input) == 0)
    {
        vga_puts_color(
            "\n  Usage: write <filename> <text>\n\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    /*
     * Read filename.
     */
    while (
        input[i] != '\0' &&
        input[i] != ' ' &&
        j < FS_FILENAME_LEN - 1
    )
    {
        filename[j] = input[i];

        i++;
        j++;
    }

    filename[j] = '\0';

    /*
     * Skip spaces before text.
     */
    while (input[i] == ' ')
    {
        i++;
    }

    if (filename[0] == '\0' ||
        input[i] == '\0')
    {
        vga_puts_color(
            "\n  Usage: write <filename> <text>\n\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    /*
     * Copy text.
     */
    j = 0;

    while (
        input[i] != '\0' &&
        j < sizeof(text) - 1
    )
    {
        text[j] = input[i];

        i++;
        j++;
    }

    text[j] = '\0';

    /*
     * Open file for writing.
     */
    int fd = fs_open(
        filename,
        FS_MODE_WRITE
    );

    /*
     * If file doesn't exist, create it.
     */
    if (fd < 0)
    {
        if (fs_create(filename) != 0)
        {
            vga_puts_color(
                "\n  Failed to create file.\n\n",
                VGA_LIGHT_RED,
                VGA_BLACK
            );
            return;
        }

        fd = fs_open(
            filename,
            FS_MODE_WRITE
        );
    }

    if (fd < 0)
    {
        vga_puts_color(
            "\n  Failed to open file.\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    int written = fs_write(
        fd,
        text,
        j
    );

    fs_close(fd);

    if (written < 0)
    {
        vga_puts_color(
            "\n  Write failed.\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
        return;
    }

    vga_puts_color(
        "\n  File written successfully.\n\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );
}


static void cmd_rm(const char *args)
{
    const char *name = k_ltrim(args);

    if (k_strlen(name) == 0)
    {
        vga_puts_color(
            "\n  Usage: rm <filename>\n\n",
            VGA_YELLOW,
            VGA_BLACK
        );
        return;
    }

    if (fs_unlink(name) == 0)
    {
        vga_puts_color(
            "\n  File deleted successfully.\n\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );
    }
    else
    {
        vga_puts_color(
            "\n  Failed to delete file.\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK
        );
    }
}



/* ---------------------------------------------------------------------------
 * Shell
 * --------------------------------------------------------------------------*/
static char shell_buf[256];
static char prompt[] = "\n  ksh> ";

static void shell_run(void)
{
    vga_puts_color(
        "\n  Kernel Shell ready. Type 'help' for commands.\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    while (true)
    {
        vga_puts_color(
            prompt,
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );

        kb_readline(shell_buf, sizeof(shell_buf));

        const char *cmd = k_ltrim(shell_buf);

        if (k_strlen(cmd) == 0)
        {
            continue;
        }

        /* Standard commands */
        if (k_strcmp(cmd, "help") == 0)
        {
            cmd_help();
            continue;
        }

        if (k_strcmp(cmd, "clear") == 0)
        {
            cmd_clear();
            continue;
        }

        if (k_strcmp(cmd, "about") == 0)
        {
            cmd_about();
            continue;
        }

         if (k_strcmp(cmd, "mem") == 0 ||
             k_strcmp(cmd, "free") == 0)
        {
            cmd_mem();
            continue;
        }

        if (k_strncmp(cmd, "echo ", 5) == 0)
        {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        /* Stage 1 commands */
        if (k_strcmp(cmd, "ps") == 0)
        {
            cmd_ps();
            continue;
        }

        if (k_strncmp(cmd, "kill ", 5) == 0)
        {
            cmd_kill(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "ticks") == 0)
        {
            cmd_ticks();
            continue;
        }

                /* Stage 4 - Filesystem commands */

        if (k_strcmp(cmd, "ls") == 0)
        {
            cmd_ls();
            continue;
        }

        if (k_strncmp(cmd, "touch ", 6) == 0)
        {
            cmd_touch(k_ltrim(cmd + 6));
            continue;
        }

        if (k_strncmp(cmd, "cat ", 4) == 0)
        {
            cmd_cat(k_ltrim(cmd + 4));
            continue;
        }

        if (k_strncmp(cmd, "write ", 6) == 0)
        {
            cmd_write(k_ltrim(cmd + 6));
            continue;
        }

        if (k_strncmp(cmd, "rm ", 3) == 0)
        {
            cmd_rm(k_ltrim(cmd + 3));
            continue;
        }

	/* Thread command */
	if (k_strcmp(cmd, "threads") == 0)
	{
    	cmd_threads();
    	continue;
	}

        /* Future milestone stubs */
        if (
            k_strcmp(cmd, "ls") == 0 ||
            k_strcmp(cmd, "cat") == 0)
        {
            vga_puts_color(
                "  [TODO] This command is not yet implemented.\n",
                VGA_YELLOW,
                VGA_BLACK
            );

            vga_puts(
                "  Implement it as part of your lecture assignment.\n"
            );

            continue;
        }

        vga_puts_color(
            "  Unknown command: ",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

        vga_puts(cmd);

        vga_puts(
            "\n  Type 'help' for a list of commands.\n"
        );
    }
}

/* ---------------------------------------------------------------------------
 * Test processes for context switching
 * --------------------------------------------------------------------------*/
void test_process1(void)
{
    while (1)
    {
    }
}

void test_process2(void)
{
    while (1)
    {
    }
}

void test_process3(void)
{
    while (1)
    {
    }
}

void test_thread(void *arg)
{
    (void)arg;

    while (1)
    {
        vga_puts("T");

        for (volatile int i = 0; i < 1000000; i++)
        {
        }
    }
}



/* ---------------------------------------------------------------------------
 * Kernel entry point
 * --------------------------------------------------------------------------*/

volatile int myglobal = 0;
volatile int race_done = 0;
mutex_t my_mutex;

#define BUFFER_SIZE 5

int buffer[BUFFER_SIZE];
int buffer_in = 0;
int buffer_out = 0;

semaphore_t empty;
semaphore_t full;
mutex_t buffer_mutex;


void race_thread(void *arg)
{
    (void)arg;

    for (int i = 0; i < 1000; i++)
    {
        int temp = myglobal;

        for (volatile int j = 0; j < 10000; j++)
        {
        }

        myglobal = temp + 1;
    }

    race_done++;

    thread_exit();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}


    



void mutex_thread(void *arg)
{
    (void)arg;

    for (int i = 0; i < 1000; i++)
    {
        mutex_lock(&my_mutex);

        int temp = myglobal;

        for (volatile int j = 0; j < 10000; j++)
        {
        }

        myglobal = temp + 1;

        mutex_unlock(&my_mutex);
    }

    thread_exit();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}


void producer(void *arg)
{
    (void)arg;

    for (int i = 1; i <= 10; i++)
    {
        sem_wait(&empty);

        mutex_lock(&buffer_mutex);

        buffer[buffer_in] = i;
        buffer_in = (buffer_in + 1) % BUFFER_SIZE;

        mutex_unlock(&buffer_mutex);

        sem_signal(&full);
    }

    thread_exit();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}

void consumer(void *arg)
{
    (void)arg;

    for (int i = 0; i < 10; i++)
    {
        sem_wait(&full);

        mutex_lock(&buffer_mutex);

        int value = buffer[buffer_out];
        buffer_out = (buffer_out + 1) % BUFFER_SIZE;

        mutex_unlock(&buffer_mutex);

        sem_signal(&empty);
        (void)value;
    }

    thread_exit();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}


void kernel_main(void)
{
    vga_init();
    kb_init();

    process_init();
    thread_init();
    scheduler_init();
    pmm_init();


	/* Stage 4 filesystem */
     fs_init();

    mutex_init(&my_mutex);

    sem_init(&empty, BUFFER_SIZE);
    sem_init(&full, 0);

    mutex_init(&buffer_mutex);

    interrupts_init();
    timer_init();

    __asm__ __volatile__("sti");  

    print_splash();

    vga_puts_color(
        "\n=== STAGE 2 SYNCHRONIZATION TEST ===\n",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_puts("Creating kernel threads...\n");

    myglobal = 0;
    race_done = 0;

    thread_create(race_thread, 0);
    thread_create(race_thread, 0);

    vga_puts("Race condition test started.\n");

    /* Mutex test */
    myglobal = 0;

    thread_create(mutex_thread, 0);
    thread_create(mutex_thread, 0);

    vga_puts("Mutex test started.\n");

    /* Producer-consumer test */
    buffer_in = 0;
    buffer_out = 0;

    sem_init(&empty, BUFFER_SIZE);
    sem_init(&full, 0);
    mutex_init(&buffer_mutex);

    thread_create(producer, 0);
    thread_create(consumer, 0);

    vga_puts("Producer-consumer test started.\n");
    vga_puts("Stage 2 threads created successfully.\n\n");

    shell_run();

    __asm__ __volatile__("hlt");
}
