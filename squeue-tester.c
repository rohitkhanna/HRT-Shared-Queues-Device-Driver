/*
 * squeue-tester.c
 *
 *  Created on: Oct 10, 2013
 *      Author: rohit
 */


#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<pthread.h>
#include <string.h>
#include <sys/ioctl.h>

/*	IOCTL VARIABLES	*/
#define IOCTL_APP_TYPE	99							/* arbitrary number unique to the app 	*/
#define QUERY_START_TIMER _IO(IOCTL_APP_TYPE, 1)
#define QUERY_STOP_TIMER _IO(IOCTL_APP_TYPE, 2)
#define QUERY_SET_TIMER _IOW(IOCTL_APP_TYPE, 3, unsigned int*)


/*		Token struct	*/
typedef struct{
	int 			id;					// The timestamps in a token are
	unsigned int 	ts1;				// TS just before writing the token to the device
	unsigned int 	ts2;				// TS at the instants that the token is enqueued
	unsigned int 	ts3;				// TS at the instants that the token is dequeued
	unsigned int 	ts4;				// TS immediately after receiving the token of a read device call.
	char 			string[80];			// string of each token can consist of arbitrary characters.
}token;

unsigned int TOKEN_SEQUENCE_NO=1;		// Global sequence number for token ids
unsigned int STR_MAX_LEN=80;			// Max token string length
unsigned int NUM_OF_SENDERS=6;			// Max number of senders

typedef struct thread_data{
	int threadId;									// threadId of the thread
	//int tokenId;											// tokenId set in main()
	int fd_hrt;												// both receiver and sender use this to get timer counter value
	int fd_squeue;											// used by sender thread to write data to either squeue1 or squeue2 as set by main()
	int fd_squeue1;											// receiver thread needs both fd_squeue1 and fd_squeue2
	int fd_squeue2;											// receiver thread needs both fd_squeue1 and fd_squeue2

}thread_data;


pthread_mutex_t mutex;						// mutex for protecting TOKEN_SEQUENCE_NO

/* Function Declarations	*/
int read_hrt_counter(int fd);
void *sender_thread_function(void* data);
int write_to_squeue(int fd_squeue, token *tok );
char* getRandomString(unsigned int max_str_len);



/*
 * Sender thread function
 */
void *sender_thread_function(void* data){

	int i;
	int sleep_interval;
	char random_str[70];

	for(i=1; i<=100; i++){													// Send 100 tokens
		int res;
		thread_data *tdata = (thread_data*)data;
		//printf("*****************************************threadID = %d***************************************\n", tdata->threadId);

		token *tok = calloc(1, sizeof(token));

		pthread_mutex_lock(&mutex);
		tok->id = (TOKEN_SEQUENCE_NO)++;									//protect the shared variable TOKEN_SEQUENCE_NO
		pthread_mutex_unlock (&mutex);

		//read HRT counter value
		int counter_value = 0;
		if( (counter_value=read_hrt_counter(tdata->fd_hrt)) == -1 ){
			printf("read failed while reading hrt counter\n");
			close(tdata->fd_hrt);
			exit(-1);
		}

		//printf("adding ts1 = counter_value = %u to token \n", counter_value);
		tok->ts1 = counter_value;															//fill up token structure with ts1

		strcpy(random_str, getRandomString(STR_MAX_LEN-10));
		//printf("\nrandom_str=%s, len=%d\n", random_str, strlen(random_str));
		sprintf(tok->string, "Sender%d-%s", tdata->threadId, random_str);					//Concatenate threadID with random string to form the final string
		//printf("\nfinal_str=%s, strlen=%d\n", tok->string, strlen(tok->string));			// final_str eg. Sender

		res = -1;
		while(res==-1){

			res = write(tdata->fd_squeue, tok, sizeof(token));		/* Issue a write */
			sleep_interval=(rand()%9000)+1000;
			usleep(sleep_interval);												// sleep for random interval

			if(res == sizeof(token)){
				printf("Can not write to the squeue device file.\n");
				close(tdata->fd_squeue);
				exit(-1);
			}
			else if(res == -1){														//res=-1 means buffer is full, hence retry
				//printf("The circular buffer is full hence will try again\n");
			}
			else {
				//printf("write_to_squeue() succeeded with res=%d\n", res);
			}
		}
	}
	pthread_exit(0);
}



