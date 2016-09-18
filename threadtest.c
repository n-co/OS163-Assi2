// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"

#define STK_SIZE 4096

void test1(void);
void* test1_threadfunc();
void test2(void);
void* test2_threadfunc();

int tester2;
int tester11;

int main(void){
  int pid;
  //int tester1 = 0;
  int tester22 = 1;
  tester11 = 0;

  pid = fork();
  if(pid==0){
  	//tester1 = 1;
  	printf(1, "TEST 1.a. PASSED - process forked succesfully\n");
  	test1();
  	
  	exit();
  }
  else{
  	wait();
  	printf(1, "TEST 1.b. PASSED - process killed by executing\n");
  }
  //if(tester1!=1)
  //	printf(1, "TEST 1.a. FAILED - process didn't forked\n");
  

  pid = fork();
  if(pid == 0){
  	test2();
  	tester22 = 0;
  	exit();
  }
  else
  	wait();
  if(tester22==1)
  	printf(1, "TEST 2.d. PASSED - the last kthread_exit() killed the process\n");
  else
  	printf(1, "TEST 2.d. FAILED - the last kthread_exit() didn't killed the process\n");

  exit();
}

void test1(void){
	void *stack = malloc(STK_SIZE);	

	kthread_create(test1_threadfunc, stack, STK_SIZE);

	while(tester11 == 0) sleep(10);

	char *a[2] = {"cd", 0};
	exec(a[0], a);
}

void* test1_threadfunc(){
	while(1){
		tester11 = 1;
	}

	return 0;
}

void test2(void){
	tester2 = -1;
	//printf(1, "test 1 - start:\n");
	void *stack = malloc(STK_SIZE);
	int tid = kthread_id();
	int new_tid = kthread_create(test2_threadfunc, stack, STK_SIZE);
	
	if(new_tid<0){
		printf(1, "TEST 2.a. FAILED - thread was not created\n");
		exit();
	}

	//thread was created
	kthread_join(new_tid);
	if(tester2 == -1)
		printf(1, "TEST 2.b. FAILED - kthread_join not waiting\n");
	else
		printf(1, "TEST 2.b. PASSED - thread #%d joined with #%d\n", tid, new_tid);
	if(tester2 == 1)
		printf(1, "TEST 2.c. PASSED - thread exit with kthread_exit()\n");

	//printf(1, "test 1 - end\n");
	kthread_exit();
}

void* test2_threadfunc(){
	int tid = kthread_id();
	printf(1, "TEST 2.a. PASSED - thread #%d created succesfully\n", tid);
	tester2 = 1;
	kthread_exit();
	tester2 = 0;
	printf(1, "TEST 2.c FAILED - thread #%d was not exit with kthread_exit()\n", tid);
	exit();
}