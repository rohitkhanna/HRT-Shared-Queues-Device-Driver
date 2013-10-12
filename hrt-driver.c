/* My driver  for Lab 1*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <plat/dmtimer.h>


#include "hrt-ioctl.h"
#include "hrt-driver.h"


#define DEVICE_NAME  "hrt"					 /* device would be named as /dev/gmem	*/


/* per device structure */
struct My_dev {
	struct cdev cdev;               	 	 /* The cdev structure */
	char name[20];                 			 /* Name of device*/
	struct omap_dm_timer *hr_timer;			 /* Our High Resolution Timer	*/
} *my_devp;

static dev_t my_dev_number;    		  		 /* Allotted device number */
struct class *my_dev_class;          		 /* Tie with the device model */

/*	timer variables	*/
struct clk *htr_fclk;
unsigned long htr_rate;

/*	Function Declarations 	*/
int init_hrt(struct My_dev *my_devp);

/*
 * Open My driver
 */
int My_driver_open(struct inode *inode, struct file *file){

	struct My_dev *my_devp;
	printk("\nMy_driver_open hrt()\n");
	my_devp = container_of(inode->i_cdev, struct My_dev, cdev);			/* Get the per-device structure that contains this cdev */
	file->private_data = my_devp;										/* Easy access to cmos_devp from rest of the entry points */
	printk("%s has opened\n", my_devp->name);
	return 0;
}


/*
 * Release My driver
 */
int My_driver_release(struct inode *inode, struct file *file){

	struct My_dev *my_devp = file->private_data;
	printk("\nMy_driver_release hrt() -- %s is closing\n", my_devp->name);
	return 0;
}



/*
 * Write to my driver
 */
ssize_t My_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos){

	int res=0;
	struct My_dev *my_devp = file->private_data;
	printk("My_driver_write %s()\n", my_devp->name);

	return res;
}


/*
 * Read from my driver
 */
static ssize_t My_driver_read(struct file *file, char *buf, size_t count, loff_t *ptr){

	int len;
	struct My_dev *my_devp = file->private_data;
	unsigned int counter_val = 0;
	//printk("My_driver_read %s()\n", my_devp->name);

	counter_val =  omap_dm_timer_read_counter(my_devp->hr_timer);
	len = sizeof(counter_val);
	//printk("counter_val = %u\n", counter_val);

	if (  copy_to_user(buf, &counter_val, len )  ) {
		return -EFAULT;
	}
	return len;
}


/*
 * Returns the value of counter of a dm_timer
 * -- This will be used both in this driver as well as from squeue-driver
 * 	  and hence its symbol is exported.
 */
unsigned int read_counter(void){
	return   omap_dm_timer_read_counter(my_devp->hr_timer);
}
EXPORT_SYMBOL(read_counter);



static long My_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg){

	struct My_dev *my_devp = file->private_data;
	struct omap_dm_timer *hrt;														// Temporary variable to hold our HRT
	struct clock *htr_fclk;															// timer clock
	unsigned long htr_rate;															// timer clock rate in HZ
	unsigned int user_val;

	printk("My_driver_ioctl() hrt\n");
	hrt = my_devp->hr_timer;

	switch (cmd){
		case QUERY_START_TIMER:
			printk("cmd = QUERY_START_TIMER\n");
			htr_fclk = omap_dm_timer_get_fclk(hrt);										// get the timer clock
			htr_rate = clk_get_rate(htr_fclk);											// get clock rate in HZ
			omap_dm_timer_start(hrt);													// start the timer !!!
			printk("HRT started at (%lu Hz) !!!\n", htr_rate);
			printk("Reading counter = %u\n", omap_dm_timer_read_counter(hrt));
			printk("Reading counter again = %u\n", omap_dm_timer_read_counter(hrt));
			printk("Reading counter again = %u\n", omap_dm_timer_read_counter(my_devp->hr_timer));
			break;

		case QUERY_STOP_TIMER:															// stop the timer
			printk("cmd = QUERY_STOP_TIMER\n");
			omap_dm_timer_stop(hrt);
			printk("hrt timer has been stopped\n");
			break;

		case QUERY_SET_TIMER:															//set the timer
			printk("cmd = QUERY_SET_TIMER\n");
			if (copy_from_user(&user_val, (unsigned int*)arg, sizeof(unsigned int))){
				return -EACCES;
			}
			printk("timer value received from user space = %u\n", user_val);
			omap_dm_timer_write_counter(hrt, user_val);
			printk("Reading counter = %u\n", omap_dm_timer_read_counter(hrt));
			printk("Reading counter agaun = %u\n", omap_dm_timer_read_counter(hrt));
			break;

		default:
			return -EINVAL;
	}
	return 0;
}



