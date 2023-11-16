#include<stdio.h>
#include<sys/syscall.h>
#include<unistd.h>

int main(){
int ret;
printf("Entered main function…!\n");
/*here starts system call*/
ret = syscall(333);
//ret = syscall(39);
//ret = syscall(311);
printf("ret: %d\n",ret);
return 0;
}
