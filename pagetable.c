#include <assert.h>
#include <string.h> 
#include "sim.h"
#include "pagetable.h"

// The top-level page table (also known as the 'page directory')
pgdir_entry_t pgdir[PTRS_PER_PGDIR]; 

// Counters for various events.
// Your code must increment these when the related events occur.
int hit_count = 0;
int miss_count = 0;
int ref_count = 0;
int evict_clean_count = 0;
int evict_dirty_count = 0;

/*
 * Allocates a frame to be used for the virtual page represented by p.
 * If all frames are in use, calls the replacement algorithm's evict_fcn to
 * select a victim frame.  Writes victim to swap if needed, and updates 
 * pagetable entry for victim to indicate that virtual page is no longer in
 * (simulated) physical memory.
 *
 * Counters for evictions should be updated appropriately in this function.
 */
int allocate_frame(pgtbl_entry_t *p) {
	int i;
	int frame = -1;
	for(i = 0; i < memsize; i++) {
		if(!coremap[i].in_use) {
			frame = i;
			break;
		}
	}
	if(frame == -1) { // Didn't find a free page.

		frame = evict_fcn(); // Call replacement algorithm's evict function to select victim

		if (coremap[frame].pte->frame & PG_DIRTY) { //dirty pages need to be written to swap file on page out
			int new_offset;
			new_offset = swap_pageout(frame, coremap[frame].pte->swap_off);
			assert(new_offset != INVALID_SWAP); //ensure swap was successful
			
			coremap[frame].pte->swap_off = new_offset; //need to update the offset in swap file for victim
			coremap[frame].pte->frame = coremap[frame].pte->frame & ~PG_DIRTY; //set dirty bit off
			
			evict_dirty_count++;
		}
		else { //dirty bit is 0
			evict_clean_count++;
		}
		
		coremap[frame].pte->frame = coremap[frame].pte->frame | PG_ONSWAP; //set onswap bit of victim page to on
		coremap[frame].pte->frame = coremap[frame].pte->frame & ~PG_VALID;//set valid bit of victim page to off
		
	}
	

	coremap[frame].in_use = 1; // Record information for virtual page that will now be stored in frame
	coremap[frame].pte = p; // Record information for virtual page that will now be stored in frame

	return frame;
}

/*
 * Initializes the top-level pagetable.
 * This function is invoked once at the start of the simulation.
 * For the simulation, there is a single "process" whose reference trace is 
 * being simulated, so there is just one top-level page table (page directory).
 * To keep things simple, we use a global array of 'page directory entries'.
 *
 * In a real OS, each process would have its own page directory, which would
 * need to be allocated and initialized as part of process creation.
 */
void init_pagetable() {
	int i;

	for (i=0; i < PTRS_PER_PGDIR; i++) {	// Set all entries in top-level pagetable to 0, which ensures valid bits are all 0 initially.
		pgdir[i].pde = 0;
	}
}


pgdir_entry_t init_second_level() { // For simulation, we get second-level pagetables from ordinary memory
	int i;
	pgdir_entry_t new_entry;
	pgtbl_entry_t *pgtbl;

	if (posix_memalign((void **)&pgtbl, PAGE_SIZE, // Allocating aligned memory ensures the low bits in the pointer must be zero, so we can use them to store our status bits, like PG_VALID
			   PTRS_PER_PGTBL*sizeof(pgtbl_entry_t)) != 0) {
		perror("Failed to allocate aligned memory for page table");
		exit(1);
	}

	
	for (i=0; i < PTRS_PER_PGTBL; i++) { // Initialize all entries in second-level pagetable
		pgtbl[i].frame = 0; // sets all bits, including valid, to zero
		pgtbl[i].swap_off = INVALID_SWAP;
	}

	new_entry.pde = (uintptr_t)pgtbl | PG_VALID; // Mark the new page directory entry as valid

	return new_entry;
}

/* 
 * Initializes the content of a (simulated) physical memory frame when it 
 * is first allocated for some virtual address.  Just like in a real OS,
 * we fill the frame with zero's to prevent leaking information across
 * pages. 
 * 
 * In our simulation, we also store the the virtual address itself in the 
 * page frame to help with error checking.
 *
 */
void init_frame(int frame, addr_t vaddr) {
	
	char *mem_ptr = &physmem[frame*SIMPAGESIZE]; // Calculate pointer to start of frame in (simulated) physical memory
	
    addr_t *vaddr_ptr = (addr_t *)(mem_ptr + sizeof(int)); // Calculate pointer to location in page where we keep the vaddr
	
	memset(mem_ptr, 0, SIMPAGESIZE); // zero-fill the frame
	*vaddr_ptr = vaddr;             // record the vaddr for error checking

	return;
}

/*
 * Locate the physical frame number for the given vaddr using the page table.
 *
 * If the entry is invalid and not in swap file, then this is the first reference 
 * to the page and a (simulated) physical frame should be allocated and 
 * initialized (using init_frame).  
 *
 * If the entry is invalid and in swap file, then a (simulated) physical frame
 * should be allocated and filled by reading the page data from swap.
 *
 * Counters for hit, miss and reference events should be incremented in
 * this function.
 */
