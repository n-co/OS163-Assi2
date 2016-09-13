#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{

    int pid = cow_fork();
    if(pid == 0){
      printf(1, "This is child\n");
    }
    else{
      cow_wait();
      printf(1,"This is parent %d\n", getpid());
      //printf(1,"This is parent %d, his child id is %d\n", getpid(), pid);
    }
    exit();

}