
/* =============================================================================
 * SENG21213-OS :: Main Kernel
 * File   : kernel/kernel.c
 *
 * Stage 1 – Process Management
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

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);

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
        "  Faculty of Engineering – Department of Software Engineering",
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
        "  from bare metal. There is no Linux or Windows underneath – only\n"
    );
    vga_puts(
        "  the code you and your team write.\n"
    );

    vga_puts("  Assignment milestones to implement:\n");

    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Process Management  – PCB, ready queue, round-robin scheduler\n"
    );

    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Threads & Sync      – kernel threads, mutex, semaphore\n"
    );

    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "Memory Management   – physical page allocator, virtual memory\n"
    );

    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts(
        "File System         – RAM disk, FAT-like directory structure\n"
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
        "  ─────────────────────────────────────────────\n"
    );

    vga_puts("  help     – Show this help message\n");
    vga_puts("  clear    – Clear the screen\n");
    vga_puts("  about    – About this OS and course\n");
    vga_puts("  echo     – Echo text to screen\n");
    vga_puts("  mem      – Memory map (stub)\n");

    vga_puts_color(
        "\n  Stage 1 – Process Management:\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts("  ps       – List processes\n");
    vga_puts("  kill PID – Terminate a process\n");
    vga_puts("  ticks    – Show timer ticks\n");

    vga_puts_color(
        "\n  Future milestones:\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts("  threads  – [L10] List kernel threads\n");
    vga_puts("  free     – [L11] Show free memory\n");
    vga_puts("  ls       – [L12] List files\n");
    vga_puts("  cat      – [L12] Print file contents\n\n");
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
        "  ─────────────────────────────────────────────\n"
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
        "  Course       : SENG 21213 – Sem 2\n"
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
    vga_puts_color(
        "\n  Memory Map (stub – implement PMM in Lecture 11)\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ─────────────────────────────────────────────\n"
    );

    vga_puts(
        "  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n"
    );

    vga_puts(
        "  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n"
    );

    vga_puts(
        "  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n"
    );

    vga_puts(
        "  0xB8000    – 0xBFFFF     :  VGA frame buffer\n"
    );

    vga_puts_color(
        "\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
        VGA_YELLOW,
        VGA_BLACK
    );
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

        if (k_strcmp(cmd, "mem") == 0)
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

        /* Future milestone stubs */
        if (k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free") == 0 ||
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
        vga_puts("1");

        for (volatile unsigned int i = 0; i < 500000; i++)
        {
        }
    }
}

void test_process2(void)
{
    while (1)
    {
        vga_puts("2");

        for (volatile unsigned int i = 0; i < 500000; i++)
        {
        }
    }
}

void test_process3(void)
{
    while (1)
    {
        vga_puts("3");

        for (volatile unsigned int i = 0; i < 500000; i++)
        {
        }
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point
 * --------------------------------------------------------------------------*/
void kernel_main(void)
{
    vga_init();
    kb_init();

    process_init();
    scheduler_init();

    process_create(test_process1);
    process_create(test_process2);
    process_create(test_process3);

    interrupts_init();
    timer_init();

    __asm__ __volatile__("sti");

    print_splash();
    shell_run();

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}

