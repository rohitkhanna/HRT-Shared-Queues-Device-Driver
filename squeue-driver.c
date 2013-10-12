/*
 * squeue-tester.c
 *
 *  Created on: Oct 9, 2013
 *      Author: rohit
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include <linux/semaphore.h>										//To use semaphores
#include "cb.h"
#include "hrt-driver.h"

#define DEVICE_NAME  "squeue"

#define DEVICE_NAME1  "squeue1"					 /* device file would be named as /dev/squeue1	*/
#define DEVICE_NAME2  "squeue2"					 /* device file would be named as /dev/squeue2	*/

/* per device structure */
struct My_dev {
	struct cdev cdev;               	 	 	/* The cdev structure */
	char name[20];                 			 	/* Name of device*/
	circular_buffer *cb;
	struct semaphore mutex;					/* SEMAPHORE per device */

} *my_devp1, *my_devp2;

static dev_t my_dev_number;    		  		 /* Allotted device number */
struct class *my_dev_class;          		 /* Tie with the device model */


/*
 * Open My driver
 */
int My_driver_open(struct inode *inode, struct file *file){

	struct My_dev *my_devp;
	printk("\nMy_driver_open squeue()\n");
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
	printk("\nMy_driver_release squeue() -- %s is closing\n", my_devp->name);
	return 0;
}



/*
 * Write to my driver
 *
 * returns -1 if not able to add token to the circular buffer or returns the index of the circular buffer where
 * 		it was added
 */
ssize_t My_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos){

	int res;
	int ret;
	token *user_token;
	struct My_dev *my_devp = file->private_data;
	down_interruptible(&(my_devp->mutex));								// CRTICAL REGION BEGINS
	printk("My_driver_write %s()\n", my_devp->name);

	user_token = kmalloc(sizeof(token), GFP_KERNEL);
	if (!user_token) {
		printk("Bad Kmalloc user_token\n");
		up(&(my_devp->mutex));												// CRTICAL REGION ENDS
		return -ENOMEM;
	}

	res = copy_from_user((void *)user_token, (void __user *)buf, count);
	if(res){
		up(&(my_devp->mutex));												// CRTICAL REGION ENDS
		return -EFAULT;
	}

	printk("\nToken of len=%d received  from User Space with token_id = |%d|, ts1 = %u, \nstring=<<%s>>, len=%d\n",count, user_token->id,
			user_token->ts1, user_token->string, strlen(user_token->string) );
	if(strlen(user_token->string)>80)									// Wont Happen but just in case
		user_token->string[79]='\0';

	user_token->ts2 = read_counter();
	printk("ts2 = %u added to token\n", user_token->ts2);

	ret=write_cb(my_devp->cb, user_token);							// add to the circular buffer
	if( (ret) == -1 ){												// -1 shows buffer is full
		printk("Buffer is full\n");
	}
	else{															// else ret contains index where its added
		printk("token with token_id=%d, added at position %d\n", user_token->id, ret);
	}
	up(&(my_devp->mutex));												// CRTICAL REGION ENDS
	return ret;														// return -1 or index
}


/*
 * Read from my driver
 *
 * 	Tries to dequeue the circular buffer :-
 * 		if successful - then add TS3 to dequeued token, copy token to user space and return dequeued index
 * 		else return -1 to user space.
 */
static ssize_t My_driver_read(struct file *file, char *buf, size_t count, loff_t *ptr){

	int ret;
	struct My_dev *my_devp = file->private_data;
	unsigned int counter_val;
	token *tok;
	down_interruptible(&(my_devp->mutex));									// CRTICAL REGION BEGINS

	counter_val = 0;
	tok = kmalloc(sizeof(token), GFP_KERNEL);
	if (!tok) {
		printk("Bad Kmalloc tok\n");
		up(&(my_devp->mutex));												// CRTICAL REGION ENDS
		return -ENOMEM;
	}
	printk("My_driver_read %s()\n", my_devp->name);

	ret = read_cb(my_devp->cb, tok);										// DEQUEUE the circular buffer
	if(ret == -1){															// ret = -1 means buffer is empty
		printk("Buffer is empty\n");
		up(&(my_devp->mutex));												// CRTICAL REGION ENDS
	}
	else{																		// else dequeue successful, --
		printk("token with id=%d removed from buffer index=%d successfully\n", tok->id, ret );	// 	--now add TS3 to the received token

		counter_val = read_counter();												// get counter from hrt-driver
		printk("adding ts3 = counter_val = %u\n", counter_val);
		tok->ts3 = counter_val;

		if (  copy_to_user(buf, tok, sizeof(token))  ) {							// send dequeued token to user app
			up(&(my_devp->mutex));												// CRTICAL REGION ENDS
			return -EFAULT;
		}
	}
	up(&(my_devp->mutex));												// CRTICAL REGION ENDS
	return ret;																		// return -1 if unsuccessful or else dequeued index
}



