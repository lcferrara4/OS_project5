// Lauren Ferrara and Emily Obaditch

/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

int NFAULTS = 0;
int NREADS = 0;
int NWRITES = 0;

int *FRAME_TABLE; // page number if full, -1 for empty
int FREE_FRAMES;

// for fifo
int *FIFO_QUEUE;
int NQUEUE = 0;

// for custom - keeps track of use bits
int *USE_BITS;

struct disk *disk;

char* algorithm;

// methods for fifo queue
void queue_push(int p){
	int i = 0;
	while( FIFO_QUEUE[i] != -1 )
		i++;

	FIFO_QUEUE[i] = p;
	NQUEUE++;
}

// pop first element and move offer remaining
int queue_pop(int n){
	int popped = FIFO_QUEUE[0];

	// move data over
	int i = 0;
	while( FIFO_QUEUE[i] != -1 && i < n-1 ){
		FIFO_QUEUE[i] = FIFO_QUEUE[i+1];
		i++;
	}
	FIFO_QUEUE[n-1] = -1;
	NQUEUE--;
	
	return popped;
}

// get next free frame if no replacement algorithm necessary
int find_free_frame(){
	int i = 0;
	while( FRAME_TABLE[i] != -1 ){
		i++;
	}
	return i;
}

void page_fault_handler( struct page_table *pt, int page)
{
	NFAULTS++;

	printf("page fault on page #%d\n",page);

	char *physmem = page_table_get_physmem(pt);
	int curr_frame = -1;
	int* curr_frame_ptr = &curr_frame;
	int curr_bit = -1;
	int* curr_bit_ptr = &curr_bit;

	page_table_get_entry(pt, page, curr_frame_ptr, curr_bit_ptr);

	// In physical memory but needs write bit
	if( *curr_bit_ptr ){	
		page_table_set_entry(pt, page, *curr_frame_ptr, PROT_READ|PROT_WRITE);

	} else if( FREE_FRAMES ){ // Fill a free frame
		int frame = find_free_frame();
		FREE_FRAMES--;
		FRAME_TABLE[frame] = page;
		page_table_set_entry(pt, page, frame, PROT_READ);
		disk_read(disk, page, &physmem[frame * PAGE_SIZE]);
		NREADS++;

		// add to queue if fifo
		if( !strcmp(algorithm, "fifo") ){
			queue_push(page);
		}

	} else{ // Do a replacement algorithm
		int page_num;
		if( !strcmp(algorithm, "rand") ){
			page_num = FRAME_TABLE[ rand() % page_table_get_nframes(pt) ];
		} else if( !strcmp(algorithm, "fifo") ){
			page_num = queue_pop(page_table_get_npages(pt));
			queue_push(page);
		} else if ( !strcmp(algorithm, "custom") ){ // custom
			page_num  = rand() % page_table_get_npages(pt); // start at random page
			while( USE_BITS[page_num] || page_num == page ){
				USE_BITS[page_num] = 0;
				page_num++;
				if (page_num == page_table_get_npages(pt))
					page_num = 0;
			}
		}

		// handle what is replaced
		page_table_get_entry(pt, page_num, curr_frame_ptr, curr_bit_ptr);
		if( *curr_bit_ptr == (PROT_READ|PROT_WRITE) ){ // dirty
			disk_write(disk, page_num, &physmem[*curr_frame_ptr * PAGE_SIZE] );
			NWRITES++;
		}
	
		// update values	
		disk_read(disk, page, &physmem[*curr_frame_ptr * PAGE_SIZE]);
		NREADS++;
	
		FRAME_TABLE[*curr_frame_ptr] = page;
		page_table_set_entry(pt, page, *curr_frame_ptr, PROT_READ);
		page_table_set_entry(pt, page_num, *curr_frame_ptr, 0);
	} 
	
	if ( !strcmp(algorithm, "custom") ){	
		USE_BITS[page] = 1;
	}

	page_table_print(pt);
}

int main( int argc, char *argv[] )
{
	srand(time(NULL));

	// check and store command line arguments
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	if( npages == 0 ){
		printf("Please enter a valid number for npages.\n");
		return 1;
	}

	int nframes = atoi(argv[2]);
	if( nframes == 0 ){
		printf("Please enter a valid number for nframes.\n");
		return 1;
	}

	algorithm = argv[3];
	if( strcmp(argv[3], "rand") && strcmp(argv[3], "fifo") && strcmp(argv[3], "custom") ){
		printf("Must choose rand|fifo|custom for replacement algorithm.\n");
		return 1;
	}

	const char *program = argv[4]; // error check implemented below

	// initialize frame table
	FRAME_TABLE = (int *)malloc(sizeof(int) * nframes);
	FREE_FRAMES = nframes;
	int i;
	for( i = 0; i < nframes; i++ ){
		FRAME_TABLE[i] = -1; // starts empty
	}

	// initialize fifo queue if fifo chosen
	if( !strcmp(argv[3], "fifo") ){
		FIFO_QUEUE = (int *)malloc(sizeof(int) * npages);
        	int i;
        	for( i = 0; i < npages; i++ ){
                	FIFO_QUEUE[i] = -1; // starts empty
        	}
	}

        // initialize use-bit array if custom chosen
        if( !strcmp(argv[3], "custom") ){
        	USE_BITS = (int *)malloc(sizeof(int) * npages);
                for( i = 0; i < npages; i++ ){
                        USE_BITS[i] = 0; // starts initialized to 0s
                }
	}
	
	// Set up virtual disk space	
	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	// Set up page table
	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	// Check which program to run
	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);
		return 1;
	}

	// Clean up
	page_table_delete(pt);
	disk_close(disk);
	free(FRAME_TABLE);
	free(FIFO_QUEUE);
	free(USE_BITS);

	// Print results
	printf("Number of page faults: %d\n", NFAULTS);
	printf("Number of disk reads: %d\n", NREADS);
	printf("Number of disk writes: %d\n", NWRITES);

	return 0;
}
