/*
 * hrt-ioctl.h
 *
 *  Created on: Sep 26, 2013
 *      Author: rohit
 */

#ifndef HRT_DRIVER_H_
#define HRT_DRIVER_H_

#include <linux/ioctl.h>

#define IOCTL_APP_TYPE	99							/* arbitrary number unique to the app 	*/
#define QUERY_START_TIMER _IO(IOCTL_APP_TYPE, 1)
#define QUERY_STOP_TIMER _IO(IOCTL_APP_TYPE, 2)
#define QUERY_SET_TIMER _IOW(IOCTL_APP_TYPE, 3, unsigned int*)




#endif /* HRT_DRIVER_H_ */
