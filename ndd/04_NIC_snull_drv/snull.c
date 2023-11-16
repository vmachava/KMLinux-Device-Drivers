/*
 * snull.c --  the Simple Network Utility
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: snull.c,v 1.21 2004/11/05 02:36:03 rubini Exp $
 *
 * o ported to work on recent kernels; tested on 2.6.35
 * o ping'ing the interface did not work straight off; modified slightly
 * 
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h>    /* printk() */
#include <linux/slab.h>      /* kmalloc() */
#include <linux/errno.h>     /* error codes */
#include <linux/types.h>     /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>
#include <linux/icmp.h>        /* ICMP proto types */

#include "snull.h"

#include <linux/in6.h>
#include <asm/checksum.h>

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");


/*
 * Transmitter lockup simulation, normally disabled.
 */
static int lockup = 0;
module_param(lockup, int, 0);

static int timeout = SNULL_TIMEOUT;
module_param(timeout, int, 0);

/*
 * Do we run in NAPI mode?
 */
static int use_napi = 0;
module_param(use_napi, int, 0);


/*
 * A structure representing an in-flight packet.
 */
struct snull_packet {
	struct snull_packet *next;
	struct net_device *dev;
	int	datalen;
	u8 data[ETH_DATA_LEN];
};

static int pool_size = 8;
module_param(pool_size, int, 0);

/*
 * This structure is private to each device. It is used to pass
 * packets in and out, so there is place for a packet
 */
struct snull_priv {
	struct net_device_stats stats;
	int status;
	struct snull_packet *ppool;
	struct snull_packet *rx_queue;  /* List of incoming packets */
	int rx_int_enabled;
	int tx_packetlen;
	u8 *tx_packetdata;
	struct sk_buff *skb;
	spinlock_t lock;
	struct napi_struct *pstNapi;
};

static void snull_tx_timeout(struct net_device *dev);
static void (*snull_interrupt)(int, void *);

/*
 * Set up a device's packet pool.
 */
static void snull_setup_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
	int i;

	assert (priv != NULL);

	// The debug print below shows the 2 net devices & their private pointers
	MSG("netdev = %08lx priv=%08lx\n", dev, priv);

	priv->ppool = NULL;
	for (i = 0; i < pool_size; i++) {
		pkt = kmalloc (sizeof (struct snull_packet), GFP_KERNEL);
		if (NULL == pkt) {
			printk (KERN_NOTICE "%s: Ran out of memory allocating packet pool\n", DRVNAME);
			return;
		}
		pkt->dev = dev;
		pkt->next = priv->ppool;
		priv->ppool = pkt;
#if 0   // enable to see the linked list of buffer pool
		MSG("pkt=%08lx pkt->next=%08lx priv->ppool=%08lx\n",
			pkt, pkt->next, priv->ppool);
#endif
	}
}

static void snull_teardown_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
    
	while ((pkt = priv->ppool)) {
		priv->ppool = pkt->next;
		kfree (pkt);
		/* FIXME - in-flight packets ? */
	}
}    

/*
 * Buffer/pool management.
 */
static struct snull_packet *snull_get_tx_buffer(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	unsigned long flags;
	struct snull_packet *pkt;
    
	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->ppool;
	priv->ppool = pkt->next;
	if (priv->ppool == NULL) {
		printk (KERN_INFO "%s: Pool empty\n", DRVNAME);
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}


static void snull_release_buffer(struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(pkt->dev);
	
	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->ppool;
	priv->ppool = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
	if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
		netif_wake_queue(pkt->dev);
}

static void snull_enqueue_buf(struct net_device *dev, struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);

	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->rx_queue;  /* FIXME - misorders packets */
	priv->rx_queue = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
}

