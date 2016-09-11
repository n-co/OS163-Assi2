// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "types.h"
#include "stat.h"
#include "user.h"

void test1(int mx);
void test2(int mx);

int main(void){

	int pid;
	int mutex1=kthread_mutex_alloc();
	int mutex2=kthread_mutex_alloc();

	//int i=0;
	/*for(;i<1;i++){
		fork();
		test1(mutex1);
	}*/
    printf(1, "test1: son & father calls to test 1 function\n");

	pid=fork();
	test1(mutex1);
	sleep(20);
	test1(mutex1);

	if(pid!=0){
		wait();
		kthread_mutex_dealloc(mutex1);

	}
	else
		exit();

 	printf(1, "test2: 30 forks\n");
  	int i;
  

  for(i=0; i<30; i++){
    pid = fork();

    if(pid==0){
    	sleep(100-3*i);
    	test2(mutex2);
    	exit();
    }
  }
  for(i=0; i<30; i++){
  	wait();

  }
  if(pid!=0)
     kthread_mutex_dealloc(mutex2);
  exit();
}
void test1(int mx){

	kthread_mutex_lock(mx);
	printf(1,"I got the key I'm thread number: %d\n", kthread_id());
	kthread_mutex_unlock(mx);

}

void test2(int mx){

	kthread_mutex_lock(mx);
	printf(1,"test 2 pid number: %d\n", kthread_id());
	kthread_mutex_unlock(mx);

}