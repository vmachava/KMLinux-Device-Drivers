/***************************************************************************
 *                                                                         *
 *      Author          : Kernel Masters                                   *
 *      Organisation    : KPHB, Hyderabad			           *
 *      Website		: www.kernelmasters.org                            *
 *                                                                         *
 *  Conducting Workshops on - Embedded Linux & Device Drivers Training.    *
 *  -------------------------------------------------------------------    *
 *  Tel : 91-9949062828, Email : info@kernelmasters.org                    *
 *                                                                         *
 ***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation. No warranty is attached; we cannot take *
 *   responsibility for errors or fitness for use.                         *
 ***************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>

static int __init my_init (void)
{
    struct net_device *dev;
    printk (KERN_INFO "Hello: module loaded at 0x%p\n", my_init);
    dev = first_net_device (&init_net);
    printk (KERN_INFO "Hello: dev_base address=0x%p\n", dev);
    while (dev) {
        printk (KERN_INFO
                "name = %6s irq=%4d trans_start=%12lu last_rx=%12lu\n",
                dev->name, dev->irq, dev->trans_start, dev->last_rx);
        dev = next_net_device (dev);
    }
    return 0;
}
static void __exit my_exit (void)
{
    printk (KERN_INFO "Module Unloading\n");
}

module_init (my_init);
module_exit (my_exit);

MODULE_AUTHOR ("Kernel Masters");
MODULE_DESCRIPTION ("KPHB, Hyderabad, INDIA");
MODULE_LICENSE ("GPL v2");