char *find_physpage(addr_t vaddr, char type) {
	pgtbl_entry_t *p = NULL; // pointer to the full page table entry for vaddr
	unsigned idx = PGDIR_INDEX(vaddr); // get index into page directory

	if (pgdir[idx].pde == 0) { //have to initialize page table
		pgdir[idx] = init_second_level();
	}

	pgtbl_entry_t* page_table = (pgtbl_entry_t*) (pgdir[idx].pde & PAGE_MASK); //pde masked with PAGE_MASK gives appropriate page table address
	
	p = &page_table[PGTBL_INDEX(vaddr)]; //use information about the page table index stored in vaddr
	
	if (p->frame & PG_VALID) { //check if p is valid
		hit_count++; //p is in physical memory, so it's considered a hit
	}
	else { //p is invalid 
		if (p->frame & PG_ONSWAP) { //p is invalid and in swap file
			p->frame = allocate_frame(p) << PAGE_SHIFT; //frees up a frame by either looking for an open one or evicting for one
			
			int swap_in_return;
			swap_in_return = swap_pagein(p->frame >> PAGE_SHIFT, p->swap_off); //read data about p from swap file into physical memory at frame position
			assert(swap_in_return == 0); //make sure return value of swap_pagein is okay

			p->frame = p->frame | PG_VALID; //mark p as valid since it's now located in physical memory
			p->frame = p->frame & ~PG_DIRTY; //p is not dirty since it was just swapped in
			miss_count++; //p wasn't in physical memory, so it's considered a miss		
		}
		else { //p is invalid and not in swap file
			p->frame = allocate_frame(p) << PAGE_SHIFT; //frees up a frame by either looking for an open one or evicting for one

			init_frame(p->frame >> PAGE_SHIFT, vaddr); //fill up physical memory at frame position with data about vaddr

			p->frame = p->frame | PG_VALID; //mark p as valid since it's now located in physical memory
			p->frame = p->frame | PG_DIRTY; //mark p as dirty so that it can be added to the swap file when (if) evicted
			miss_count++; //p wasn't in physical memory, so it's considered a miss
		}
	}
	
	p->frame = p->frame | PG_VALID; // make sure that p is marked valid 
	p->frame = p->frame | PG_REF; //make sure that p is marked as referenced
	
	assert(p->frame & PG_VALID);
	assert(p->frame & PG_REF);

	if (type == 'S' || type == 'M') {
		p->frame = p->frame | PG_DIRTY; //mark p dirty if access type indicates that the page will be written to
	}
	
	ref_count++; //ref_count should be incremented for every find_physpage call
	
	ref_fcn(p);	// Call replacement algorithm's ref_fcn for this page

	return  &physmem[(p->frame >> PAGE_SHIFT)*SIMPAGESIZE]; // Return pointer into (simulated) physical memory at start of frame
}

void print_pagetbl(pgtbl_entry_t *pgtbl) {
	int i;
	int first_invalid, last_invalid;
	first_invalid = last_invalid = -1;

	for (i=0; i < PTRS_PER_PGTBL; i++) {
		if (!(pgtbl[i].frame & PG_VALID) && 
		    !(pgtbl[i].frame & PG_ONSWAP)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("\t[%d] - [%d]: INVALID\n",
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			printf("\t[%d]: ",i);
			if (pgtbl[i].frame & PG_VALID) {
				printf("VALID, ");
				if (pgtbl[i].frame & PG_DIRTY) {
					printf("DIRTY, ");
				}
				printf("in frame %d\n",pgtbl[i].frame >> PAGE_SHIFT);
			} else {
				assert(pgtbl[i].frame & PG_ONSWAP);
				printf("ONSWAP, at offset %lu\n",pgtbl[i].swap_off);
			}			
		}
	}
	if (first_invalid != -1) {
		printf("\t[%d] - [%d]: INVALID\n", first_invalid, last_invalid);
		first_invalid = last_invalid = -1;
	}
}

void print_pagedirectory() {
	int i; // index into pgdir
	int first_invalid,last_invalid;
	first_invalid = last_invalid = -1;

	pgtbl_entry_t *pgtbl;

	for (i=0; i < PTRS_PER_PGDIR; i++) {
		if (!(pgdir[i].pde & PG_VALID)) {
			if (first_invalid == -1) {
				first_invalid = i;
			}
			last_invalid = i;
		} else {
			if (first_invalid != -1) {
				printf("[%d]: INVALID\n  to\n[%d]: INVALID\n", 
				       first_invalid, last_invalid);
				first_invalid = last_invalid = -1;
			}
			pgtbl = (pgtbl_entry_t *)(pgdir[i].pde & PAGE_MASK);
			printf("[%d]: %p\n",i, pgtbl);
			print_pagetbl(pgtbl);
		}
	}
}
