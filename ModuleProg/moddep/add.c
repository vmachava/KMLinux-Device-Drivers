#include <linux/module.h> // MODULE_AUTHOR , MODULE_LICENSE, MODULE_DESCRIPTION 
#include <linux/init.h> // module_init & module_exit macro
#include <linux/kernel.h> // printk function prototype
//#include "add.h" // printk function prototype

MODULE_AUTHOR("Kernel Masters");
MODULE_DESCRIPTION("Hello Module");
MODULE_LICENSE("GPL2");


int add (int x, int y)
{
	printk ("This is add function\n");
	return x+y;
}

EXPORT_SYMBOL(add);
static int add_init(void)
{
	printk ("This is a add_init function\n");
	return 0;
}


static void add_exit(void)
{
	printk ("This is a add_exit function\n");
}
module_init(add_init);
module_exit(add_exit);
