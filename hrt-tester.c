/* hrt-tester application for gmem-driver kernel module
 * Author : Rohit Khanna
 *
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "hrt-ioctl.h"

/* Function Declarations 	*/
void start_timer(int fd);
void stop_timer(int fd);
void set_timer(int fd, unsigned int timer_val);



int main(int argc, char **argv)
{
	int fd1;
	int res;
	unsigned int counter_value;							// To read the timer value from driver
	int ret=0;

	if(argc < 2 || argc > 3){							// can only be 2 to 3
		ret = -1;
		goto USAGE;
	}

	/* open device */
	fd1 = open("/dev/hrt", O_RDWR);
	if ( fd1 < 0 ){
		printf("Can not open device file.\n");
		return 0;
	}
	printf("%s: open: successful\n", argv[0]);

	if ( (strcmp(argv[1], "read") == 0) ){									/* 	 Issue a read 	*/
		printf("issuing a read()\n");
		res = read(fd1, &counter_value, sizeof(counter_value));
		if ( res == -1 ) {
			printf("read failed");
			close(fd1);
			exit(-1);
		}
		printf("--------read returns with counter_value = %u and res = %u ----------\n", counter_value, res);
	}
	else if (strcmp(argv[1], "start") == 0){						//call start_timer()
		start_timer(fd1);
	}
	else if(strcmp(argv[1], "stop") == 0){							//call stop_timer()
		stop_timer(fd1);
	}
	else if(strcmp(argv[1], "set") == 0){							//call stop_timer()
		if(argc != 3){
			ret = -1;
			goto USAGE;
		}
		else{
			set_timer(fd1, (unsigned int)atoi(argv[2]));			//pass the third argc which is timer val to be set
		}
	}
	else{
		ret = -1;
		goto USAGE;
	}

	USAGE:														// Label to handle erroneous run args
	if(ret == -1)
		printf("\nUSAGE : \n"
				" %s \n"
				"\t read -- To read the counter value of our HRT\n"
				"\t start -- To start the HRT, through ioctl\n"
				"\t stop -- To stop the HRT, through ioctl\n"
				"\t set val -- To set the HRT counter to val, through ioctl\n\n", argv[0]);


	printf("closing file descriptor..\n");
	close(fd1);
	printf("file descriptor closed..\n");
	printf("PROGRAM ENDS...\n");

	return 0;
}


void start_timer(int fd){
	if (ioctl(fd, QUERY_START_TIMER ) == -1){
		printf("QUERY_START_TIMER ioctl error");
	}
	printf("start_timer ioctl returns successfully \n");
}

void stop_timer(int fd){
	if (ioctl(fd, QUERY_STOP_TIMER ) == -1)	{
		printf("QUERY_STOP_TIMER ioctl error\n");
	}
	printf("stop_timer ioctl returns successfully\n");
}

void set_timer(int fd, unsigned int timer_val){
	printf("set_timer() to time_val = %d\n", timer_val);
	if (ioctl(fd, QUERY_SET_TIMER, &timer_val ) == -1){
		printf("QUERY_SET_TIMER ioctl error\n");
	}
	printf("set_timer ioctl returns successfully\n");
}
