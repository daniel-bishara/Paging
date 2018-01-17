#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int curr_index;

/* Page to evict is chosen using the FIFO algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int fifo_evict() {
	
	int temp = curr_index; //store the first in that we will evict
	curr_index++; //the first in (after we evict the old first in) is now at the next index
	
	if (curr_index == memsize){
		curr_index = 0; // first in is at the first index
	}
	return temp;
}

/* This function is invoked on each access to a page to update 
 * any information needed by the fifo algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void fifo_ref(pgtbl_entry_t *p) {
	// We don't need to do anything here for the fifo algorithm
}

/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void fifo_init() {
	curr_index = 0; //initialize the first in index as the first index of table
}
