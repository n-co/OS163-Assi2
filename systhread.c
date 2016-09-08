#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"


int sys_kthread_create(void){
  return 0;
}

int sys_kthread_id(void){
  return kthread_id();
}

int sys_kthread_exit(void){
  return 0;
}
int sys_kthread_join(void){
  return 0;
}

