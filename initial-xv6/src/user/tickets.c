// lottery_test.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

#define TICKS_PER_SEC 100  // Adjust based on your system's tick rate

// Function to busy-wait for a specified number of ticks
void wait_ticks(int ticks) {
    int start = uptime();
    while (uptime() - start < ticks) {
        // Busy wait
    }
}

int main(int argc, char *argv[]) {
    int pidA, pidB, pidC;

    // Start Process A at t=0s
    pidA = fork();
    if(pidA < 0){
        printf("Failed to fork Process A\n");
      exit(0);
    }
    if(pidA == 0){
        // Child Process A
        settickets(3);
        printf("Process A started with PID=%d and 3 tickets\n", getpid());
        while(1){
            // Simulate work
            // Optionally, print when scheduled
            sleep(5);
        }
      exit(0);
    }

    // Wait for 3 seconds (assuming 100 ticks per second)
    wait_ticks(3 * TICKS_PER_SEC);

    // Fork Process B at t=3s
    pidB = fork();
    if(pidB < 0){
        printf("Failed to fork Process B\n");
      exit(0);
    }
    if(pidB == 0){
        // Child Process B
        settickets(4);
        printf("Process B started with PID=%d and 4 tickets\n", getpid());
        while(1){
            // Simulate work
            sleep(5);
        }
      exit(0);
    }

    // Wait for 1 second (t=4s)
    wait_ticks(1 * TICKS_PER_SEC);

    // Fork Process C at t=4s
    pidC = fork();
    if(pidC < 0){
        printf("Failed to fork Process C\n");
      exit(0);
    }
    if(pidC == 0){
        // Child Process C
        settickets(3);
        printf("Process C started with PID=%d and 3 tickets\n", getpid());
        while(1){
            // Simulate work
            sleep(5);
        }
      exit(0);
    }

    // Wait until t=5s
    wait_ticks(1 * TICKS_PER_SEC);

    // At t=5s, simulate a condition where C is chosen as winner
    // Since we cannot directly control the scheduler's random selection,
    // we rely on the scheduler's logging to verify behavior.

    printf("Test setup complete. Monitoring scheduler logs for correct behavior.\n");

    // Let the system run for additional time to observe scheduling
    wait_ticks(5 * TICKS_PER_SEC);

    // Clean up: Kill all child processes
    kill(pidA);
    kill(pidB);
    kill(pidC);
    wait(0);
    wait(0);
    wait(0);

    printf("Lottery scheduling test completed.\n");
  exit(0);
}
