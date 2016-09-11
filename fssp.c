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
static soldier *soldiers;
static int num_of_soldiers;
static int mutex;

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
		{Z, B, Z, Z, B, B}
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

typedef struct {
	int c_mutex;
	int counter;
	int limit;
} barrier;

barrier time_barrier;

void* soldier_func();
void inc_barrier(barrier* b);

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

	// initialize
	soldiers = malloc(sizeof(soldier)*num_of_soldiers);
	int i;
	for(i=0; i<num_of_soldiers; i++){
		soldiers[i].state = 0;						// initial state
		soldiers[i].stack = malloc(STK_SIZE);		// thread user stack
		soldiers[i].tid = kthread_create(soldier_func, soldiers[i].stack, STK_SIZE);
		soldiers[i].index = i;
	}
	mutex = kthread_mutex_alloc();
	time_barrier.mutex = kthread_mutex_alloc();
	time_barrier.limit = num_of_soldiers;
	time_barrier.counter = 0;
	printf(1, "num of soldiers: %d\n", num_of_soldiers);

	
	// free before exit
	//kthread_mutex_dealloc(mutex);
	for(i=0; i<num_of_soldiers; i++)
		free(soldiers[i].stack);
	free(soldiers);
	kthread_exit();
	exit();
}

void* soldier_func(){
	//printf(1, "SOLDIER FUNC\n");
	sleep(500);
	inc_barrier(&barrier); 		// wait for all threads to start

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
	kthread_mutex_lock(mutex);	
	printf(1, "tid: #%d - index: %d\n", tid, index);
	kthread_mutex_unlock(mutex);
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

		inc_barrier(&barrier);	// wait for all thread to calculate their next_state

		soldiers[index].state = next_state;

		inc_barrier(&barrier);	// wait for all thread to update their state
	}

	kthread_exit();
	return 0;
}

void inc_barrier(barrier* b){
	kthread_mutex_lock(b->mutex);
	b->counter++;
	b->counter %= b->limit;
	kthread_mutex_unlock(b->mutex);	

}