/*
 * Receiver thread function
 */
void *receiver_thread_function(void* data){

	int res;
	int counter_value;
	int sleep_interval;
	thread_data *tdata = (thread_data*)data;
	token *tok = calloc(1, sizeof(token));
	int flag=0;

	//printf("****************************threadID = %d************************************\n", tdata->threadId);

	while(flag!=1){
		while( (res = read(tdata->fd_squeue1, tok, sizeof(token))) !=-1 ){									// Dequeue squeue1 and print tokens
			if(tok->id == 100*NUM_OF_SENDERS)
				flag=1;

			counter_value = 0;
			if( (counter_value=read_hrt_counter(tdata->fd_hrt)) == -1 ){
				printf("read failed while reading hrt counter\n");
				close(tdata->fd_hrt);
				exit(-1);
			}

			tok->ts4 = counter_value;														//fill up token structure with ts4
			printf("\nFinalized Token ==> id=%d, ts1=%u, ts2=%u, ts3=%u, ts4=%u\n", tok->id, tok->ts1, tok->ts2, tok->ts3, tok->ts4);
		}
		if ( res == -1 ) {																	// means circular buffer squeue1 is empty
			printf("-----circular buffer squeue1 is empty-----\n ");
			memset(tok, 0, sizeof(token));
			while( (res = read(tdata->fd_squeue2, tok, sizeof(token))) !=-1 ){				// Dequeue squeue2 and print tokens
				if(tok->id == 100*NUM_OF_SENDERS)
					flag=1;

				counter_value = 0;
				if( (counter_value=read_hrt_counter(tdata->fd_hrt)) == -1 ){
					printf("read failed while reading hrt counter\n");
					close(tdata->fd_hrt);
					exit(-1);
				}
				tok->ts4 = counter_value;													//fill up token structure with ts1
				printf("\nFinalized Token ==> id=%d, ts1=%u, ts2=%u, ts3=%u, ts4=%u\n", tok->id, tok->ts1, tok->ts2, tok->ts3, tok->ts4);
			}
			if(res == -1){																	// squeue2 is empty now, so exit app
				printf("-----circular buffer squeue2 is empty, now take nap-----\n ");
				printf("sleeping for %d us\n", (sleep_interval=(rand()%9000)+1000));
				usleep(sleep_interval);														// has to be random
			}
		}
	}
	pthread_exit(0);
}



/*
 * if fd is the hrt device file name then this function returns the timer counter value
 * else returns -1
 */
int read_hrt_counter(int fd){
	int res;
	unsigned int ctr_value = -1;							// To read the timer value from driver

	res = read(fd, &ctr_value, sizeof(ctr_value));
	return ((res==-1) ? res:ctr_value);
}


/*
 * starts the HRT timer in hrt-driver.c using ioctl command
 */
void start_timer(int fd){
	if (ioctl(fd, QUERY_START_TIMER ) == -1){
		printf("QUERY_START_TIMER ioctl error");
	}
	printf("start_timer ioctl returns successfully \n");
}


/*
 * sets the HRT timer in hrt-driver.c using ioctl command to  a particular value
 */
void set_timer(int fd, unsigned int timer_val){
	printf("set_timer() to time_val = %d\n", timer_val);
	if (ioctl(fd, QUERY_SET_TIMER, &timer_val ) == -1){
		printf("QUERY_SET_TIMER ioctl error\n");
	}
	printf("set_timer ioctl returns successfully\n");
}


/*
 * stops the HRT timer in hrt-driver.c using ioctl command
 */
void stop_timer(int fd){
	if (ioctl(fd, QUERY_STOP_TIMER ) == -1)	{
		printf("QUERY_STOP_TIMER ioctl error\n");
	}
	printf("stop_timer ioctl returns successfully\n");
}


/*
 *  main function which triggers the 6 sender threads and 1 receiver thread
 */