static struct snull_packet *snull_dequeue_buf(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->rx_queue;
	if (pkt != NULL)
		priv->rx_queue = pkt->next;
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

/*
 * Enable and disable receive interrupts.
 */
static void snull_rx_ints(struct net_device *dev, int enable)
{
	struct snull_priv *priv = netdev_priv(dev);
	priv->rx_int_enabled = enable;
}

    
/*
 * Open and close
 */

static int snull_open(struct net_device *dev)
{
	/* request_region(), request_irq(), ....  (like fops->open) */

	/* 
	 * Assign the hardware address of the board: use "\0SNULx", where
	 * x is 0 or 1. The first byte is '\0' to avoid being a multicast
	 * address (the first byte of multicast addrs is odd).
	 */
	memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);
	if (dev == snull_devs[1])
		dev->dev_addr[ETH_ALEN-1]++; /* \0SNUL1 */
	netif_start_queue(dev);
	return 0;
}

static int snull_release(struct net_device *dev)
{
    /* release ports, irq and such -- like fops->close */

	netif_stop_queue(dev); /* can't transmit any more */
	return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
static int snull_config(struct net_device *dev, struct ifmap *map)
{
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "%s: Can't change I/O address\n", DRVNAME);
		return -EOPNOTSUPP;
	}

	/* Allow changing the IRQ */
	if (map->irq != dev->irq) {
		dev->irq = map->irq;
        	/* request_irq() is delayed to open-time */
	}

	/* ignore other fields */
	return 0;
}

/*
 * Receive a packet: retrieve, encapsulate and pass over to upper levels.
 * Called with the spinlock held.
 */
static void snull_rx(struct net_device *dev, struct snull_packet *pkt)
{
	struct sk_buff *skb;
	struct snull_priv *priv = netdev_priv(dev);
	struct icmphdr *ich = NULL;

QP;
	/*
	 * The packet has been retrieved from the transmission
	 * medium. Build an skb around it, so upper layers can handle it
	 */
	skb = dev_alloc_skb(pkt->datalen + 2);
	if (!skb) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "%s rx: low on mem - packet dropped\n", DRVNAME);
		priv->stats.rx_dropped++;
		goto out;
	}
	skb_reserve(skb, 2); /* align IP on 16B boundary */  
	/* In order to insert the rx packet data at the end of the skb payload, 
	   first make space in the SKB payload region, by increasing skb->tail 
	   with the skb_put(); then, we copy in the packet data (a real NIC 
	   driver would probably use DMA).
	  */
	memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

	/* Write metadata, and then pass to the receive level */
	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += pkt->datalen;

	/* By now, skb->data points to the beginning of the IP header
	   (the ETH header is "stripped off"). */
#if 0
	print_hex_dump_bytes ("", DUMP_PREFIX_OFFSET, skb->data, skb->len);
#endif

	ich = (struct icmphdr *)(skb->data+sizeof(struct iphdr));

	// Is this really an ICMP packet?
	// The byte following ICMP protocol Type is Code which should be 0
	if (((ich->type == ICMP_ECHO) || (ich->type == ICMP_ECHOREPLY)) && (ich->code == 0x0)) { // ICMP ping
		if (ich->type == ICMP_ECHO)
			MSG("Rx:[ping] ICMP ECHO request packet\n");
		else if (ich->type == ICMP_ECHOREPLY)
			MSG("Rx:[ping] ICMP ECHOREPLY reply packet\n");

		print_hex_dump_bytes ("", DUMP_PREFIX_OFFSET, skb->data, skb->len);
		/* Convert the ICMP_ECHO request packet to an ICMP_ECHOREPLY reply packet and
		 * send it up the protocol stack. Will appear just as though the ping worked in
		 * the usual way!
		 */
		if (ich->type == ICMP_ECHO) {
			ich->type=ICMP_ECHOREPLY;
			// FIXME- checksum reported as wrong by wireshark
			//ich->checksum += 0x800;
			skb->data[24] += 0x8; // checksum . ping Works!
			//MSG("icmp type = x%x imcp cksum=x%04x id=%x\n", 
			// ich->type, ntohs(ich->checksum), ntohs(ich->un.echo.id));
			MSG ("Rx: converted to ICMP ECHOREPLY and being sent up!\n");
		}
	}

	if (netif_rx(skb) == NET_RX_DROP) {
		printk (KERN_ALERT "%s:%s: ALERT! [Rx path] congestion:packet dropped!\n", DRVNAME, __func__);
	}
  out:
	return;
}
    

