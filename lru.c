#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	int lowest_index = 0; 
	int i;
	for (i = 0; i < memsize; i++){
		if (coremap[i].last_accessed < coremap[lowest_index].last_accessed){
			lowest_index = i; // current index with lowest last acessed attribute
		}
	}
	return lowest_index; //return index of lowest last accessed attribute
}

/* This function is invoked on each access to a page to 
 * update any information needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
 
void lru_ref(pgtbl_entry_t *p) {
	
	coremap[p->frame >> PAGE_SHIFT].last_accessed = 0;  //reset the last_accessed attribute of the page being accessed
	int i;
	for (i = 0; i < memsize; i++) {
		coremap[i].last_accessed--; //decrement all pages last acessed by 1 (including the one we just accessed)
	}
	
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	int i;
	for (i = 0; i < memsize; i++){
		coremap[i].last_accessed = 0; //initialize all the pages last accessed attribute to 0
	}
}
