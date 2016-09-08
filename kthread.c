#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "kthread.h"
#include "proc.h"
#include "spinlock.h"

static int nexttid = 0;

extern void forkret(void);
extern void trapret(void);


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
  return 0; //TODO 1.2
}

int kthread_id(void){
  if(thread->tid < 0)
    return -1;
  return thread->tid;
}

void kthread_exit(void){  // TODO: not sure about this implementation
  struct thread *t;
  
  wakeup1(thread);
  thread->state = ZOMBIE;

  int num_of_threads;
  for(t = thread->tproc->pthreads; t < &thread->tproc->pthreads[NTHREAD] ; t++){
      if(t->state != UNUSED && t->state != ZOMBIE)
        num_of_threads++;
  }
  if(num_of_threads==0){
    exit();
  }
}

int kthread_join(int thread_id){
  struct thread *t;
  for(t = proc->pthreads; t < &proc->pthreads[NTHREAD]; t++)
    if(t->tid != thread_id)
      continue;
  if(t == &proc->pthreads[NTHREAD])
    return -1; // thread_id not found

  if(t->state == ZOMBIE){
    kfree(t->kstack);
    t->kstack = 0;
    p->state = UNUSED;
    return 0;
  }
  else if(t->state == UNUSED)
    return 0;
  else{
    // TODO
  }

  return 0; //TODO 1.2
}