/*
 * The poll implementation.
 */
static int snull_poll(struct napi_struct *napi, int budget)
{
	int npackets = 0, quota = budget;
	struct sk_buff *skb;
	struct net_device *dev = napi->dev;
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
    
QP;
	while (npackets < quota && priv->rx_queue) {
		pkt = snull_dequeue_buf(dev);
		skb = dev_alloc_skb(pkt->datalen + 2);
		if (! skb) {
			if (printk_ratelimit())
				printk(KERN_NOTICE "%s: packet dropped\n", DRVNAME);
			priv->stats.rx_dropped++;
			snull_release_buffer(pkt);
			continue;
		}
		skb_reserve(skb, 2); /* align IP on 16B boundary */  
	/* In order to insert the rx packet data at the end of the skb payload, 
	   first make space in the SKB payload region, by increasing skb->tail 
	   with the skb_put(); then, we copy in the packet data (a real NIC 
	   driver would probably use DMA).
	  */
		memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
		netif_receive_skb(skb);
		
        	/* Maintain stats */
		npackets++;
		priv->stats.rx_packets++;
		priv->stats.rx_bytes += pkt->datalen;
		snull_release_buffer(pkt);
	}

	/* If we processed all packets, we're done; tell the kernel and reenable ints */
	quota -= npackets;
	if (! priv->rx_queue) {
		//netif_rx_complete(dev, priv->pstNapi);
		napi_disable (priv->pstNapi);
		snull_rx_ints(dev, 1);
		return 0;
	}
	/* We couldn't process everything. */
	return 1;
}


/*
 * The typical interrupt entry point
 */
static void snull_regular_interrupt(int irq, void *dev_id)
{
	int statusword;
	struct snull_priv *priv;
	struct snull_packet *pkt = NULL;
	/*
	 * As usual, check the "device" pointer to be sure it is
	 * really interrupting.
	 * Then assign "struct device *dev"
	 */
	struct net_device *dev = (struct net_device *)dev_id;
	/* ... and check with hw if it's really ours */

//QP;
	/* paranoid */
	if (!dev)
		return;

	/* Lock the device */
	priv = netdev_priv(dev);
	assert (priv != NULL);
	spin_lock(&priv->lock);

	/* retrieve statusword: real netdevices use I/O instructions */
	statusword = priv->status;
	priv->status = 0;
	if (statusword & SNULL_RX_INTR) {
		/* send it to snull_rx for handling */
		pkt = priv->rx_queue;
		if (pkt) {
			priv->rx_queue = pkt->next;
			MSG("Rx path: Invoking snull_rx now ...\n");
			snull_rx(dev, pkt);
		}
	}
	if (statusword & SNULL_TX_INTR) {
		/* a transmission is over: free the skb */
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += priv->tx_packetlen;
		dev_kfree_skb(priv->skb);
		MSG("Tx path: Tx done, skb freed.\n");
	}

	/* Unlock the device and we are done */
	spin_unlock(&priv->lock);
	if (pkt) 
		snull_release_buffer(pkt); /* Do this outside the lock! */
	return;
}

/*
 * A NAPI interrupt handler.
 */
static void snull_napi_interrupt(int irq, void *dev_id)
{
	int statusword;
	struct snull_priv *priv;

	/*
	 * As usual, check the "device" pointer for shared handlers.
	 * Then assign "struct device *dev"
	 */
	struct net_device *dev = (struct net_device *)dev_id;
	/* ... and check with hw if it's really ours */

QP;
	/* paranoid */
	if (!dev)
		return;

	/* Lock the device */
	priv = netdev_priv(dev);
	spin_lock(&priv->lock);

	/* retrieve statusword: real netdevices use I/O instructions */
	statusword = priv->status;
	priv->status = 0;
	if (statusword & SNULL_RX_INTR) {
		snull_rx_ints(dev, 0);  /* Disable further interrupts */
		// Turn on (NAPI) polling...
//		netif_rx_schedule(dev, priv->pstNapi); // ??
	}
	if (statusword & SNULL_TX_INTR) {
        	/* a transmission is over: free the skb */
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += priv->tx_packetlen;
		dev_kfree_skb(priv->skb);
	}

	/* Unlock the device and we are done */
	spin_unlock(&priv->lock);
	return;
}


