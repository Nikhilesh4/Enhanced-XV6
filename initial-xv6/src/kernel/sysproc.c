#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#define NSYSCALLS 32 // Maximum number of system calls

extern struct
{
  struct spinlock lock;
  struct proc procs[NPROC];
} ptable;

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_waitx(void)
{
  uint64 addr, addr1, addr2;
  uint wtime, rtime;
  argaddr(0, &addr);
  argaddr(1, &addr1); // user virtual memory
  argaddr(2, &addr2);
  int ret = waitx(addr, &wtime, &rtime);
  struct proc *p = myproc();
  if (copyout(p->pagetable, addr1, (char *)&wtime, sizeof(int)) < 0)
    return -1;
  if (copyout(p->pagetable, addr2, (char *)&rtime, sizeof(int)) < 0)
    return -1;
  return ret;
}
int sum_syscall_counts(struct proc *p, int syscall_num)
{
  if (p == 0)
    return 0;

  int count = p->syscall_count[syscall_num];

  // Iterate through all child processes
  struct proc *child;
  for (child = proc; child < &proc[NPROC]; child++)
  {
    if (child->parent1 == p)
    {
      count += sum_syscall_counts(child, syscall_num);
    }
  }

  return count;
}

int sys_getSysCount(void)
{
  int mask;
  argint(0, &mask);
  // Ensure only one bit is set
  if ((mask & (mask - 1)) != 0 || mask == 0)
    return -1; // Invalid mask

  // Find the syscall number from the mask
  int syscall_num = 0;
  while ((mask >> syscall_num) > 1)
  {
    syscall_num++;
  }

  if (syscall_num >= MAX_SYSCALLS)
    return -1; // Invalid syscall number

  struct proc *current = myproc();

  // Sum syscall counts from current process and its descendants
  int total_count = sum_syscall_counts(current, syscall_num);

  return total_count;
}


// Helper function to check if a user-space address is valid
int
vaddr_check(uint64 addr)
{
    struct proc *p = myproc();
    if (addr < p->sz && addr >= p->trapframe->sp - PGSIZE) // Simple check
        return 1;
    return 0;
}
// sys_sigalarm implementation

  uint64
  sys_sigalarm(void)
{
    int ticks;
    uint64 handler;

    // Retrieve the first argument (ticks)
    argint(0, &ticks);

    // Retrieve the second argument (handler function address)
    argaddr(1, &handler);

    // Get the current process
    struct proc *p = myproc();
// Acquire the process lock to modify its state
    acquire(&p->lock);
    p->alarmticks = ticks;
    p->ticks_elapsed = 0;
    p->alarmhandler = (void (*)())handler;
    p->in_handler = 0; // Reset the handler flag
    release(&p->lock);

    return 0;
}
// sys_sigreturn implementation
uint64
sys_sigreturn(void)
{
    struct proc *p = myproc();

    // Acquire the process lock to modify its state
    acquire(&p->lock);

    // Ensure that sigreturn is called from within a handler
    if (!p->in_handler) {
        release(&p->lock);
        return -1;
    }

    memmove(p->trapframe, &p->tf_backup, sizeof(struct trapframe));

    p->in_handler = 0;

    release(&p->lock);

    return 0;
}


uint64
sys_settickets(void)
{
    int num;
    argint(0, &num) ;
    struct proc *curproc = myproc();
    acquire(&curproc->lock);
    curproc->tickets = num;
    release(&curproc->lock);
    return curproc->tickets;
}

