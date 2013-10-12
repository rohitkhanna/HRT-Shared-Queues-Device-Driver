/*
 *  cb.h
 *	Description:-
 *				- Circular buffer implementation,
 *				- keeps one extra slot - to distinguish between the two cases - ie when buffer if empty and when buffer is full.
 *				- when queue is full - (end+1) % size = start
 *				- when queue is empty - end = start
 *
 *  Created on: Oct 9, 2013
 *      Author: rohit
 */


/*	define squeue length	*/
#define SQUEUE_LENGTH 10

/*		Token struct	*/
typedef struct{
	int 			id;
	unsigned int 	ts1;
	unsigned int 	ts2;
	unsigned int 	ts3;
	unsigned int 	ts4;
	char 			string[80];
}token;


/* Circular buffer struct	*/
typedef struct{
    int         size;   		/* maximum number of elements       		    */
    int         start;  		/* index of oldest element              		*/
    int         end;    		/* index at which to write the new/next element */
    token  	    *elems; 		/* vector of elements                   		*/
}circular_buffer;


/*
 * 	Initialize the circular buffer
 */
void init_cb(circular_buffer **cb, int size) {
	printk("init_cb()\n");

	(*cb) = kmalloc(sizeof(circular_buffer), GFP_KERNEL);
    (*cb)->size  = size + 1; 										/* include extra slot	*/
    (*cb)->start = 0;
    (*cb)->end   = 0;
    (*cb)->elems = kmalloc( ((*cb)->size)*sizeof(token), GFP_KERNEL );
}


/*
 * Free the circular buffer memory
 */
void free_cb(circular_buffer *cb) {
	if(!cb){
		printk("cb is NULL !!\n");
		return;
	}
    kfree(cb->elems);
}


/*
 * Check if circular buffer is full
 * - returns 1 if true or 0 if false
 */
int is_cb_full(circular_buffer *cb) {
    return (cb->end + 1) % cb->size == cb->start;
}


/*
 * check if circular buffer is empty
 * - returns 1 if true or 0 if false
 */
int is_cb_empty(circular_buffer *cb) {
    return cb->end == cb->start;
}


/*
 * returns -1 if cb is full
 * or else
 * write an element to the circular buffer ie. at end and return the index of where element was added
 */
int write_cb(circular_buffer *cb, token *tok) {
	int ret = -1;
	if(is_cb_full(cb)){
		return -1;
	}

    cb->elems[cb->end] = *tok;
    ret = cb->end;
    cb->end = (cb->end + 1) % cb->size;
    return ret;

}


/*
 * return -1 if circular buffer is empty
 * or else
 * read the oldest element from start and return it by setting tok.
 */

int read_cb(circular_buffer *cb, token *tok) {
	int ret;
	if(is_cb_empty(cb)){
		return -1;
	}

    *tok = cb->elems[cb->start];
    ret = cb->start;
    cb->start = (cb->start + 1) % cb->size;
    return ret;
}



