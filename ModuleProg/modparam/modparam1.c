#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/moduleparam.h>

MODULE_DESCRIPTION("This is a sample module parameter example");
MODULE_AUTHOR("Kernel masters");


int myint =10;
module_param(myint,int,0);

static int modparam_init (void)
{
	printk ("This is modparam init function myint:%d\n",myint);
	return 0;
}


static void modparam_exit (void)
{
	printk ("This is modparam exit function\n");
}


module_init(modparam_init);
module_exit(modparam_exit);