/*
 * Transmit a packet (low level interface).
 *
 * This function deals with hw details. This interface loops
 * back the packet to the other snull interface (if any).
 * In other words, this function implements the snull behaviour,
 * while all other procedures are rather device-independent.
 */
static void snull_hw_tx(char *buf, int len, struct net_device *dev)
{
	struct iphdr *ih;
	struct net_device *dest;
	struct snull_priv *priv;
	u32 *saddr, *daddr;
	struct snull_packet *tx_buffer;
	u8 prot = 0xff; //buf[14+20]; // first byte after the Eth and IP headers is the protocol type
    
	/* I am paranoid. Ain't I? */
	if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
		printk("%s: Hmm... packet too short (%i octets)\n",
				DRVNAME, len);
		return;
	}

#if 0
    //  void print_hex_dump_bytes(const char *prefix_str, int prefix_type, const void *buf, size_t len)
	print_hex_dump_bytes ("", DUMP_PREFIX_OFFSET, buf, len);
#endif

	if (0) { /* enable this conditional to look at the data */
		int i;
		PDEBUG("len is %i\n" KERN_DEBUG "data:",len);
		for (i=14 ; i<len; i++) {
			printk(" %02x",buf[i]&0xff);
			if (i == 19) printk("\n");
			if (i == (19+8)) printk("\n"); // ping case: ICMP metadata is 8 octets
		}
		printk("\n");
	}
	/*
	 * Ethhdr is 14 bytes, but the kernel arranges for iphdr
	 * to be aligned (i.e., ethhdr is unaligned)
	 */
	ih = (struct iphdr *)(buf+sizeof(struct ethhdr));
	saddr = &ih->saddr;
	daddr = &ih->daddr;

	MSG("len is %d octets.\n", len);
	// Is this really an ICMP packet?
	// [??] byte following ICMP protocol Type is Code which should be 0 [??]
	if ((buf[14+20] == ICMP_ECHO) && (buf[14+20+1] == 0x0))
		prot = ICMP_ECHO;
	else if ((buf[14+20] == ICMP_ECHOREPLY) && (buf[14+20+1] == 0x0))
		prot = ICMP_ECHOREPLY;

	/* 
	 * We're skipping the LDD3-work of toggling the third octet in this ver;
	 * we're only really concerned with getting the 'ping' semantics working right. 
	 */
	if ((prot != ICMP_ECHO) && (prot != ICMP_ECHOREPLY)) { // not an ICMP echo (ping) request/reply
#if 0
		((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
		((u8 *)daddr)[2] ^= 1;
#endif
		MSG("NOT an ICMP packet; arranging for Rx on twin device...\n");
	} else { // we're in a 'ping'-driven Tx
		u32 tmp = ih->saddr;
		if (prot == ICMP_ECHO)
			MSG("Tx:[ping] ICMP ECHO request packet\n");
		else if (prot == ICMP_ECHOREPLY)
			MSG("Tx:[ping] ICMP ECHOREPLY reply packet\n");

		// Interchange Src & Dest IP addresses
		ih->saddr = ih->daddr;
		ih->daddr = tmp;
		//MSG("src=%08x dest=%08x\n", ntohl(ih->saddr), ntohl(ih->daddr));

		// Set ICMP identifier, byte[x26,x27]
		buf[0x26] -= 0x8;
	}

	ih->check = 0;         /* and rebuild the checksum (ip needs it) */
	ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);

	if (dev == snull_devs[0])
		PDEBUG("%08x:%05i --> %08x:%05i\n",
				ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source),
				ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest));
	else
		PDEBUG("%08x:%05i <-- %08x:%05i\n",
				ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest),
				ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source));

	/*
	 * Ok, now the packet is ready for transmission: first simulate a
	 * receive interrupt on the twin device, then a transmission-done on 
     * the transmitting device.
	 */
	if ((prot == ICMP_ECHO) || (prot == ICMP_ECHOREPLY)) // an ICMP echo (ping) request/reply
		dest = snull_devs[dev == snull_devs[0] ? 0 : 1]; // Rx intr on same interface
	else
		dest = snull_devs[dev == snull_devs[0] ? 1 : 0]; // Rx intr on twin interface

	priv = netdev_priv(dest);
	tx_buffer = snull_get_tx_buffer(dev);
	tx_buffer->datalen = len;
	memcpy(tx_buffer->data, buf, len);
	snull_enqueue_buf(dest, tx_buffer);
	if (priv->rx_int_enabled) {
		priv->status |= SNULL_RX_INTR;
		MSG("Simulating Rx interrupt now...\n");
		snull_interrupt(0, dest); // simulate Rx interrupt
	}

	priv = netdev_priv(dev);
	priv->tx_packetlen = len;
	priv->tx_packetdata = buf;
	priv->status |= SNULL_TX_INTR;
	if (lockup && ((priv->stats.tx_packets + 1) % lockup) == 0) {
        	/* Simulate a dropped transmit interrupt */
		netif_stop_queue(dev);
		PDEBUG("Simulate lockup at %ld, txp %ld\n", jiffies,
				(unsigned long) priv->stats.tx_packets);
	}
	else {
		MSG("Simulating Tx done interrupt now...\n");
		snull_interrupt(0, dev); // simulate Tx done interrupt
	//	dump_stack();
	}
}

