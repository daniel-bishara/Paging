#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"

extern unsigned memsize;

extern int debug;

extern struct frame *coremap;


typedef struct node { // linked list data structure
    unsigned long val;
    struct node* next;
} node_t;

node_t * head;
node_t * temp_node;

int buffsize = 200;


/* Page to evict is chosen using the OPT (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	
	int furthest = 0;
	int furthestindex = 0;
	int x; //for each index in coremap
	int count;
	for ( x = 0; x < memsize; x++){
		count = 0;
		temp_node = head;
		while (temp_node != NULL) { //go through all the future addresses until we find one that corresponds to coremap[x]
			if (coremap[x].addr == temp_node->val ){
				if ( count > furthest) //if this coremap[x] is furthest in the future so far
				{
					furthest = count;
					furthestindex = x;
				}
				break; 				
			}

			if (temp_node->next==NULL){
				
				return x; //coremap[x] will never appear again in the rest of the tracefile so we can return it
			}
			
			count++;
			temp_node = temp_node->next;
		}
	}

	return furthestindex;
}

/* This function is invoked on each access to a page to 
 * update any information needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	coremap[p->frame >> PAGE_SHIFT].addr = head->val; //initializing address
	temp_node = head; //store current head so that we can free the memory
	head = head->next;
	free(temp_node); //free the old head corresponding to *p
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
 void opt_init() {

	head = malloc(sizeof(node_t)); //initialize head node

	FILE *filept = fopen(tracefile, "r");

	char buffer[buffsize];
	
	temp_node = head ;
	char c; //used when extracting lines from tracefile


	while (fgets(buffer, buffsize, filept) != NULL ) {  //read one line
		sscanf(buffer, "%c %lx", &c, &temp_node->val); //store address for this line in linked list node
		temp_node->next = (node_t *) malloc(sizeof(node_t)); //create new node in linked list for this line
		temp_node = temp_node->next;
		temp_node->next = NULL;
	}
	
	temp_node = head;

	

	
	
	
}