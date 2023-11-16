#include<stdio.h>

void my_func1(int );//protype declaration
void my_func2(int );//protype declaration

void my_func1(int x)
{
printf ("This is my_func1 x value:%d\n",x);
}

void my_func2(int x)
{
printf ("This is my_func2 x value:%d\n",x);
}

int main ()
{
void (*my_fptr) (int );// fptr Declaration 
my_fptr = &my_func1;// fptr Initalization


my_fptr(2);

my_fptr = &my_func2;// fptr Initalization
(*my_fptr) (3);


return 0;

}
