#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "kthread.h"
#include "proc.h"
#include "spinlock.h"
#include "mutex.h"

static int nextmxid = 1;

struct {
  struct spinlock lock;
  kthread_mutex_t mutex[MAX_MUTEXES];
} mxtable;

void mxinit(void){
  initlock(&mxtable.lock, "mxtable");
 }

int kthread_mutex_alloc(){
	kthread_mutex_t *mx;
	acquire(&mxtable.lock);
  	for(mx = mxtable.mutex; mx < &mxtable.mutex[MAX_MUTEXES]; mx++)
  		if(mx->state == FREE)
  			goto found;
  	release(&mxtable.lock);
	return -1;

	found:
		mx->state = UNLOCKED;
		mx->mxid = nextmxid++;
		mx->owner = 0;
		//initlock(&mx->lock, "mutex");
		release(&mxtable.lock);
		return mx->mxid;
}
int kthread_mutex_dealloc(int mutex_id){
	kthread_mutex_t *mx;
	acquire(&mxtable.lock);
  	for(mx = mxtable.mutex; mx < &mxtable.mutex[MAX_MUTEXES]; mx++)
  		if(mx->mxid == mutex_id)
  			goto found;
  	release(&mxtable.lock);
	return -1;
  	
  	found:
  		if(mx->state == LOCKED){
  			release(&mxtable.lock);
  			return -1;
  		}

  		mx->state = FREE;
  		mx->owner = 0;
  		mx->mxid = 0;
  		release(&mxtable.lock);
		return 0;
}
int kthread_mutex_lock(int mutex_id){
	kthread_mutex_t *mx;
	acquire(&mxtable.lock);
  	for(mx = mxtable.mutex; mx < &mxtable.mutex[MAX_MUTEXES]; mx++)
  		if(mx->mxid == mutex_id)
  			goto found;
  	release(&mxtable.lock);
  	//panic("kthread_mutex_lock: mutex id not found");
	return -1;
  	
  	found: 
  		if(mx->state == FREE){
  			panic("kthread_mutex_lock: trying to catch FREE mutex"); 		
  		}
  		
  		while(xchg(&mx->state, LOCKED) == LOCKED){ // TODO what if FREE?
  			sleep(mx, &mxtable.lock);
  		}

  		mx->owner = thread;
  		//acquire(&mx->lock);
  	release(&mxtable.lock);

	return 0;
}
int kthread_mutex_unlock(int mutex_id){
	kthread_mutex_t *mx;
	acquire(&mxtable.lock);
  	for(mx = mxtable.mutex; mx < &mxtable.mutex[MAX_MUTEXES]; mx++)
  		if(mx->mxid == mutex_id)
  			goto found;
  	release(&mxtable.lock);
	return -1;
  	
  	found: 
  		if(mx->state == UNLOCKED || mx->owner != thread){
	  	  	release(&mxtable.lock);
			return -1;
  		}

  		mx->state = UNLOCKED;
  		mx->owner = 0;
  		release(&mxtable.lock);
  		//release(&mx->lock);
  		wakeup(mx);
	return 0;
}