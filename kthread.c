#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "kthread.h"
#include "proc.h"
#include "spinlock.h"

static int nexttid = 1;

extern void forkret(void);
extern void trapret(void);

static void wakeup2(void *chan);


struct thread*
allocthread(struct proc *p)
{
  struct thread *t;
  char *sp;

  acquire(&p->lock);
  for(t=p->pthreads; t<&p->pthreads[NTHREAD]; t++)
    if(t->state == UNUSED)
      goto found;
  release(&p->lock);
  return 0;

found:
  // initializing threads table as UNUSED, and their containing process as p
  t->state = EMBRYO;
  t->tid = nexttid++;
  t->tproc = p;
  release(&p->lock);

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0){
    t->state = UNUSED;
    return 0;
  }
  sp = t->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;
  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;

  return t;
}

int kthread_create(void*(*start_func)(), void* stack, int stack_size){
  struct thread *t;  
  t = allocthread(proc);
  if(t==0)
    return -1;
  *t->tf = *thread->tf; // initialize tf with non-garbage values 
  t->tf->eip = (uint)start_func;
  t->tf->esp = (uint)(stack + stack_size);

  acquire(&proc->lock);
  t->state = RUNNABLE;
  release(&proc->lock);
  
  return t->tid; 
}

int kthread_id(void){
  if(thread->tid <= 0)
    return -1;
  return thread->tid;
}

void kthread_exit(void){
  struct thread *t;
  acqptable();
  thread->state = ZOMBIE;

  int num_of_threads = 0;
  for(t = thread->tproc->pthreads; t < &thread->tproc->pthreads[NTHREAD] ; t++){
      if(t->state != UNUSED && t->state != ZOMBIE)
        num_of_threads++;
  }
  wakeup2(thread);
  if(num_of_threads==0){
    relptable();
    exit();
  }
  sched(); //should not return from here
  panic("zombie thread exit");
}

int kthread_join(int thread_id){
  struct thread *t;
  acquire(&proc->lock);
  for(t = proc->pthreads; t < &proc->pthreads[NTHREAD]; t++)
    if(t->tid == thread_id)
      break;
  if(t == &proc->pthreads[NTHREAD]){
    // thread_id not found 
    release(&proc->lock);
    return -1;   
  }

  if(t->state == ZOMBIE){
    kfree(t->kstack);
    t->kstack = 0;
    t->state = UNUSED;
    release(&proc->lock);
    return 0;
  }
  else if(t->state == UNUSED){
    release(&proc->lock);
    return 0;
  }
  
  else{ // RUNNABLE / RUNNING / EMBRYO etc
    while(t->state != ZOMBIE){
      sleep(t, &proc->lock);
    }

    if(t->state == UNUSED)
      panic("kthread_join: thread become unused");
    
    kfree(t->kstack);
    t->kstack = 0;
    t->state = UNUSED;
    release(&proc->lock);
    return 0;
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup2(void *chan)
{
  struct thread *t;
  for(t = proc->pthreads; t < &proc->pthreads[NPROC]; t++)
    if(t->state == SLEEPING && t->chan == chan){
      t->state = RUNNABLE;
    }
}