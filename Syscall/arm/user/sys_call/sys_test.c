#include <stdio.h> 
#include <linux/kernel.h> 
#include <sys/syscall.h> 
#include <unistd.h> 
#define __NR_sys_add 378 

long add_syscall(void) 
{ 
return syscall(__NR_sys_add,25,35); 
} 


int main(int argc, char *argv[]) 
{ 
long int a = add_syscall(); 
printf("System call invoked\n");
printf("system call returned %ld\n",a); 
return 0; 

}



