/*
 * snull.h -- definitions for the network module
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
 */

/*
 * Macros to help debugging
 */

#undef PDEBUG             /* undef it, just in case */
#ifdef SNULL_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "snull:%s:%d: " fmt, __func__, __LINE__, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#if 1
/*------------------------ MSG ---------------------------------------*/
#define DRVNAME "snull"
#undef MSG             /* undef it, just in case */
#ifdef SNULL_DEBUG
  	#define MSG(string, args...) \
		printk(KERN_ALERT "%s:%s:%d : " string, DRVNAME,  __func__, __LINE__, ##args)
  	#define MSG_SHORT(string, args...) \
		printk(KERN_ALERT string, ##args)
#else
	#define MSG(string, args...)
	#define MSG_SHORT(string, args...)
#endif
/*------------------------ QP ---------------------------------------*/
#define QP MSG("\n")

/*------------------------ assert ---------------------------------------*/
#define assert(expr)                        \
    if (!(expr)) {                      \
        printk("%s: Assertion failed! [%s] : %s:%s:%d\n",  \
               DRVNAME, #expr, __FILE__, __func__, __LINE__);    \
    }

#endif


/* These are the flags in the statusword */
#define SNULL_RX_INTR 0x0001
#define SNULL_TX_INTR 0x0002

/* Default timeout period */
#define SNULL_TIMEOUT 5   /* In jiffies */

extern struct net_device *snull_devs[];