/*
 * File operations structure. Defined in linux/fs.h
 */
static struct file_operations My_fops = {
		.owner = THIS_MODULE,           	/* Owner */
		.open = My_driver_open,              /* Open method */
		.release = My_driver_release,        /* Release method */
		.write = My_driver_write,            /* Write method */
		.read = My_driver_read				/* Read method */

};


/*
 * Driver Initialization
 */
int __init My_driver_init(void)
{
	int ret;
	printk("My_driver_init squeue ()\n");

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&my_dev_number, 0, 2, DEVICE_NAME) < 0) {		//	firstminor = 1 and count = 2
		printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	printk(" squeue  My major number = %d\n", MAJOR(my_dev_number));

	/* Populate sysfs entries */
	my_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structuremy_devp1 */
	my_devp1 = kmalloc(sizeof(struct My_dev), GFP_KERNEL);
	if (!my_devp1) {
		printk("Bad Kmalloc my_devp1\n"); return -ENOMEM;
	}
	/* Allocate memory for the per-device structure my_devp2*/
	my_devp2 = kmalloc(sizeof(struct My_dev), GFP_KERNEL);
	if (!my_devp2) {
		printk("Bad Kmalloc my_devp2\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(my_devp1->name, DEVICE_NAME1);
	sprintf(my_devp2->name, DEVICE_NAME2);

	/* Connect the file operations with the cdev */
	cdev_init(&my_devp1->cdev, &My_fops);
	cdev_init(&my_devp2->cdev, &My_fops);
	my_devp1->cdev.owner = THIS_MODULE;
	my_devp2->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&my_devp1->cdev, MKDEV(MAJOR(my_dev_number), 0), 1);
	if (ret) {
		printk("Bad cdev 1\n");
		return ret;
	}
	ret = cdev_add(&my_devp2->cdev, MKDEV(MAJOR(my_dev_number), 1), 1);
	if (ret) {
		printk("Bad cdev 2\n");
		return ret;
	}

	device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), 0), NULL,"squeue1");
	device_create(my_dev_class, NULL, MKDEV(MAJOR(my_dev_number), 1), NULL,"squeue2");

	init_cb(&(my_devp1->cb), SQUEUE_LENGTH);
	init_cb(&(my_devp2->cb), SQUEUE_LENGTH);
	printk("circular buffer initialized\n");

	sema_init(&(my_devp1->mutex),1);													// Initialize the semaphore with 1 to be a mutex.
	sema_init(&(my_devp2->mutex),1);													// Initialize the semaphore with 1 to be a mutex.
	printk("Semaphores initialized\n");

	printk("My Driver = %s Initialized.\n", DEVICE_NAME);

	return 0;
}



/*
 * Driver Exit
 */
void __exit My_driver_exit(void)
{
	int minor;
	printk("My_driver_exit squeue ()\n");

	free_cb(my_devp1->cb);
	free_cb(my_devp2->cb);
	printk("free_cb returns\n");

	/* Release the major number */
	unregister_chrdev_region((my_dev_number), 2);

	/* Destroy device */
	for (minor = 0; minor <= 1; minor++){
		device_destroy (my_dev_class, MKDEV(MAJOR(my_dev_number), minor));
	}

	cdev_del(&my_devp1->cdev);
	cdev_del(&my_devp2->cdev);
	kfree(my_devp1);
	kfree(my_devp2);

	/* Destroy driver_class */
	class_destroy(my_dev_class);

	printk("My Driver squeue  exits.\n");
}



module_init(My_driver_init);
module_exit(My_driver_exit);
MODULE_LICENSE("GPL v2");