int main(int argc, char **argv){


	int fd_sq1, fd_sq2, fd_hrt;													/* Our file descriptors */
	int i;
	thread_data *tdata_s[NUM_OF_SENDERS];										/* thread_data for the sender threads	*/
	pthread_t thread_id_r, thread_id_s[NUM_OF_SENDERS];							/* threads IDs	*/
	int rc;

	printf("%s: entered\n", argv[0]);
	srand(time(NULL));															/*	setup seed for rand()	*/

	/* open device hrt */
	fd_hrt = open("/dev/hrt", O_RDWR);
	if ( fd_hrt< 0 ){
		printf("Can not open device file squeue1.\n");
		return 0;
	}
	printf("%s: open hrt: successful, fd_hrt=%d\n", argv[0], fd_hrt);

	/*	start hrt timer	*/
	start_timer(fd_hrt);
	printf("Timer has been started !!\n");

	/*	set hrt timer to 0	*/
	set_timer(fd_hrt, 0);
	printf("Timer has been set to 0 !!\n");

	/* open device squeue1 */
	fd_sq1 = open("/dev/squeue1", O_RDWR);
	if ( fd_sq1 < 0 ){
		printf("Can not open device file squeue1.\n");
		return 0;
	}
	printf("%s: open squeue1: successful, fd_sq1=%d\n", argv[0], fd_sq1);

	/* open device squeue2 */
	fd_sq2 = open("/dev/squeue2", O_RDWR);
	if ( fd_sq2 < 0 ){
		printf("Can not open device file squeue2.\n");
		return 0;
	}
	printf("%s: open squeue2: successful, fd_sq2=%d\n", argv[0], fd_sq2);

	/*	 init mutex	*/
	pthread_mutex_init(&mutex, NULL);

	/*	Prepare RECEIVER THREAD data tdata_r and SPAWN	*/
	thread_data *tdata_r;
	tdata_r = malloc(sizeof(thread_data));
	tdata_r->threadId = 200;															// Receiver thread ID = 200
	tdata_r->fd_hrt = fd_hrt;
	tdata_r->fd_squeue1 = fd_sq1;
	tdata_r->fd_squeue2 = fd_sq2;
	printf("In main: creating receiver thread with id = %d ============================>\n", tdata_r->threadId);
	rc = pthread_create(&thread_id_r, NULL, receiver_thread_function, (void*)tdata_r);
	if (rc){
		printf("ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}

	/*	Prepare SENDER THREADS data and SPAWN	*/
	for(i=0;i<NUM_OF_SENDERS;i++){
		tdata_s[i] = malloc(sizeof(thread_data));
		tdata_s[i]->threadId = 100+i;												// Sender thread IDs from 100 to 105
		tdata_s[i]->fd_hrt = fd_hrt;
		if(i%2==0)																	// even threadIds write to squeue2,  odd to squeue1
			tdata_s[i]->fd_squeue =fd_sq2;
		else
			tdata_s[i]->fd_squeue =fd_sq1;

		printf("In main: creating sender thread with id = %d ============================>\n", tdata_s[i]->threadId);
		rc = pthread_create(&thread_id_s[i], NULL, sender_thread_function, (void*)tdata_s[i]);
		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}

	printf("ALL THREADS CREATED\n");
	/*	 BARRIER FOR ALL THREADS TO FINISH		*/
	(void) pthread_join(thread_id_r, NULL);
	(void) pthread_join(thread_id_s[0], NULL);
	printf("ALL THREADS JOIN\n");


	/*	CLOSE THE TIMER	*/
	stop_timer(fd_hrt);
	printf("TIMER CLOSED\n");

	/*	CLOSE THE DEVICES	*/
	printf("CLOSE THE DEVICES..\n");
	close(fd_hrt);
	close(fd_sq1);
	close(fd_sq2);
	printf("\nCLOSING APPLICATION !!!!\n");

	return 0;

}


/*
 * returns a random string with given max_str_len
 */
char* getRandomString(unsigned int max_str_len){

	unsigned int rand_str_len;
	int i;
	char *ran_str;

	rand_str_len = rand() % max_str_len;							// 0 to 69, say max_str_len_max=70
	*ran_str = malloc(sizeof(rand_str_len));

	for (i = 0; i < rand_str_len; i++) {
		ran_str[i] = 'a' + (i % 26);
	}

	ran_str[rand_str_len] = '\0';
	//printf("ran_str=%s, len=%d\n", ran_str, strlen(ran_str));
	return ran_str;

}
