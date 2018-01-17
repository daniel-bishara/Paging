#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

int hand_index;

/* Page to evict is chosen using the CLOCK algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int clock_evict() {
	while (1){

		if(coremap[hand_index].last_accessed == 0){ 
			return hand_index; //exits loop and returns the evicted index
		}
		
		coremap[hand_index].last_accessed--;; //decrement value of index
		hand_index++; //hand points at next index
		
		if (hand_index == memsize){ //checks if we already reached the last index of the clock
			hand_index = 0;  //start at the beginning of the clock again
		}
	}
	return -1; // This line will never execute
}

/* This function is invoked on each access to a page to update 
 * any information needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	coremap[p->frame >> PAGE_SHIFT].last_accessed = 1; //resets the last accessed attribute
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	hand_index = 0;//initializes hand to point at first index
	int i;
	for (i = 0 ; i < memsize ; i++){
		coremap[i].last_accessed = 1; //initializes every value to 1
	}
	
}
