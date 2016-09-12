#include "types.h"
#include "stat.h"
#include "user.h"

#define STK_SIZE 2048

typedef struct{
	int state;
	int tid;
	int index;
	void* stack;
} soldier;

typedef struct {
	int pre_mx; 	// mutex of eneting the barrier
	int post_mx;	// mutex of exiting the barrier
	int counter;
	int limit;
} barrier;

barrier time_barrier;
static soldier *soldiers;
static int num_of_soldiers;
static int print_mx;

//states
#define NUM_OF_STATES 6
#define B 	-1 // blank/dont-care
#define Q	0
#define P	1
#define R	2
#define Z	3
#define M	4
#define X	5	// no neigbour
#define F 	10	// fire
#define GENERAL soldiers[0]

//					 [curr_state]     [left_state]   [right_state]
static int trans_func[NUM_OF_STATES-1][NUM_OF_STATES][NUM_OF_STATES] =
{
	// Q
	{
		{Q, P, Q, Q, B, Q},
		{P, P, B, B, B, P},
		{Q, B, Q, B, B, B},
		{Q, B, B, Q, B, Q},
		{B, B, B, B, B, B},
		{Q, P, Q, Q, B, B}
	},

	// P
	{
		{Z, Z, R, R, B, B},
		{Z, B, Z, Z, B, B},
		{R, Z, Z, B, B, Z},
		{R, Z, B, Z, B, Z},
		{B, B, B, B, B, B},
		{Z, B, Z, Z, B, F}
	},

	// R
	{
		{B, B, R ,P, Z, B},
		{B, B, M, R, M, B},
		{R, M, B, B, M, B},
		{P, R, B, B, R, B},
		{Z, M, M, R, M, B},
		{B, B, B, B, B, B}
	},

	// Z
	{
		{B, B, Q, P, Q, B},
		{B, Z, B, Z, B, B},
		{Q, B, Q, Q, B, Q},
		{P, Z, Q, F, Q, F},
		{Q, B, B, Q, Q, Q},
		{B, Z, Q, F, Q, B}
	},

	// M
	{
		{B, B, B, B, B, B},
		{B, B, B, B, B, B},
		{B, B, R, Z, B, B},
		{B, B, Z, B, B, B},
		{B, B, B, B, B, B},
		{B, B, B, B, B, B}
	}
};

void* soldier_func();
void inc_barrier(barrier* b);
void init_barrier(barrier *b, int limit);
void kill_barrier(barrier *b);
void print_soldiers_state();

int main(int argc, char *argv[]){
	// get arguments
	if(argc <= 1){
		printf(2,"ERROR: not enough arguments\n");
		exit();
	}
	num_of_soldiers = atoi(argv[1]);
	if(num_of_soldiers <= 0 || 15 < num_of_soldiers){
		printf(2,"ERROR: Illegal input\n");
		exit();		
	}

	print_mx = kthread_mutex_alloc();
	kthread_mutex_lock(print_mx);
	printf(1, "num of soldiers: %d\n", num_of_soldiers);
	kthread_mutex_unlock(print_mx);

	// initialize
	soldiers = malloc(sizeof(soldier)*num_of_soldiers);

	init_barrier(&time_barrier, num_of_soldiers+1);	
	int i;
	for(i=0; i<num_of_soldiers; i++){
		soldiers[i].state = Q;						// initial state
		soldiers[i].stack = malloc(STK_SIZE);		// thread user stack
		soldiers[i].tid = kthread_create(soldier_func, soldiers[i].stack, STK_SIZE);
		soldiers[i].index = i;
	}
	GENERAL.state = P;
	print_soldiers_state(); 			// first print
	inc_barrier(&time_barrier);			// wait for all threads to start
	while(GENERAL.state != F){
		inc_barrier(&time_barrier);		// wait for all thread to calculate their next_state
		inc_barrier(&time_barrier);		// wait for all thread to update their next_state
		print_soldiers_state();			// print after change
	}


	
	// free before exit
	for(i=0; i<num_of_soldiers; i++){
		kthread_join(soldiers[i].tid);
		free(soldiers[i].stack);
	}
	kill_barrier(&time_barrier);
	kthread_mutex_dealloc(print_mx);
	free(soldiers);
	kthread_exit();
	exit();
}

void* soldier_func(){

	inc_barrier(&time_barrier); 		// wait for all threads to start

	// find the index according to thread_id
	int index=-1, i;
	int tid = kthread_id();
	for(i=0; i<num_of_soldiers; i++){
		if(soldiers[i].tid == tid){
			index = i;
			break;
		}
	}
	if(index < 0){
		printf(1, "ERROR: worng index\n");
		kthread_exit();
	}

	// function body goes here

	while(soldiers[index].state != F){
		int curr_state = soldiers[index].state;
		int next_state;
		int left_state;
		int right_state;
		if(index == 0)
			left_state = X;
		else
			left_state = soldiers[index-1].state;
		if(index == num_of_soldiers-1)
			right_state = X;
		else
			right_state = soldiers[index+1].state;

		next_state = trans_func[curr_state][left_state][right_state];

		inc_barrier(&time_barrier);	// wait for all thread to calculate their next_state

		soldiers[index].state = next_state;

		inc_barrier(&time_barrier);	// wait for all thread to update their state
	}

	kthread_exit();
	return 0;
}
void init_barrier(barrier *b, int limit){
	b->pre_mx = kthread_mutex_alloc();
	b->post_mx = kthread_mutex_alloc();
	b->limit = limit;
	b->counter = 0;
	kthread_mutex_lock(b->post_mx);	
}

void kill_barrier(barrier *b){
	kthread_mutex_dealloc(b->pre_mx);
	kthread_mutex_dealloc(b->post_mx);
	b->limit = 0;
	b->counter = 0;
}

void inc_barrier(barrier* b){
	kthread_mutex_lock(b->pre_mx);
	b->counter++;
	if(b->limit > b->counter)
		kthread_mutex_unlock(b->pre_mx);	
	else // limit == counter
		kthread_mutex_unlock(b->post_mx);			
	
	kthread_mutex_lock(b->post_mx);
	b->counter--;
	if(b->counter > 0)
		kthread_mutex_unlock(b->post_mx);
	else // counter == 0
		kthread_mutex_unlock(b->pre_mx);
}

void print_soldiers_state(){
	kthread_mutex_lock(print_mx);
	int i;
	for(i = 0; i < num_of_soldiers; i++){
		switch(soldiers[i].state){
			case Q:  printf(1,"Q"); break;
			case P:  printf(1,"P"); break;
			case R:  printf(1,"R"); break;
			case Z:  printf(1,"Z"); break;
			case M:  printf(1,"M"); break;
			case F:  printf(1,"F"); break;
			default: printf(1," "); break;
		}
	}
	printf(1,"\n");
	kthread_mutex_unlock(print_mx);
}