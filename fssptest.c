#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){
	int i, pid;
	int times = 20;
	char *sol = "10";
	if(argc >= 3){
		sol = argv[1];
		times = atoi(argv[2]);
	}
	char * a[3] = {"fssp", sol};
	for(i = 0; i<times; i++){
		pid = fork();
		if(pid == 0){
			exec(a[0], a);
			exit();
		}
		else
			wait();
	}
	exit();
}
