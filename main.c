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
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//global variables
int method;
int npages;
int nframes;
char *virtmem;
char *physmem;
int *frameTable;
int *writeTable;
struct disk *disk;
int numFramesUsed =0;
int diskWrites = 0;
int diskReads = 0;
int pagefaults = 0;
int *queue;
int evictFrame, evictBits;


int popQueue(){
	int front = queue[0];
	int i;
	for(i=1; i<nframes; i++){
		int temp = queue[i];
		queue[i-1] = temp;
	}
	queue[nframes-1] = -1;
	return front;
}

int pushQueue(int val){
	int i;
	for(i=0; i < nframes; i++){
		if(queue[i] == -1){
			queue[i] = val;
			return 1;
		}
	}
	return 0;
}

int checkWrite(struct page_table *pt)
{
	int i;
	int bits;
	int frame;
	int r = rand()%nframes;
	int count=0;
	for(i=0;i<nframes;i++)
	{
		page_table_get_entry(pt,frameTable[i],&frame,&bits);
		if(bits<3)
		{
			count++;
		}
	}
	printf("# of write-free frames: %d\n",count);
	if(count==1)
	{
		return r;
	}
	else
	{
		for(i=0;i<nframes;i++)
		{
			page_table_get_entry(pt,frameTable[r],&frame,&bits);
			if(bits<3)
			{
				printf("Returning frame %d\n",r);
				return r;
			}
			r = (r+1)%nframes;
		}
	}
	return rand()%nframes;

}
int findFrame(){
	int i;
	for(i=0; i<nframes; i++){
		if (frameTable[i] ==-1){
			return i;
		}
	}
	return -1;
}




void page_fault_handler( struct page_table *pt, int page )
{
/*
	int bits
	int fn
	getpage entry store into bits and fn
	bits will be 0,1, 3,5 ,7 or whatever for chmod to see permissions
	fn will be what frame the data is in
*/
	int bits;
	int frame;
	page_table_get_entry(pt, page, &frame, &bits);
	printf("page: %d, frame: %d\n",page,frame);
	switch(bits){
		case PROT_READ:
			page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
//			disk_read(disk,page,&physmem[frame*PAGE_SIZE]);
			printf("adding write permission\n");
//			diskReads++;
//			pagefaults++;
			break;
		case PROT_READ|PROT_WRITE:
			page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE|PROT_EXEC);
//			disk_read(disk,page,&physmem[frame*PAGE_SIZE]);
			printf("adding exec permission\n");
//			diskReads++;
//			pagefaults++;
			break;
		case 0:
			pagefaults++;
			printf("not in frame\n");
			if(numFramesUsed == nframes){
				//rand fifo custom
				//rand
				if(method ==1){
					evictFrame = rand()%nframes;
				//fifo
				}else if(method==2){
					evictFrame = popQueue();
				//custom
				}else if(method==3){
					evictFrame = rand()%nframes;
					page_table_get_entry(pt, frameTable[evictFrame],&evictFrame,&evictBits);
					if(evictBits >=3)
					{
						printf("Finding new frame with checkWrite\n");
						evictFrame = checkWrite(pt);
					}

				}
				printf("evicting page %d, frame: %d\n",frameTable[evictFrame], evictFrame);
				page_table_get_entry(pt, frameTable[evictFrame], &evictFrame, &evictBits);
				//write if bits>=3
				if(evictBits>=3){
					disk_write(disk,frameTable[evictFrame], &physmem[evictFrame*PAGE_SIZE]);
					printf("writing page %d to disk\n", frameTable[evictFrame]);
					diskWrites++;
				}
				//set entry
				page_table_set_entry(pt,frameTable[evictFrame], 0, 0);
				frameTable[evictFrame] =-1;
				numFramesUsed--;
				frame = evictFrame;
			} else { //find any open frame
				frame = findFrame();
				printf("finding open frame: %d\n", frame);
			}
			page_table_set_entry(pt,page,frame,PROT_READ);
			disk_read(disk,page,&physmem[frame*PAGE_SIZE]);
			if (method==2){ //make sure queue isnt full
				int rc =pushQueue(frame);
				if (rc==0) printf("error: queue is full\n");
			}
			diskReads++;
			frameTable[frame] = page;
			numFramesUsed++;
			break;
		default:
			break;
	}
	printf("=====================\n");

//	printf("page fault on page #%d\n",page);
//	exit(1);
}

int main( int argc, char *argv[] )
{
	time_t t;
	srand((unsigned) time(&t));

	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|lru|custom> <sort|scan|focus>\n");
		return 1;
	}

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	frameTable = (int*) malloc(nframes*sizeof(int));
	queue = (int*) malloc(nframes*sizeof(int));
	int i;
	for(i=0; i<nframes; i++){
		frameTable[i] = -1;
		queue[i] = -1;
	}
	const char *methodInput = argv[3];
	const char *program = argv[4];
	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	if(!strcmp(methodInput, "rand")){
		method = 1;
	} else if(!strcmp(methodInput, "fifo")){
		method = 2;
	} else if(!strcmp(methodInput, "custom")){
		method = 3;
	} else {
		printf("unknown method: %s\n",argv[3]);
		return 1;
	}

	virtmem = page_table_get_virtmem(pt);
	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);
		return 1;
	}

	page_table_delete(pt);
	disk_close(disk);

	printf("==Summary==\n");
	printf("Disk Reads: %d\n", diskReads);
	printf("Disk Writes: %d\n", diskWrites);
	printf("Page Faults: %d\n", pagefaults);



	return 0;
}
