// user/syscount.c

#include "kernel/types.h"
// #include "stat.h"
#include "kernel/syscall.h"
#include "user.h"
// Define syscall names based on syscall numbers
const char *syscall_names[32] = {
    [SYS_fork] "fork",
    [SYS_exit] "exit",
    [SYS_wait] "wait",
    [SYS_pipe] "pipe",
    [SYS_read] "read",
    [SYS_kill] "kill",
    [SYS_exec] "exec",
    [SYS_fstat] "fstat",
    [SYS_chdir] "chdir",
    [SYS_dup] "dup",
    [SYS_getpid] "getpid",
    [SYS_sbrk] "sbrk",
    [SYS_sleep] "sleep",
    [SYS_uptime] "uptime",
    [SYS_open] "open",
    [SYS_write] "write",
    [SYS_mknod] "mknod",
    [SYS_unlink] "unlink",
    [SYS_link] "link",
    [SYS_mkdir] "mkdir",
    [SYS_close] "close",
    [SYS_waitx] "waitx",
    [SYS_getSysCount] "getSysCount",
    // Initialize remaining indices to NULL or "unknown"
};

// Function to print usage
void usage(void)
{
    printf("Usage: syscount <mask> command [args...]\n");
    exit(0);
}
int popcount(int x)
{
    int count = 0;
    while (x)
    {
        count += x & 1; // Add the least significant bit
        x >>= 1;        // Shift right
    }
    return count;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        usage();
    }

    // Convert mask argument to integer
    int mask = atoi(argv[1]);

    // Validate that exactly one bit is set in the mask
    if (popcount(mask) != 1)
    {
        printf("Error: Mask must have exactly one bit set.\n");
        exit(0);
    }

    // Fork a child to run the command
    int pid = fork();
    if (pid < 0)
    {
        printf("syscount: fork failed\n");
        //  exit(0);
    }
    if (pid == 0)
    {
        // Child process: execute the command
        exec(argv[2], &argv[2]);
        // If exec returns, it failed
        //    printf( "syscount: exec %s failed\n", argv[2]);
        //  exit(0);
    }
    else
    {
        // Parent process: wait for the child to finish
        int status;
        wait(&status);
        // Get the syscall count
        uint64 count = getSysCount(mask, pid);
        // if(count == (uint64)-1){
        //    printf( "syscount: getSysCount failed\n");
        //  exit(0);
        // }

        // Determine the syscall number from the mask
        int syscall_num = 0;
        int temp_mask = mask;
        while ((temp_mask & 1) == 0 && syscall_num < 32)
        {
            temp_mask >>= 1;
            syscall_num++;
        }

        if (syscall_num >= 32)
        {
            printf("syscount: invalid syscall number derived from mask\n");
            exit(0);
        }

        // Get the syscall name
        const char *name = (syscall_names[syscall_num]) ? syscall_names[syscall_num] : "unknown";

        // Get the PID
        int caller_pid = getpid();

        // Print the result
        printf("PID %d called %s %d times.\n", caller_pid, name, count);
        //  exit(0);
    }
}
