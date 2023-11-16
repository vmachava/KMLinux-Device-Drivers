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
#include <linux/netdevice.h>
#include <linux/init.h>

#define HAVE_NET_DEVICE_OPS
static struct net_device *dev;
static struct net_device_stats *stats;

static int my_hard_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
    int i;

    printk (KERN_INFO "my_hard_start_xmit(%s)\n", dev->name);

    dev->trans_start = jiffies;
    printk (KERN_INFO "Sending packet :\n");

    /* print out 16 bytes per line */
    for (i = 0; i < skb->len; ++i) {
        if ((i & 0xf) == 0)
            printk (KERN_INFO "\n  ");
        printk (KERN_INFO "%02x ", skb->data[i]);
    }
    printk (KERN_INFO "\n");

    dev_kfree_skb (skb);
    ++stats->tx_packets;

    return 0;
}
static int my_do_ioctl (struct net_device *dev, struct ifreq *ifr, int cmd)
{
    printk (KERN_INFO "my_do_ioctl(%s)\n", dev->name);
    return -1;
}
static struct net_device_stats *my_get_stats (struct net_device *dev)
{
    printk (KERN_INFO "my_get_stats(%s)\n", dev->name);
    return stats;
}

/*
 * This is where ifconfig comes down and tells us who we are, etc.
 * We can just ignore this.
 */
static int my_config (struct net_device *dev, struct ifmap *map)
{
    printk (KERN_INFO "my_config(%s)\n", dev->name);
    if (dev->flags & IFF_UP) {
        return -EBUSY;
    }
    return 0;
}
static int my_change_mtu (struct net_device *dev, int new_mtu)
{
    printk (KERN_INFO "my_change_mtu(%s)\n", dev->name);
    return -1;
}
static int my_open (struct net_device *dev)
{
    printk (KERN_INFO "Hit: my_open(%s)\n", dev->name);

    netif_start_queue (dev);
    return 0;
}
static int my_close (struct net_device *dev)
{
    printk (KERN_INFO "Hit: my_close(%s)\n", dev->name);

    netif_stop_queue (dev);
    return 0;
}

static struct net_device_ops ndo = {
    .ndo_open = my_open,
    .ndo_stop = my_close,
    .ndo_start_xmit = my_hard_start_xmit,
    .ndo_do_ioctl = my_do_ioctl,
    .ndo_get_stats = my_get_stats,
    .ndo_set_config = my_config,
    .ndo_change_mtu = my_change_mtu,
};

static void my_setup (struct net_device *dev)
{
    int j;
    printk (KERN_INFO "my_setup(%s)\n", dev->name);

    for (j = 0; j < ETH_ALEN; ++j) {
        dev->dev_addr[j] = (char)j;
    }

    ether_setup (dev);

    dev->netdev_ops = &ndo;
    dev->flags |= IFF_NOARP;
    stats = &dev->stats;

    /*
     * Just for example, let's claim that we've seen 50 collisions.
     */
    stats->collisions = 50;
}

static int __init my_init (void)
{
    printk (KERN_INFO "Loading transmitting network module:....");
    dev = alloc_netdev (0, "mynet%d", my_setup);
    if (register_netdev (dev)) {
        printk (KERN_INFO " Failed to register\n");
        free_netdev (dev);
        return -1;
    }
    printk (KERN_INFO "Succeeded in loading %s!\n\n", dev_name (&dev->dev));
    return 0;
}
static void __exit my_exit (void)
{
    printk (KERN_INFO "Unloading transmitting network module\n\n");
    unregister_netdev (dev);
    free_netdev (dev);
}

module_init (my_init);
module_exit (my_exit);

MODULE_AUTHOR ("Kernel Masters");
MODULE_DESCRIPTION ("KPHB, Hyderabad, INDIA");
MODULE_LICENSE ("GPL v2");
