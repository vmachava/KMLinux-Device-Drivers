#include <linux/module.h> // MODULE_AUTHOR , MODULE_LICENSE, MODULE_DESCRIPTION 
#include <linux/init.h> // module_init & module_exit macro
#include <linux/kernel.h> // printk function prototype
#include "add.h"

MODULE_AUTHOR("Kernel Masters");
MODULE_DESCRIPTION("Hello Module");
MODULE_LICENSE("GPL2");

static int avg_init(void)
{
	printk ("This is a avg_init function:\n avg result:%d \n",add(10,20)/2);
	return 0;
}


static void avg_exit(void)
{
	printk ("This is a avg_exit function\n");
}

module_init(avg_init);
module_exit(avg_exit);


