/***************************************************************************
 *      Organisation    : Kernel Masters, KPHB, Hyderabad, India.          *
 *      facebook page	: www.facebook.com/kernelmasters                   *
 *                                                                         *
 *  Conducting Workshops on - Embedded Linux & Device Drivers Training.    *
 *  -------------------------------------------------------------------    *
 *  Tel : 91-9949062828, Email : kernelmasters@gmail.com                   *
 *                                                                         *
 ***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation. No warranty is attached; we cannot take *
 *   responsibility for errors or fitness for use.                         *
 ***************************************************************************/

#include <linux/kernel.h> /* printk */
#include <linux/module.h>/* This Header contains the necessary stuff for the Module */
#include <linux/init.h> /* Required header for the Intialization and Cleanup Functionalities....  */
#include <linux/fs.h> /* struct file_operations, struct file and struct inode */
#include <linux/kdev_t.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define NAME "Myslpy"
//#define MAJOR 60
int data_present=0;
wait_queue_head_t my_queue;
DECLARE_WAIT_QUEUE_HEAD(my_queue);

dev_t temp;
ssize_t slpy_read(struct file *, char __user *, size_t, loff_t *);
ssize_t slpy_write(struct file *, const char __user *, size_t, loff_t *);
int slpy_open(struct inode *, struct file *);
int slpy_close(struct inode *, struct file *);

struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = slpy_read,
	.write = slpy_write,
	.open = slpy_open,
	.release = slpy_close
};


/*Device methods */
ssize_t slpy_read(struct file *filp, char __user *usr, size_t size, loff_t *off)
{
	printk("Reading from device\n");
	if(data_present == 0)
	{
		printk("Process %d (%s) Going to Sleep\n",current->pid,current->comm);
		// Data is not available
		wait_event(my_queue,(data_present==1));
		printk(KERN_INFO "Process awaken - Now Data is available\n");
		
		/*if(wait_event_interruptible(my_queue,(data_present==1)))
		{
		// error
		printk(KERN_ERR "Signal Occurs\n");
		}
		else
		{
		//success
		printk(KERN_INFO "Process awaken - Now Data is available\n");
		}*/
	}
	else
	{
	printk(KERN_INFO "Data is available\n");
	}
	return 0; //-EPERM;
}

ssize_t slpy_write(struct file *filp, const char __user *usr, size_t len, loff_t *off)
{
	printk("Trying to write into the device\n");
	data_present=1;
	wake_up(&my_queue);
	return len; //-EPERM;
}

int slpy_open(struct inode *ino, struct file *filp)
{
	printk("device opened\n");
	return 0;
}

int slpy_close(struct inode *ino, struct file *filp)
{
	printk("device closed\n");
	return 0;
}

int slpy_init(void)
{
	int result;
        result = register_chrdev(60, NAME, &fops);
	if(result < 0)
	{
		printk("Device could not be registered\n");
		return -EPERM;
	}
	printk("Driver registered with major %d\n", MAJOR(temp));
	return 0;
}


void slpy_exit(void)
{
	unregister_chrdev(60, NAME);
	printk("simple_slpy unregistered\n");
}

module_init(slpy_init);
module_exit(slpy_exit);

MODULE_LICENSE("GPL");
