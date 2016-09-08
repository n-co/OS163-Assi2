#ifndef KTHREAD_H
#define KTHREAD_H

#define NTHREAD 16

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct thread {
  char *kstack;                // Bottom of kernel stack for this thread
  enum procstate state;      // Process state
  int tid;					   // thread ID
  struct proc *tproc;		   // containing process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed   ????
  char name[16];               // thread name (debugging)
  
};

#endif