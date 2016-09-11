#ifndef MUTEX_H
#define MUTEX_H

#define MAX_MUTEXES 64

enum mxstate { FREE, LOCKED, UNLOCKED };

typedef struct {
  enum mxstate state;     	// mutex state
  int mxid;					// mutex ID
  struct thread *owner;		// owner thread
  //think about waiting threads
  //struct spinlock lock;
} kthread_mutex_t;


// API
void mxinit(void);
int kthread_mutex_alloc();
int kthread_mutex_dealloc( int mutex_id );
int kthread_mutex_lock( int mutex_id );
int kthread_mutex_unlock( int mutex_id );

#endif