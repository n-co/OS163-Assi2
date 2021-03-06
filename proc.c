#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct thread*
allocproc(void)
{
  struct proc *p;
  struct thread *t;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->executed = 0;
  for(t = p->pthreads; t < &p->pthreads[NTHREAD]; t++)
    t->state = UNUSED;

  release(&ptable.lock);

  initlock(&p->lock, "proc");

  return allocthread(p);
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  struct thread *t;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  t = allocproc();
  p = t->tproc;
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  
  p->sz = PGSIZE;
  memset(t->tf, 0, sizeof(*t->tf));
  t->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  t->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  t->tf->es = t->tf->ds;
  t->tf->ss = t->tf->ds;
  t->tf->eflags = FL_IF;
  t->tf->esp = PGSIZE;
  t->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  t->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  //acquire(&proc->lock); // 1.1
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
     //release(&proc->lock); // 1.1
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      //release(&proc->lock); // 1.1
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  //release(&proc->lock); // 1.1
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct thread *nt;
  struct proc *np;

  // Allocate process.
  if((nt = allocproc()) == 0)
    return -1;

  np = nt->tproc;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(nt->kstack);
    nt->kstack = 0;
    nt->state = UNUSED;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;

  *nt->tf = *thread->tf;

  // Clear %eax so that fork returns 0 in the child.
  nt->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  nt->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;
}

int
cow_fork(void)
{
  int i, pid;
  struct thread *nt;
  struct proc *np;

  // Allocate process.
  if((nt = allocproc()) == 0)
    return -1;

  np = nt->tproc;

  // Copy process state from p.
  if((np->pgdir = cow_shareuvm(proc->pgdir, proc->sz)) == 0){
    kfree(nt->kstack);
    nt->kstack = 0;
    nt->state = UNUSED;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;

  *nt->tf = *thread->tf;

  // Clear %eax so that fork returns 0 in the child.
  nt->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  nt->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;

}





// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // iterate all threads in the process and kill them
  struct thread* t;
  for(t = proc->pthreads; t < &proc->pthreads[NTHREAD]; t++){
    if(t->state != UNUSED){
      t->state = ZOMBIE;
      t->chan = 0;
    }
  }
  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  struct thread *t;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        for(t = p->pthreads; t < &p->pthreads[NTHREAD]; t++){
          if(t->state == UNUSED)
            continue;
          if(t->state == ZOMBIE){
            kfree(t->kstack);
            t->kstack = 0;
            continue;
          }
          // t is not ZOMBIE or UNUSED, shouldnt reach here
          panic("wait: thread is still alive while process is dead"); 
        } 
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }
    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

int
cow_wait(void)
{
  struct proc *p;
  struct thread *t;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        for(t = p->pthreads; t < &p->pthreads[NTHREAD]; t++){
          if(t->state == UNUSED)
            continue;
          if(t->state == ZOMBIE){
            kfree(t->kstack);
            t->kstack = 0;
            continue;
          }
          // t is not ZOMBIE or UNUSED, shouldnt reach here
          panic("wait: thread is still alive while process is dead"); 
        } 
        cow_freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }
    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct thread *t;

  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      //if(p->state != RUNNABLE)
      //  continue;

      for(t = p->pthreads; t < &p->pthreads[NTHREAD]; t++){
        if(t->state != RUNNABLE)
          continue;
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.

        //cprintf("pid: %d, tid: %d\n",p->pid, t->tid);
        proc = p;
        thread = t;
        switchuvm(p);
        //p->state = RUNNING;
        t->state = RUNNING;
        swtch(&cpu->scheduler, t->context);
        switchkvm();
        thread = 0;
      }
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1){
    cprintf("cpu->ncli: %d\n", cpu->ncli);
    panic("sched locks");
  }
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&thread->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  thread->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0 || thread == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  thread->chan = chan;
  thread->state = SLEEPING;
  sched();

  // Tidy up.
  thread->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct thread* t;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    for(t = p->pthreads ; t < &p->pthreads[NTHREAD] ; t++)
      if(t->state == SLEEPING && t->chan == chan)
        t->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      //if(p->state == SLEEPING)
      //  p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  struct thread *t;
  char *state;
  uint pc[10];
  
  cprintf(" - - - - - - - - procdump start - - - - - - - - \n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("proc: %d %s %s\n", p->pid, state, p->name);
    print_memory_usage(p->pgdir);

    for(t = p->pthreads; t < &p->pthreads[NTHREAD]; t++){
      if(t->state == UNUSED)
        continue;
      if(t->state >= 0 && t->state < NELEM(states) && states[t->state])
        state = states[t->state];
      else
        state = "???";
      cprintf("thread: %d %s %s", t->tid, state, t->name);
      for(t=p->pthreads; t < &p->pthreads[NTHREAD]; t++){
        if(t->state == SLEEPING){
          getcallerpcs((uint*)t->context->ebp+2, pc);
          for(i=0; i<10 && pc[i] != 0; i++)
            cprintf(" %p", pc[i]);
        }
      }
      cprintf("\n");
    }
  }
  cprintf(" - - - - - - - -  procdump end  - - - - - - - - \n");
}

void acqptable(void){
  acquire(&ptable.lock);
}
void relptable(void){
  release(&ptable.lock);
}

#define VPN ((d<<10) + t)
#define PPN (pgtab[t] >> 12)
void print_memory_usage(pde_t* pgdir){

  int d,t;
  pte_t *pgtab;
  char *writeable;
  int refs;
  for(d = 0; d<1024; d++){
    if((pgdir[d] & PTE_U) && (pgdir[d] & PTE_P)){
      pgtab = (pte_t*)p2v(PTE_ADDR(pgdir[d]));
      
      for(t = 0; t<1024; t++){
        if((pgtab[t] & PTE_U) && (pgtab[t] & PTE_P)){
          if((pgtab[t] & PTE_W) && !(pgtab[t] & PTE_SH))
            writeable = "y";
          else
            writeable = "n";
          refs = page_refs(PPN);
          cprintf("%p -> %p , [w: %s] , [refs: %d]\n", VPN, PPN, writeable, refs);
        }
      }
    }
  }
}