/*
 * File operations structure. Defined in linux/fs.h
 */
static struct file_operations My_fops = {
		.owner = THIS_MODULE,           	/* Owner */
		.open = My_driver_open,              /* Open method */
		.release = My_driver_release,        /* Release method */
		.write = My_driver_write,            /* Write method */
		.read = My_driver_read,				/* Read method */
		.unlocked_ioctl = My_driver_ioctl	/* ioctl method	*/
};


/*
 * Driver Initialization
 */
int __init My_driver_init(void)
{
	int ret;
	printk("My_driver_init() hrt\n");

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&my_dev_number, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_DEBUG "Can't register device\n"); return -1;
	}
	printk("My major number = %d, Minor number = %d\n", MAJOR(my_dev_number), MINOR(my_dev_number));

	/* Populate sysfs entries */
	my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	my_devp = kmalloc(sizeof(struct My_dev), GFP_KERNEL);
	if (!my_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(my_devp->name, DEVICE_NAME);

	/* Connect the file operations with the cdev and initialize the	 cdev structure*/
	cdev_init(&my_devp->cdev, &My_fops);
	my_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev, register operations of your driver */
	ret = cdev_add(&my_devp->cdev, (my_dev_number), 1);
	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), 0), NULL, DEVICE_NAME);	

	if(init_hrt(my_devp) == -1)
		return -1;
	else{
		printk("init_hrt() successful\n");
	}

	printk("My Driver = %s Initialized.\n", my_devp->name);
	return 0;
}


/*	Initialize the HRT timer
 * 	- is called form My_driver_init
 */
int init_hrt(struct My_dev *my_devp){

	int ret;
	printk("init_hrt()\n");

	my_devp->hr_timer = omap_dm_timer_request();												// request for a timer
	if(!(my_devp->hr_timer)){
		printk("Couldnt request dm timer - no timers available !!\n");
		return -1;
	}

	printk("timer requested successfully !\n");
	if( (ret = omap_dm_timer_set_source(my_devp->hr_timer, OMAP_TIMER_SRC_SYS_CLK )) < 0 ){		// set the clock source to System clock
		printk("Couldnt set dm timer source, ret = %d \n", ret);
		return -1;
	}

	htr_fclk = omap_dm_timer_get_fclk(my_devp->hr_timer);										// get clock rate in HZ
	htr_rate = clk_get_rate(htr_fclk);
	printk("Rate of HRT = %lu\n", htr_rate);
	printk("counter value = %u\n", omap_dm_timer_read_counter(my_devp->hr_timer) );

	return 0;
}


/*
 * Driver Exit
 */
void __exit My_driver_exit(void)
{

	/* Release the major number */
	unregister_chrdev_region((my_dev_number), 1);

	// release the timer
	omap_dm_timer_free(my_devp->hr_timer);

	/* Destroy device */
	device_destroy (my_dev_class, MKDEV(MAJOR(my_dev_number), 0));
	cdev_del(&my_devp->cdev);
	kfree(my_devp);

	/* Destroy driver_class */
	class_destroy(my_dev_class);

	printk("My Driver hrt exits.\n");
}



module_init(My_driver_init);
module_exit(My_driver_exit);
MODULE_LICENSE("GPL v2");
