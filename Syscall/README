What is systemcall?
------------------
System calls are the service points from the operating system to the user space applications. 
System call is a kernel function which is written to perform a particular task on behalf of user application. 
We can say system calls are entry points to get the operating system services.

A user space application cannot access services directly from the kernel as the kernel routines reside in kernel space and an application doesn't have permission to access the kernel space memory. Say, for example, if an application wants to write to or read from a device it has to go through the system calls provided by the Linux kernel. 

In this document we will be seeing how to implement a system call and how actually a system call works. We will take x86 architecture as an example and also give some clues about how they can be implemented in other architectures ARM.


Add hello system call in x86 architecture:
-----------------------------------------
64 bit case:
------------
Step1: 
------
In this case we need to modify only two files. Add the prototype of the system call in the syscalls.h and define our system call number in the unistd_64.h

vim include/linux/syscalls.h:
looks like this.
asmlinkage long sys_hello(void); 
asmlinkage int sys_lsproc(void);

Step2:
----
Add the syscalls in systemcall table as follows
vim arch/x86/syscalls/syscall_64.h 

312     common  kcmp                    sys_kcmp 
313     64      lsproc                  sys_lsproc 
314     64      hello                   sys_hello 

Step3:
-----
kernel/module.c

Step4:
----
Compile the Kernel:
------------------
We have added the new system call to the kernel, we need to build the kernel and use that kernel to boot the system and test the system call.
	$ make menuconfig
	$ make 
	$ make modules_install
	$ make install 

Invoking System Call:
--------------------
To invoke the system call
1.Store syscall ID(number) in the processor’s accumulator (%eax). 
2.Starting with right most argument move each argument on to other processor accumulators (ebx –eix). 
3.Generate trap exception using processor specific instruction. 
4.Read return value of the system call from eax accumulator.

Invoking of systemcall is done in 2 ways:
-----------------------------------------
Using inline assembly code:
--------------------------
Int this method, you have to know the id of the system call which you would like to invoke., we have added a system call with id: 337, and now I am trying to invoke it in this below example.
First move the id of system call to the accumulator register (eax).
Raise an interrupt, for context switch to kernel mode. Here int 0x80 raises a Trap interrupt.
Now, move the return value into the accumulator.

#include<stdio.h>
int main(){
printf(“Entered main function…!\n”);

/*here starts system call*/
__asm__(“movl $337, %eax”);
__asm__(“int $0x80”);
__asm__(“movl %eax, -4(%ebp)”);

printf(“System call invoked, to see type command: dmesg at your terminal\n”);
printf(“Exiting main….!\n);

return 0;
}



Using syscall:
-------------
In this we have to include sys/syscall.h header and the code is as follows. mycall is the name of the system call to be invoked, and it is the one we have added bellow .

Syntax: int syscall(int number, ...);

#include<stdio.h>
#inclide<sys/syscall.h>
int main(){
printf(“Entered main function…!\n”);

/*here starts system call*/
syscall(“SYS_mycall”);

printf(“System call invoked, to see type command: dmesg at your terminal\n”);
printf(“Exiting main….!\n)

return 0;
}



We also said that system _call() takes system call number as its argument. How do we send arguments to a function using assembly code? CPU general purpose registers are used to send arguments to a system calls. In x86 (32 bit), general purpose registers are named eax, ebx, ecx, etc. As we are sending only one argument, a system call number, just copy the system call number into the eax register. That is what is done in the first instruction of the modcount(). Always eax is used to pass the system call number.

With these two instructions, system_call() is invoked and system_call() finds out our system call(sys_modcount) address using given system call number as an index into sys _call_table and invokes our system call. Our system call counts the number of modules inserted and returns the number of modules. Here, the question is, how do we get the return value returned by our system call?. Intel ABI says that the return value of a function is stored in the eax register. So, by the time system_call() returns, eax register contains the return value. In the main() function we are catching the return value into a local variable count. The compiler generates instructions in such a way that eax is copied into the count variable.That's it, we have invoked the system call successfully.


How It Works?
------------

When an interrupt is raised processor puts aside the work that it is doing currently and executes a set of instructions implemented in an interrupt service routine(ISR). After completion of ISR processor resumes the task that it had put aside when the interrupt was raised. The processor receives interrupts from two sources:

External interrupts: These interrupts occur at random times during the execution of a program, in response to signals from hardware. System hardware uses interrupts to handle events external to the processor, such as requests to service peripheral devices . 

Software-generated interrupts: These interrupts are generated by software by executing the INT n instruction. The INT n instruction permits interrupts to be generated from within software by supplying an interrupt vector number as an operand. For example, the INT 35 instruction forces an implicit call to the interrupt handler for interrupt 35. 

x86 processors support 256 interrupts from 0 to 255. x86 processors look for a vector table called Interrupt descriptor table( IDT) in the memory where the addresses of the interrupt service routines(ISR) are stored by the operating system. For each interrupt an entry will be there in the interrupt descriptor table(IDT). Each of this entry is called a vector. x86 has 256 entries in the interrupt descriptor table(IDT), each entry is of size 8 bytes.

How System Calls are Executed:
-----------------------------
system calls are kernel functions. User space applications cannot access system calls directly. We need a way to switch to the kernel space and to execute the system call and get the return value of the system call. This is where software-generated interrupts are used. A vector in the interrupt descriptor table (IDT) is used to invoke the system call.

Only one vector is allocated for the system calls. But, how are more than one system calls invoked with one vector?. 

There will be a generic function (you can say an ISR) which will multiplex the all other system calls. That means when an interrupt (software interrupt using INT instruction) is raised on this vector, the generic function will be called and a system call number is passed as an argument to this function. This generic function uses this system call number as an index into the sys_call_table array and gets the address (function pointer) of the system call and invokes that system call.

Interrupt vector used for the system call is 0x80(313), interrupt descriptor table's 313th entry. 313Th entry in the IDT table is filled( in the trap_init(), arch/x86/kernel/traps.c) with the address of system_call() function. system_call() is defined in arch/x86/kernel/entry_32.S. The following figure shows how a system call is invoked.



arch/x86/kernel/traps.c
        set_system_trap_gate(SYSCALL_VECTOR, &system_call);

arch/x86/kernel/entry_32.S

syscall_call:
        call *sys_call_table(,%eax,4)      call *sys_hello
        movl %eax,PT_EAX(%esp)          # store the return value

./arch/x86/include/generated/asm/syscalls_64.h
__SYSCALL_64(313, sys_hello, sys_hello)
