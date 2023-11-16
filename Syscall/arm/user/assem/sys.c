#include <stdio.h>

main ()
{
asm("ldr r7,=378");
asm("mov r0, #25");
asm("mov r1, #35");
asm("SWI 0");
asm("mov r7, 1");
asm("SWI  0");
}

