#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_kthread_create(void){
  void*(*start_func)();
  void *stack;
  int stack_size;
  if(argint(0, (int*)&start_func) < 0 ||
  	 argint(1, (int*)&stack) < 0 ||
  	 argint(2, &stack_size) < 0)
  	return -1;
  return kthread_create(start_func, stack, stack_size);
}

int sys_kthread_id(void){
  return kthread_id();
}

int sys_kthread_exit(void){
  kthread_exit();
  return 0;
}
int sys_kthread_join(void){
  int tid;
  if(argint(0, &tid) < 0)
    return -1;
  return kthread_join(tid);
}