/*
 * Transmit a packet (called by the kernel)
 */
static int snull_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	char *data, shortpkt[ETH_ZLEN];
	struct snull_priv *priv = netdev_priv(dev);
	unsigned long flags;

#ifdef SNULL_DEBUG
	printk("\n--------------------------------------------------------------\n");
#endif
QP;
	data = skb->data;
	len = skb->len;
	if (len < ETH_ZLEN) {
		memset(shortpkt, 0, ETH_ZLEN);
		memcpy(shortpkt, skb->data, skb->len);
		len = ETH_ZLEN;
		data = shortpkt;
	}
	dev->trans_start = jiffies; /* save the timestamp */

	/* Remember the skb, so we can free it at interrupt time */
	spin_lock_irqsave(&priv->lock, flags);
	priv->skb = skb;
	spin_unlock_irqrestore(&priv->lock, flags);

	/* actual deliver of data is device-specific, and not shown here */
	snull_hw_tx(data, len, dev);

	return 0; /* Our simple device can not fail */
}

/*
 * Deal with a transmit timeout.
 */
static void snull_tx_timeout (struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);

	PDEBUG("Transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - dev->trans_start);
        /* Simulate a transmission interrupt to get things moving */
	priv->status = SNULL_TX_INTR;
	snull_interrupt(0, dev);
	priv->stats.tx_errors++;
	netif_wake_queue(dev);
	return;
}



/*
 * Ioctl commands 
 */
static int snull_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	PDEBUGG("ioctl\n");
	return 0;
}

/*
 * Return statistics to the caller
 */
static struct net_device_stats *snull_stats(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	return &priv->stats;
}

#if 0
/*
 * This function is called to fill up an eth header, since arp is not
 * available on the interface
 */
int snull_rebuild_header(struct sk_buff *skb)
{
	struct ethhdr *eth = (struct ethhdr *) skb->data;
	struct net_device *dev = skb->dev;
    
	memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
	return 0;
}


int snull_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, void *daddr, void *saddr,
                unsigned int len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	eth->h_proto = htons(type);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
	return (dev->hard_header_len);
}
#endif




/*
 * The "change_mtu" method is usually not needed.
 * If you need it, it must be like this.
 */
