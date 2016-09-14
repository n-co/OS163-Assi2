#include "types.h"
#include "stat.h"
#include "user.h"

void test1(){ // basic fork
  printf(1,"TEST1 - procdump before fork:\n");
  procdump();
  int pid = cow_fork();
  if(pid == 0){
    printf(1,"TEST1 - child pid: %d procdump:\n", getpid());
    procdump();
    exit();
  }
  else{
    cow_wait();
    printf(1,"TEST1 - parent pid: %d procdump:\n", getpid());
    procdump();
  }
}

void test2(){ // change variable in process
  int *var = malloc(sizeof(int));
  *var = 40;
  printf(1,"TEST2 - var: %d  procdump before fork:\n", *var);
  procdump();
  int pid = cow_fork();
  if(pid == 0){
    printf(1,"TEST2 - child pid: %d.   var=%d before changing var\n", getpid(), *var);
    procdump();
    *var = 20;
    printf(1,"TEST2 - child pid: %d.   var=%d after changing var\n", getpid(), *var);
    procdump();

    exit();
  }
  else{
    cow_wait();
    printf(1,"TEST2 - parent pid: %d.   var=%d before changing var - after child is dead\n", getpid(), *var);
    procdump();
    *var = 0;
    printf(1,"TEST2 - parent pid: %d.   var=%d after changing var\n", getpid(), *var);
    procdump();
  }

}

void test3(){ // check the refernces
  int *var = malloc(sizeof(int));
  *var = 40;
  printf(1,"TEST3 - var: %d\n", *var);
  int pid = cow_fork();
  int pid2;
  if(pid == 0){
    sleep(50);
    *var = 20;
    printf(1,"TEST3 - child pid: %d.   var=%d after changing var\n", getpid(), *var);
    exit();
  }
  else{
    pid2=cow_fork();
    if(pid2 == 0){
      procdump();
      *var = 50;
      //printf(1,"TEST3 - child pid: %d.   var=%d after changing var\n", getpid(), *var);
      sleep(300);
      printf(1,"TEST3 - child pid: %d.   var=%d after first child died \n", getpid(), *var);
      procdump();
      exit();
    }
    cow_wait();
    cow_wait();
    //printf(1,"TEST2 - parent pid: %d.   var=%d before changing var - after child is dead\n", getpid(), *var);
    *var = 0;
    //printf(1,"TEST2 - parent pid: %d.   var=%d after changing var\n", getpid(), *var);
    //procdump();
  }
  *var = 80;

}

int
main(int argc, char *argv[])
{
  test1();
  test2(); 
  test3();
  exit();

}