static int snull_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);
	spinlock_t *lock = &priv->lock;
    
	/* check ranges */
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;
	/*
	 * Do anything you need, and the accept the value
	 */
	spin_lock_irqsave(lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(lock, flags);
	return 0; /* success */
}


static const struct net_device_ops snull_netdev_ops = {
	.ndo_open            = snull_open,
	.ndo_stop            = snull_release,
	.ndo_set_config      = snull_config,
	.ndo_start_xmit      = snull_tx,
	.ndo_do_ioctl        = snull_ioctl,
	.ndo_get_stats       = snull_stats,
	.ndo_change_mtu      = snull_change_mtu,  
//	.ndo_rebuild_header  = snull_rebuild_header,
//	.ndo_hard_header     = snull_header,
	.ndo_tx_timeout      = snull_tx_timeout,
};

/*
 * The init function (sometimes called probe).
 * It is invoked by register_netdev()
 */
static void snull_init(struct net_device *dev)
{
	struct snull_priv *priv;
   	/*
	 * Make the usual checks: check_region(), probe irq, ...  -ENODEV
	 * should be returned if no device found.  No resource should be
	 * grabbed: this is done on open(). 
	 */

   	/* 
	 * Then, assign other fields in dev, using ether_setup() and some
	 * hand assignments
	 */
	ether_setup(dev); /* assign some of the fields */

	dev->netdev_ops = &snull_netdev_ops;
	dev->watchdog_timeo = timeout;
	/* keep the default flags, just add NOARP */
	dev->flags           |= IFF_NOARP;
/* TODO : if ARP is overriden, we need the 'rebuild header' / 'snull_header' code to execute ?*/
//	dev->features        |= NETIF_F_NO_CSUM;
//	dev->hard_header_cache = NULL;      /* Disable caching */

	/*
	 * Then, initialize the priv field. This encloses the statistics
	 * and a few private fields.
	 */
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct snull_priv));
	spin_lock_init(&priv->lock);
	if (use_napi) {
	/* The last param is 'budget': specifies how many packets the driver is allowed 
	 to pass into the network stack on this call. There is no need to manage separate 
	 quota fields anymore; drivers should simply respect budget and return the number 
	 of packets which were actually processed. 
	 Source: 'Newer, newer NAPI' : http://lwn.net/Articles/244640/
	*/
		netif_napi_add (dev, priv->pstNapi, snull_poll, 2);
	}
	snull_rx_ints(dev, 1);		/* enable receive interrupts */
	snull_setup_pool(dev);
}

/*
 * The devices
 */

struct net_device *snull_devs[2];

/*
 * Finally, the module stuff
 */

static void snull_cleanup(void)
{
	int i;
    
	for (i = 0; i < 2;  i++) {
		if (snull_devs[i]) {
			unregister_netdev(snull_devs[i]);
			snull_teardown_pool(snull_devs[i]);
			free_netdev(snull_devs[i]);
		}
	}
	printk ("%s: unregistered.\n", DRVNAME);
	return;
}

static int snull_init_module(void)
{
	int result, i, ret = -ENOMEM;

	snull_interrupt = use_napi ? snull_napi_interrupt : snull_regular_interrupt;

	/* Allocate the devices:
	#define alloc_netdev(sizeof_priv, name, setup) \
        alloc_netdev_mq(sizeof_priv, name, setup, 1)
	In alloc_netdev_mq():
	@setup:         callback to initialize device
	*/
	snull_devs[0] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
			snull_init);
	snull_devs[1] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
			snull_init);
	if (snull_devs[0] == NULL || snull_devs[1] == NULL)
		goto out;

	ret = -ENODEV;
	for (i = 0; i < 2;  i++)
		if ((result = register_netdev(snull_devs[i])))
			printk("%s: error %i registering device \"%s\"\n",
					DRVNAME, result, snull_devs[i]->name);
		else
			ret = 0;
   out:
	if (ret) 
		snull_cleanup();
	return ret;
}

module_init(snull_init_module);
module_exit(snull_cleanup);
